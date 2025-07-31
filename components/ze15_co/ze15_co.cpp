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

  // Clear UART buffer
  while (this->available()) {
    uint8_t dummy;
    this->read_byte(&dummy);
  }

  if (this->qa_mode_enabled_) {
    ESP_LOGCONFIG(TAG, "ZE15-CO in Q&A mode (update_interval: %us)", this->update_interval_ / 1000);
  } else {
    ESP_LOGCONFIG(TAG, "ZE15-CO in Initiative mode (real-time data)");
  }
}

void ZE15COSensor::loop() {
  // Send Q&A command periodically if Q&A mode is enabled
  if (this->qa_mode_enabled_) {
    if (millis() - this->last_request_ >= this->update_interval_) {
      ESP_LOGD(TAG, "Sending Q&A read command");
      
      // Clear buffer before sending command
      while (this->available()) {
        uint8_t dummy;
        this->read_byte(&dummy);
      }
      
      this->send_command_(CMD_READ_DATA, 9);
      this->last_request_ = millis();
    }
  }
  
  // Always read incoming data
  while (this->available()) {
    uint8_t byte;
    this->read_byte(&byte);
    
    // Sync to start byte
    if (this->buffer_index_ == 0 && byte != START_BYTE) {
      continue;
    }
    
    this->buffer_[this->buffer_index_++] = byte;
    
    // Need at least 2 bytes to identify packet type
    if (this->buffer_index_ >= 2) {
      bool should_process = false;
      
      if (this->qa_mode_enabled_) {
        // In Q&A mode: ONLY process Q&A responses (0x86)
        if (this->buffer_[1] == CMD_RESPONSE) {
          should_process = true;
        } else if (this->buffer_[1] == GAS_TYPE_CO) {
          // Initiative data - ignore in Q&A mode
          ESP_LOGV(TAG, "Ignoring Initiative data in Q&A mode");
          this->buffer_index_ = 0;
          continue;
        }
      } else {
        // In Initiative mode: ONLY process Initiative data (0x04)
        if (this->buffer_[1] == GAS_TYPE_CO) {
          should_process = true;
        } else if (this->buffer_[1] == CMD_RESPONSE) {
          // Q&A response - unexpected in Initiative mode
          ESP_LOGW(TAG, "Unexpected Q&A response in Initiative mode");
          this->buffer_index_ = 0;
          continue;
        }
      }
      
      // Process packet if we have complete data
      if (should_process && this->buffer_index_ >= 9) {
        if (this->validate_checksum_(this->buffer_)) {
          this->parse_data_(this->buffer_);
          
          if (this->qa_mode_enabled_) {
            ESP_LOGD(TAG, "Q&A response processed");
          }
        } else {
          ESP_LOGW(TAG, "Checksum mismatch");
        }
        this->buffer_index_ = 0;
      } else if (!should_process) {
        // Wrong packet type, reset
        this->buffer_index_ = 0;
      }
    }
    
    // Prevent buffer overflow
    if (this->buffer_index_ >= 9) {
      this->buffer_index_ = 0;
    }
  }
}

void ZE15COSensor::parse_data_(const uint8_t *data) {
  uint8_t high_byte, low_byte;
  
  if (data[1] == CMD_RESPONSE) {
    // Q&A mode response
    high_byte = data[2];
    low_byte = data[3];
    ESP_LOGV(TAG, "Q&A response: %02X %02X %02X %02X %02X %02X %02X %02X %02X",
             data[0], data[1], data[2], data[3], data[4], 
             data[5], data[6], data[7], data[8]);
  } else {
    // Initiative mode
    high_byte = data[4];
    low_byte = data[5];
    ESP_LOGV(TAG, "Initiative data: %02X %02X %02X %02X %02X %02X %02X %02X %02X",
             data[0], data[1], data[2], data[3], data[4], 
             data[5], data[6], data[7], data[8]);
  }
  
  // Extract sensor fault status
  bool sensor_fault = this->extract_sensor_fault_(high_byte);
  
  // Calculate CO concentration
  float co_concentration = this->calculate_concentration_(high_byte, low_byte);
  
  ESP_LOGD(TAG, "CO: %.1f ppm, Fault: %s", 
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
  if (this->qa_mode_enabled_) {
    ESP_LOGCONFIG(TAG, "  Mode: Q&A (update_interval: %us)", this->update_interval_ / 1000);
  } else {
    ESP_LOGCONFIG(TAG, "  Mode: Initiative (real-time)");
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
