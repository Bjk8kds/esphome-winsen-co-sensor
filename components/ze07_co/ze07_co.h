#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart.h"

namespace esphome {
namespace ze07_co {

enum ZE07COMode {
  MODE_PASSIVE = 0,  // Initiative upload mode (default)
  MODE_QA = 1        // Question & Answer mode
};

// QA state machine states
enum QAState {
  QA_IDLE,
  QA_WAKE_SENT,
  QA_READ_SENT,
  QA_WAITING_SLEEP
};

class ZE07COSensor : public Component, public uart::UARTDevice {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override;

  void set_co_sensor(sensor::Sensor *co_sensor) { co_sensor_ = co_sensor; }
  void set_mode(ZE07COMode mode) { mode_ = mode; }
  void set_update_interval(uint32_t interval) { update_interval_ = interval; }

 protected:
  // Buffer for passive mode (9 bytes like ZE15-CO)
  uint8_t passive_buffer_[9];
  uint8_t passive_index_{0};
  // Buffer for Q&A mode (9 bytes)
  uint8_t qa_buffer_[9];
  uint8_t qa_index_{0};

  // Sensor pointer
  sensor::Sensor *co_sensor_{nullptr};

  ZE07COMode mode_{MODE_PASSIVE};
  uint32_t update_interval_{60000};  // 60 seconds default
  uint32_t last_transmission_{0};
  uint32_t last_request_{0};
  
  // Stabilization tracking
  bool stabilizing_{false};
  uint32_t stabilization_start_{0};
  
  // QA mode state machine
  QAState qa_state_{QA_IDLE};
  uint32_t qa_timer_{0};

  // Constants
  static const uint8_t START_BYTE = 0xFF;
  static const uint8_t GAS_TYPE_CO = 0x04;
  static const uint8_t CMD_RESPONSE = 0x86;

  // Commands (9 bytes) - Same as ZH03B
  static const uint8_t CMD_READ_DATA[9];
  static const uint8_t CMD_SET_QA_MODE[9];
  static const uint8_t CMD_SET_PASSIVE_MODE[9];
  static const uint8_t CMD_DORMANT_ON[9];
  static const uint8_t CMD_DORMANT_OFF[9];

  // Functions
  bool validate_checksum_(const uint8_t *data);
  void parse_passive_data_(const uint8_t *data);
  void parse_qa_data_(const uint8_t *data);
  void send_command_(const uint8_t *command, uint8_t length);
  float calculate_concentration_(uint8_t high_byte, uint8_t low_byte);
};

}  // namespace ze07_co
}  // namespace esphome
