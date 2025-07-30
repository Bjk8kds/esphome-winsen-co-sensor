# ESPHome ZE15-CO Carbon Monoxide Sensor

[![ESPHome](https://img.shields.io/badge/ESPHome-Compatible-blue.svg)](https://esphome.io/)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

ESPHome Custom/External component for Winsen ZE15-CO electrochemical carbon monoxide sensor with support for both Initiative Upload (Passive) and Question & Answer (Q&A) modes.

## 🌟 Features

- 📊 **Carbon Monoxide (CO)** measurement in ppm (parts per million)
- ✅ **Dual Mode Support**: Initiative (Passive) and Q&A modes
- 🔋 **Power-efficient Q&A mode** with configurable intervals
- 🚨 **Sensor fault detection** with binary sensor output
- ⚡ **Auto mode switching**: Automatically switches between modes based on commands
- 🏠 **Home Assistant** integration ready
- 📡 **UART communication** at 9600 baud
- 🎯 **High precision**: 0.1 ppm resolution
- 🌡️ **Temperature compensated** measurements

## 📦 Installation

### Method 1: External Components (When Published)

```yaml
external_components:
  - source: github://yourusername/esphome-ze15-co-sensor
    components: [ ze15_co ]
```

### Method 2: Local Components

1. Create a `components` folder in your ESPHome configuration directory
2. Copy the `ze15_co` folder into the components directory
3. Use the component in your YAML configuration

## 🔌 Wiring

| ZE15-CO Pin | Function | ESP32 Pin | Description |
|-------------|----------|-----------|-------------|
| PIN 15 | VIN | 5V-12V | Power Supply (5-12V DC) |
| PIN 5, 14 | GND | GND | Ground |
| PIN 8 | TXD | GPIO16 (RX) | UART Data Output |
| PIN 7 | RXD | GPIO17 (TX) | UART Command Input |
| PIN 10 | Analog Out | ADC Pin | Optional: 0.4-2V = 0-500ppm |
| PIN 3 | Fault | GPIO | Optional: Fault signal output |

> ⚠️ **Important**: TX connection to sensor RX is required for Q&A mode operation

## ⚙️ Configuration

### Basic Setup (Initiative / Passive Mode)

```yaml
# UART Configuration
uart:
  id: uart_ze15
  tx_pin: GPIO17
  rx_pin: GPIO16
  baud_rate: 9600

# Sensor Configuration
sensor:
  - platform: ze15_co
    uart_id: uart_ze15
    co:
      name: "Carbon Monoxide"
```

### Power-Efficient Setup (Q&A Mode)

```yaml
# UART Configuration
uart:
  id: uart_ze15
  tx_pin: GPIO17
  rx_pin: GPIO16
  baud_rate: 9600

# Sensor Configuration
sensor:
  - platform: ze15_co
    uart_id: uart_ze15
    mode: QA
    update_interval: 2min  # Request data every 2 minutes
    co:
      name: "Carbon Monoxide"
    
binary_sensor:
  - platform: ze15_co
    uart_id: uart_ze15
    sensor_fault:
      name: "CO Sensor Fault"
```

## 📊 Configuration Parameters

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `mode` | string | `PASSIVE` | Operating mode: `PASSIVE` or `QA` |
| `update_interval` | time | `60s` | Update interval for Q&A mode (min 30s) |
| `co` | sensor | - | CO concentration sensor configuration |
| `sensor_fault` | binary_sensor | - | Sensor fault detection |
| `uart_id` | id | - | UART bus ID reference |

## 🔄 Operating Modes

### Passive Mode (Initiative Upload)
- Sensor continuously transmits data every second
- Real-time CO monitoring
- No commands sent to sensor
- Higher power consumption
- Auto-selected when no commands received for 30s

### Q&A Mode (Question & Answer)
- ESP32 controls when to request data
- Power efficient operation
- Minimum 30s update interval
- Requires TX connection to sensor
- Auto-selected when receiving Q&A commands

## 📊 Technical Specifications

### Measurement Range
- **Range**: 0-500 ppm
- **Resolution**: 0.1 ppm
- **Response Time**: ≤30s
- **Recovery Time**: ≤30s
- **Warm-up Time**: 30s

### Communication Protocol
- **Baud Rate**: 9600
- **Data Bits**: 8
- **Stop Bits**: 1
- **Check Byte**: Checksum validation

### Data Format
| Mode | Bytes | Start | Description |
|------|-------|-------|-------------|
| Initiative | 9 | 0xFF 0x04 | Auto-sent every 1s |
| Q&A Response | 9 | 0xFF 0x86 | Response to 0x86 command |

## 💡 Advanced Examples

### CO Safety Monitor with Alarms
```yaml
sensor:
  - platform: ze15_co
    uart_id: uart_ze15
    co:
      name: "CO Level"
      id: co_sensor
      on_value_range:
        - above: 30
          below: 50
          then:
            - logger.log: 
                format: "Warning: CO level %.1f ppm"
                args: ['x']
        - above: 50
          then:
            - logger.log:
                format: "DANGER: CO level %.1f ppm!"
                args: ['x']
                level: ERROR

binary_sensor:
  - platform: template
    name: "CO Alarm"
    lambda: |-
      return id(co_sensor).state > 50;
    device_class: gas
```

### With Status LED Indicator
```yaml
output:
  - platform: ledc
    pin: GPIO2
    id: status_led_output

light:
  - platform: monochromatic
    output: status_led_output
    name: "CO Status"
    id: status_led
    effects:
      - lambda:
          name: CO Level
          update_interval: 1s
          lambda: |-
            auto co = id(co_sensor).state;
            if (co < 30) {
              // Green - pulse slowly
              it.set_brightness(0.5 + 0.5 * sin(millis() / 2000.0));
            } else if (co < 50) {
              // Yellow - pulse medium
              it.set_brightness(0.5 + 0.5 * sin(millis() / 1000.0));
            } else {
              // Red - flash quickly
              it.set_brightness((millis() / 250) % 2);
            }
```

## 🏥 CO Safety Levels Reference

| Level (ppm) | Effect | Exposure Time |
|-------------|--------|---------------|
| 0-9 | Normal background | Indefinite |
| 10-29 | Typical home levels | Indefinite |
| 30-49 | **Warning** - Flu-like symptoms | 8 hours |
| 50-99 | **Danger** - Headache, nausea | 2-3 hours |
| 100-199 | **Severe** - Severe symptoms | 1-2 hours |
| 200+ | **Life-threatening** | Minutes |

## 🔍 Troubleshooting

### No Data Received
- ✓ Check 5-12V power supply to sensor
- ✓ Verify UART pin connections
- ✓ Ensure correct baud rate (9600)
- ✓ Allow 30s warm-up time

### Q&A Mode Issues
- ✓ Confirm TX pin is connected to sensor RX
- ✓ Verify update_interval ≥ 30s
- ✓ Check mode auto-switching (30s timeout)

### Erratic Readings
- ✓ Ensure proper ventilation around sensor
- ✓ Check for electromagnetic interference
- ✓ Verify power supply stability
- ✓ Allow sensor to stabilize for 5 minutes on first use

### Sensor Fault Detection
```yaml
logger:
  level: DEBUG
  logs:
    ze15_co: VERY_VERBOSE
```

## 🛠️ Development

### Debug Output Example
```
[D][ze15_co:123]: Passive mode - CO: 2.5 ppm, Fault: NO
[D][ze15_co:234]: Q&A: Sending read command
[D][ze15_co:345]: Q&A mode - CO: 2.4 ppm, Fault: NO
[W][ze15_co:456]: High CO level detected: 55.2 ppm
```

### Checksum Calculation
```cpp
checksum = (~(byte1 + byte2 + ... + byte7)) + 1
```

### Concentration Formula
```cpp
CO (ppm) = ((high_byte & 0x1F) * 256 + low_byte) * 0.1
```

## ⚠️ Safety Warnings

- **CO is a deadly gas** - Install proper CO alarms in addition to this sensor
- This sensor is for monitoring only, not for life safety applications
- Always follow local building codes for CO detection
- Test sensor functionality regularly
- Replace sensor every 3-5 years as recommended

## 📚 References

- [ZE15-CO Datasheet](https://www.winsen-sensor.com/sensors/co-sensor/ze15-co.html)
- [ESPHome UART Component](https://esphome.io/components/uart.html)
- [CO Safety Guidelines](https://www.cdc.gov/co/faqs.htm)

## 🤝 Contributing

Contributions are welcome! Please:
1. Fork the repository
2. Create a feature branch
3. Test thoroughly
4. Submit a Pull Request

## 📄 License

This project is licensed under the MIT License.

## 🙏 Acknowledgments

- Based on the ZH03B component structure
- ESPHome development team
- Winsen Electronics for sensor documentation

---

Made with ❤️ for the ESPHome community
