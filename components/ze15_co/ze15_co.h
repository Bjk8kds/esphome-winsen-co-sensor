#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/uart/uart.h"

namespace esphome {
namespace ze15_co {

class ZE15COSensor : public Component, public uart::UARTDevice {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override;

  void set_co_sensor(sensor::Sensor *co_sensor) { co_sensor_ = co_sensor; }
  void set_sensor_fault_binary(binary_sensor::BinarySensor *sensor_fault) { sensor_fault_ = sensor_fault; }
  void set_update_interval(uint32_t interval) { update_interval_ = interval; }
  void set_qa_mode_enabled(bool enabled) { qa_mode_enabled_ = enabled; }

 protected:
  // Buffer for data (9 bytes)
  uint8_t buffer_[9];
  uint8_t buffer_index_{0};

  // Sensors
  sensor::Sensor *co_sensor_{nullptr};
  binary_sensor::BinarySensor *sensor_fault_{nullptr};

  // Configuration
  bool qa_mode_enabled_{false};  // true if update_interval is set
  uint32_t update_interval_{60000};  // Only used if qa_mode_enabled
  
  // Timing
  uint32_t last_request_{0};
  uint32_t last_transmission_{0};

  // Constants
  static const uint8_t START_BYTE = 0xFF;
  static const uint8_t GAS_TYPE_CO = 0x04;
  static const uint8_t CMD_RESPONSE = 0x86;

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
