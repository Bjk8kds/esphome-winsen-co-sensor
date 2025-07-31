#include "ze07_co.h"
#include "esphome/core/log.h"

namespace esphome {
namespace ze07_co {

static const char *const TAG = "ze07_co";

// Q&A mode commands (9 bytes each) - Same as ZH03B
const uint8_t ZE07COSensor::CMD_READ_DATA[9] = {0xFF, 0x01, 0x86, 0x00, 0x00, 0x00, 0x00, 0x00, 0x79};
const uint8_t ZE07COSensor::CMD_SET_QA_MODE[9] = {0xFF, 0x01, 0x78, 0x41, 0x00, 0x00, 0x00, 0x00, 0x46};
const uint8_t ZE07COSensor::CMD_SET_PASSIVE_MODE[9] = {0xFF, 0x01, 0x78, 0x40, 0x00, 0x00, 0x00, 0x00, 0x47};
const uint8_t ZE07COSensor::CMD_DORMANT_ON[9]  = {0xFF, 0x01, 0xA7, 0x01, 0x00, 0x00, 0x00, 0x00, 0x57};
const uint8_t ZE07COSensor::CMD_DORMANT_OFF[9] = {0xFF, 0x01, 0xA7, 0x00, 0x00, 0x00, 0x00, 0x00, 0x58};

void ZE07COSensor::setup() {
  ESP_LOGCONFIG(TAG, "Setting up ZE07-CO...");
  this->passive_index_ = 0;
  this->qa_index_ = 0;
  this->stabilizing_ = true;
  this->stabilization_start_ = millis();

  // Clear UART buffer
  while (this->available()) {
    uint8_t dummy;
    this->read_byte(&dummy);
  }

  // Set mode first before waking up
  if (this->mode_ == MODE_QA) {
    ESP_LOGCONFIG(TAG, "Setting ZE07-CO to Q&A mode");
    this->send_command_(CMD_SET_QA_MODE, 9);
    delay(500);
    // Put to dormant after mode change
    this->send_command_(CMD_DORMANT_ON, 9);
    delay(500);
  } else {
    ESP_LOGCONFIG(TAG, "Setting ZE07-CO to Passive mode");
    this->send_command_(CMD_SET_PASSIVE_MODE, 9);
    delay(500);
    // Wake up for continuous reading in passive mode
    this->send_command_(CMD_DORMANT_OFF, 9);
    delay(1000);
  }

  ESP_LOGCONFIG(TAG, "Waiting 3 minutes for sensor stabilization...");
}

void ZE07COSensor::loop() {
  // Stabilization period (3 minutes for ZE07-CO)
  if (this->stabilizing_) {
    if (millis() - this->stabilization_start_ < 180000) {  // 3 minutes
      // During stabilization, clear buffer to prevent old data
      while (this->available()) {
        uint8_t dummy;
        this->read_byte(&dummy);
      }
      return;
    }
    this->stabilizing_ = false;
    ESP_LOGI(TAG, "Sensor stabilization complete");
  }

  if (this->mode_ == MODE_QA) {
    // Q&A Mode State Machine
    switch (qa_state_) {
      case QA_IDLE:
        if (millis() - this->last_request_ >= this->update_interval_) {
          ESP_LOGD(TAG, "QA: Waking sensor");
          this->send_command_(CMD_DORMANT_OFF, 9);
          qa_timer_ = millis();
          qa_state_ = QA_WAKE_SENT;
          this->qa_index_ = 0;  // Reset buffer
        }
        break;

      case QA_WAKE_SENT:
        if (millis() - qa_timer_ >= 30000) {  // 30s wake time for ZE07-CO
          ESP_LOGD(TAG, "QA: Requesting data");
          // Clear buffer before request
          while (this->available()) {
            uint8_t dummy;
            this->read_byte(&dummy);
          }
          this->send_command_(CMD_READ_DATA, 9);
          qa_timer_ = millis();
          qa_state_ = QA_READ_SENT;
          this->qa_index_ = 0;
        }
        break;

      case QA_READ_SENT:
        // Read available data
        while (this->available() && this->qa_index_ < 9) {
          uint8_t byte;
          this->read_byte(&byte);
          
          // Sync to start byte
          if (this->qa_index_ == 0 && byte != 0xFF) {
            continue;  // Skip until we find start byte
          }
          
          this->qa_buffer_[this->qa_index_++] = byte;
          
          // Check if we have complete packet
          if (this->qa_index_ == 9) {
            if (this->qa_buffer_[0] == 0xFF && this->qa_buffer_[1] == 0x86) {
              if (this->validate_checksum_(this->qa_buffer_)) {
                this->parse_qa_data_(this->qa_buffer_);
                ESP_LOGD(TAG, "QA: Data received successfully");
              } else {
                ESP_LOGW(TAG, "QA: Checksum failed");
              }
            } else {
              ESP_LOGW(TAG, "QA: Invalid response header");
            }
            qa_timer_ = millis();
            qa_state_ = QA_WAITING_SLEEP;
          }
        }
        
        // Timeout check
        if (millis() - qa_timer_ >= 3000) {  // 3s timeout
          ESP_LOGW(TAG, "QA: Response timeout");
          qa_timer_ = millis();
          qa_state_ = QA_WAITING_SLEEP;
        }
        break;

      case QA_WAITING_SLEEP:
        if (millis() - qa_timer_ >= 2000) {  // 2s delay before sleep
          ESP_LOGD(TAG, "QA: Putting sensor to sleep");
          this->send_command_(CMD_DORMANT_ON, 9);
          this->last_request_ = millis();
          qa_state_ = QA_IDLE;
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
    if (this->passive_index_ == 0 && byte != START_BYTE) {
      continue;  // Skip until we find start byte
    }
    
    this->passive_buffer_[this->passive_index_++] = byte;
    
    // Check if we have complete packet
    if (this->passive_index_ >= 9) {
      // Validate packet
      if (this->passive_buffer_[0] == START_BYTE && this->passive_buffer_[1] == GAS_TYPE_CO) {
        if (this->validate_checksum_(this->passive_buffer_)) {
          this->parse_passive_data_(this->passive_buffer_);
        } else {
          ESP_LOGW(TAG, "Passive: Checksum mismatch");
        }
      }
      this->passive_index_ = 0;  // Reset for next packet
    }
  }
}

void ZE07COSensor::parse_passive_data_(const uint8_t *data) {
  // Log raw data for debugging
  ESP_LOGV(TAG, "Passive raw data: %02X %02X %02X %02X %02X %02X %02X %02X %02X",
           data[0], data[1], data[2], data[3], data[4], 
           data[5], data[6], data[7], data[8]);

  // Parse CO concentration from passive mode data
  float co_concentration = this->calculate_concentration_(data[4], data[5]);

  ESP_LOGD(TAG, "Passive mode - CO: %.1f ppm", co_concentration);

  // Validate reasonable values (ZE07-CO range is 0-500 ppm)
  if (co_concentration > 500) {
    ESP_LOGW(TAG, "CO value out of range (>500 ppm), ignoring");
    return;
  }

  // Update sensor
  if (this->co_sensor_ != nullptr) {
    this->co_sensor_->publish_state(co_concentration);
  }
  
  this->last_transmission_ = millis();
}

void ZE07COSensor::parse_qa_data_(const uint8_t *data) {
  // Log raw data for debugging
  ESP_LOGV(TAG, "QA raw data: %02X %02X %02X %02X %02X %02X %02X %02X %02X",
           data[0], data[1], data[2], data[3], data[4], 
           data[5], data[6], data[7], data[8]);
  
  // Parse CO value from Q&A response
  // According to ZE07-CO datasheet: CO value at bytes 2 (high) and 3 (low)
  float co_concentration = this->calculate_concentration_(data[2], data[3]);
  
  ESP_LOGD(TAG, "Q&A mode - CO: %.1f ppm", co_concentration);
  
  // Validate reasonable values
  if (co_concentration > 500) {
    ESP_LOGW(TAG, "CO value out of range (>500 ppm), ignoring");
    return;
  }
  
  // Update sensor
  if (this->co_sensor_ != nullptr) {
    this->co_sensor_->publish_state(co_concentration);
  }
}

float ZE07COSensor::calculate_concentration_(uint8_t high_byte, uint8_t low_byte) {
  // Gas concentration = (High Byte * 256 + Low Byte) * 0.1
  uint16_t raw_value = (high_byte << 8) | low_byte;
  return raw_value * 0.1f;
}

bool ZE07COSensor::validate_checksum_(const uint8_t *data) {
  // Checksum = (~(byte1 + byte2 + ... + byte7)) + 1
  uint8_t sum = 0;
  for (int i = 1; i < 8; i++) {
    sum += data[i];
  }
  uint8_t calculated = (~sum) + 1;
  return (calculated == data[8]);
}

void ZE07COSensor::send_command_(const uint8_t *command, uint8_t length) {
  this->write_array(command, length);
  this->flush();
  ESP_LOGV(TAG, "Command sent: %02X %02X %02X ... %02X", 
           command[0], command[1], command[2], command[8]);
}

void ZE07COSensor::dump_config() {
  ESP_LOGCONFIG(TAG, "ZE07-CO Carbon Monoxide Sensor:");
  ESP_LOGCONFIG(TAG, "  Mode: %s", this->mode_ == MODE_QA ? "Q&A" : "Passive");
  ESP_LOGCONFIG(TAG, "  Update Interval: %u ms", this->update_interval_);
  LOG_SENSOR("  ", "CO", this->co_sensor_);
  this->check_uart_settings(9600);
}

float ZE07COSensor::get_setup_priority() const { 
  return setup_priority::DATA; 
}

}  // namespace ze07_co
}  // namespace esphome
