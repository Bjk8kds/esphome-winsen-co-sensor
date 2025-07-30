#include "ze15_co.h"
#include "esphome/core/log.h"

namespace esphome {
namespace ze15_co {

static const char *const TAG = "ze15_co";

// Q&A mode read command
const uint8_t ZE15COSensor::CMD_READ_DATA[9] = {0xFF, 0x01, 0x86, 0x00, 0x00, 0x00, 0x00, 0x00, 0x79};

void ZE15COSensor::setup() {
  ESP_LOGCONFIG(TAG, "Setting up ZE15-CO...");
  this->buffer_index_ = 0;
  this->waiting_for_response_ = false;
  this->in_qa_mode_ = false;
  this->qa_state_ = QA_IDLE;

  // Clear UART buffer
  while (this->available()) {
    uint8_t dummy;
    this->read_byte(&dummy);
  }

  if (this->mode_ == MODE_QA) {
    ESP_LOGCONFIG(TAG, "ZE15-CO configured for Q&A mode");
    // Send first command to trigger Q&A mode
    this->send_command_(CMD_READ_DATA, 9);
    this->last_qa_command_ = millis();
    this->in_qa_mode_ = true;
    this->qa_state_ = QA_READ_SENT;
    this->qa_timer_ = millis();
  } else {
    ESP_LOGCONFIG(TAG, "ZE15-CO configured for Passive mode");
  }
}

void ZE15COSensor::loop() {
  // Handle mode auto-switching
  if (this->mode_ == MODE_PASSIVE && this->in_qa_mode_) {
    // Check if we should switch back to passive mode (30s timeout)
    if (millis() - this->last_qa_command_ > 30000) {
      ESP_LOGD(TAG, "Auto-switching back to passive mode");
      this->in_qa_mode_ = false;
      this->buffer_index_ = 0;
    }
  }

  if (this->mode_ == MODE_QA || this->in_qa_mode_) {
    // Q&A Mode State Machine
    switch (qa_state_) {
      case QA_IDLE:
        if (millis() - this->last_request_ >= this->update_interval_) {
          ESP_LOGD(TAG, "Q&A: Sending read command");
          // Clear buffer before request
          while (this->available()) {
            uint8_t dummy;
            this->read_byte(&dummy);
          }
          this->send_command_(CMD_READ_DATA, 9);
          this->last_qa_command_ = millis();
          this->qa_timer_ = millis();
          this->qa_state_ = QA_READ_SENT;
          this->buffer_index_ = 0;
        }
        break;

      case QA_READ_SENT:
        // Read available data
        while (this->available() && this->buffer_index_ < 9) {
          uint8_t byte;
          this->read_byte(&byte);
          
          // Sync to start byte
          if (this->buffer_index_ == 0 && byte != 0xFF) {
            continue;  // Skip until we find start byte
          }
          
          this->buffer_[this->buffer_index_++] = byte;
          
          // Check if we have complete packet
          if (this->buffer_index_ == 9) {
            if (this->buffer_[0] == 0xFF && this->buffer_[1] == 0x86) {
              if (this->validate_checksum_(this->buffer_)) {
                this->parse_data_(this->buffer_);
                ESP_LOGD(TAG, "Q&A: Data received successfully");
              } else {
                ESP_LOGW(TAG, "Q&A: Checksum failed");
              }
            } else {
              ESP_LOGW(TAG, "Q&A: Invalid response header");
            }
            this->last_request_ = millis();
            this->qa_state_ = QA_IDLE;
            this->buffer_index_ = 0;
          }
        }
        
        // Timeout check
        if (millis() - this->qa_timer_ >= 3000) {  // 3s timeout
          ESP_LOGW(TAG, "Q&A: Response timeout");
          this->last_request_ = millis();
          this->qa_state_ = QA_IDLE;
          this->buffer_index_ = 0;
        }
        break;
    }
    return;
  }

  // Passive (Initiative) Mode
  while (this->available()) {
    uint8_t byte;
    this->read_byte(&byte);
    
    // Sync to start byte
    if (this->buffer_index_ == 0 && byte != 0xFF) {
      continue;  // Skip until we find start byte
    }
    
    // Check gas type for second byte
    if (this->buffer_index_ == 1 && byte != GAS_TYPE_CO) {
      this->buffer_index_ = 0;  // Reset if not CO sensor data
      continue;
    }
    
    this->buffer_[this->buffer_index_++] = byte;
    
    // Check if we have complete packet
    if (this->buffer_index_ >= 9) {
      // Validate packet
      if (this->buffer_[0] == 0xFF && this->buffer_[1] == GAS_TYPE_CO) {
        if (this->validate_checksum_(this->buffer_)) {
          this->parse_data_(this->buffer_);
        } else {
          ESP_LOGW(TAG, "Passive: Checksum mismatch");
        }
      }
      this->buffer_index_ = 0;  // Reset for next packet
    }
  }
}

void ZE15COSensor::parse_data_(const uint8_t *data) {
  // Different parsing for Initiative vs Q&A mode
  uint8_t high_byte, low_byte;
  
  if (data[1] == 0x86) {
    // Q&A mode response
    high_byte = data[2];
    low_byte = data[3];
    ESP_LOGV(TAG, "Q&A raw data: %02X %02X %02X %02X %02X %02X %02X %02X %02X",
             data[0], data[1], data[2], data[3], data[4], 
             data[5], data[6], data[7], data[8]);
  } else {
    // Initiative mode
    high_byte = data[4];
    low_byte = data[5];
    ESP_LOGV(TAG, "Passive raw data: %02X %02X %02X %02X %02X %02X %02X %02X %02X",
             data[0], data[1], data[2], data[3], data[4], 
             data[5], data[6], data[7], data[8]);
  }
  
  // Extract sensor fault status
  bool sensor_fault = this->extract_sensor_fault_(high_byte);
  
  // Calculate CO concentration
  float co_concentration = this->calculate_concentration_(high_byte, low_byte);
  
  ESP_LOGD(TAG, "CO concentration: %.1f ppm, Sensor fault: %s", 
           co_concentration, sensor_fault ? "YES" : "NO");

  // Update sensors
  if (this->co_sensor_ != nullptr) {
    this->co_sensor_->publish_state(co_concentration);
  }
  
  if (this->sensor_fault_ != nullptr) {
    this->sensor_fault_->publish_state(sensor_fault);
  }
  
  this->last_transmission_ = millis();
}

float ZE15COSensor::calculate_concentration_(uint8_t high_byte, uint8_t low_byte) {
  // Gas concentration = (Low 5 bits of High Byte * 256 + Low Byte) * 0.1
  uint16_t raw_value = ((high_byte & 0x1F) << 8) | low_byte;
  return raw_value * 0.1f;
}

bool ZE15COSensor::extract_sensor_fault_(uint8_t high_byte) {
  // Highest bit (bit 7) indicates sensor fault: 1 = fault, 0 = normal
  return (high_byte & 0x80) != 0;
}

bool ZE15COSensor::validate_checksum_(const uint8_t *data) {
  // Checksum = (~(byte1 + byte2 + ... + byte7)) + 1
  uint8_t sum = 0;
  for (int i = 1; i < 8; i++) {
    sum += data[i];
  }
  uint8_t calculated = (~sum) + 1;
  bool valid = (calculated == data[8]);
  
  if (!valid) {
    ESP_LOGV(TAG, "Checksum calc: %02X, received: %02X", calculated, data[8]);
  }
  
  return valid;
}

void ZE15COSensor::send_command_(const uint8_t *command, uint8_t length) {
  this->write_array(command, length);
  this->flush();
  ESP_LOGV(TAG, "Command sent: %02X %02X %02X ... %02X", 
           command[0], command[1], command[2], command[8]);
}

void ZE15COSensor::dump_config() {
  ESP_LOGCONFIG(TAG, "ZE15-CO Carbon Monoxide Sensor:");
  ESP_LOGCONFIG(TAG, "  Mode: %s", this->mode_ == MODE_QA ? "Q&A" : "Passive");
  if (this->mode_ == MODE_QA) {
    ESP_LOGCONFIG(TAG, "  Update Interval: %u ms", this->update_interval_);
  }
  LOG_SENSOR("  ", "CO", this->co_sensor_);
  LOG_BINARY_SENSOR("  ", "Sensor Fault", this->sensor_fault_);
  this->check_uart_settings(9600);
}

float ZE15COSensor::get_setup_priority() const { 
  return setup_priority::DATA; 
}

}  // namespace ze15_co
}  // namespace esphome