#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/uart/uart.h"

namespace esphome {
namespace ze15_co {

enum ZE15COMode {
  MODE_PASSIVE = 0,  // Initiative upload mode (default)
  MODE_QA = 1        // Question & Answer mode
};

// QA state machine states
enum QAState {
  QA_IDLE,
  QA_READ_SENT,
  QA_WAITING_RESPONSE
};

class ZE15COSensor : public Component, public uart::UARTDevice {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override;

  void set_co_sensor(sensor::Sensor *co_sensor) { co_sensor_ = co_sensor; }
  void set_sensor_fault_binary(binary_sensor::BinarySensor *sensor_fault) { sensor_fault_ = sensor_fault; }
  void set_mode(ZE15COMode mode) { mode_ = mode; }
  void set_update_interval(uint32_t interval) { update_interval_ = interval; }

 protected:
  // Buffer for both modes (9 bytes)
  uint8_t buffer_[9];
  uint8_t buffer_index_{0};

  // Sensors
  sensor::Sensor *co_sensor_{nullptr};
  binary_sensor::BinarySensor *sensor_fault_{nullptr};

  ZE15COMode mode_{MODE_PASSIVE};
  uint32_t update_interval_{60000};  // 60 seconds default
  uint32_t last_transmission_{0};
  uint32_t last_request_{0};
  bool waiting_for_response_{false};
  
  // Mode switching tracking
  uint32_t last_qa_command_{0};
  bool in_qa_mode_{false};
  
  // QA mode state machine
  QAState qa_state_{QA_IDLE};
  uint32_t qa_timer_{0};

  // Constants
  static const uint8_t START_BYTE = 0xFF;
  static const uint8_t GAS_TYPE_CO = 0x04;

  // Commands (9 bytes)
  static const uint8_t CMD_READ_DATA[9];

  // Functions
  bool validate_checksum_(const uint8_t *data);
  void parse_data_(const uint8_t *data);
  void send_command_(const uint8_t *command, uint8_t length);
  float calculate_concentration_(uint8_t high_byte, uint8_t low_byte);
  bool extract_sensor_fault_(uint8_t high_byte);
};

}  // namespace ze15_co
}  // namespace esphome