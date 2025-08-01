# ESPHome Winsen CO Sensors

[![ESPHome](https://img.shields.io/badge/ESPHome-Compatible-blue.svg)](https://esphome.io/)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

ESPHome custom/external components for Winsen electrochemical carbon monoxide (CO) sensors.

> 🟢 **NB**: Examples of using uart debug and commands directly in yaml (without custom components) are available in the [example](example/) folder.

## 📦 Supported Sensors

| Sensor | Range | Status | Component |
|--------|-------|--------|-----------|
| **ZE15-CO** | 0-500 ppm | ✅ Ready | `ze15_co` |
| **ZE07-CO** | 0-500 ppm | ✅ Ready | `ze07_co` |

## 🌟 Features

- 📊 **Carbon Monoxide (CO)** measurement in ppm
- 🔄 **Automatic mode selection** based on configuration (ZE15-CO)
- 🔄 **Dual mode support** for ZE07-CO (Passive/Q&A modes)
- 🚨 **Sensor fault detection** (ZE15-CO only)
- 🏠 **Home Assistant** integration ready
- 📡 **UART communication** at 9600 baud
- 🎯 **High precision**: 0.1 ppm resolution
- 🔋 **Power efficient Q&A mode** with dormant support (ZE07-CO)

## 📦 Installation

### External Component

```yaml
external_components:
  - source: github://Bjk8kds/esphome-winsen-co-sensor
    components: [ ze15_co, ze07_co ]
```

### Local Installation

1. Copy the `components/ze15_co` or `components/ze07_co` folder to your ESPHome configuration directory
2. Use the component in your YAML configuration

## 🔌 Wiring

### ZE15-CO

| ZE15-CO Pin | ESP32 Pin | Function |
|-------------|-----------|----------|
| PIN 15 | 5V | Power Supply (5-12V DC) |
| PIN 5, 14 | GND | Ground |
| PIN 8 | GPIO16 | TX → ESP RX |
| PIN 7 | GPIO17 | RX ← ESP TX (for Q&A mode) |

### ZE07-CO

| ZE07-CO Pin | ESP32 Pin | Function |
|-------------|-----------|----------|
| PIN 15 | 5V | Power Supply (5-12V DC) |
| PIN 5, 14 | GND | Ground |
| PIN 8 | GPIO16 | TX → ESP RX |
| PIN 7 | GPIO17 | RX ← ESP TX (required for Q&A mode) |

> ⚠️ **Note**: ESP TX to sensor RX connection is only required for Q&A mode

## ⚙️ Configuration

### ZE15-CO

The component **automatically selects the operating mode** based on your configuration:

#### Real-time Monitoring (Initiative Mode)
**No `update_interval`** = Receives data every second

```yaml
# UART Configuration
uart:
  rx_pin: GPIO16  # Required
  tx_pin: GPIO17  # Optional for Initiative mode
  baud_rate: 9600

# Sensor Configuration
sensor:
  - platform: ze15_co
    co:
      name: "Carbon Monoxide"
```

#### Periodic Monitoring (Q&A Mode)
**With `update_interval`** = Requests data at specified intervals

```yaml
# UART Configuration  
uart:
  rx_pin: GPIO16  # Required
  tx_pin: GPIO17  # Required for Q&A mode
  baud_rate: 9600

# Sensor Configuration
sensor:
  - platform: ze15_co
    update_interval: 60s  # Minimum 5s, enables Q&A mode
    co:
      name: "Carbon Monoxide"
    sensor_fault:
      name: "CO Sensor Fault"
```

### ZE07-CO

ZE07-CO requires **explicit mode selection** and supports power-saving dormant mode:

#### Passive Mode (Default)
```yaml
sensor:
  - platform: ze07_co
    mode: PASSIVE  # Continuous data every second
    co:
      name: "CO Level"
```

#### Q&A Mode with Dormant
```yaml
sensor:
  - platform: ze07_co
    mode: QA  # Power efficient mode
    update_interval: 60s  # Minimum 45s, default 60s
    co:
      name: "CO Level"
```

## 📊 Configuration Parameters

### ZE15-CO

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `update_interval` | time | - | If set (≥5s): Q&A mode<br>If not set: Initiative mode |
| `co` | sensor | - | CO concentration sensor |
| `sensor_fault` | binary_sensor | - | Sensor fault detection |
| `uart_id` | id | - | UART bus ID |

### ZE07-CO

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `mode` | string | `PASSIVE` | Operating mode: `PASSIVE` or `QA` |
| `update_interval` | time | `60s` | Update interval for Q&A mode (≥45s) |
| `co` | sensor | - | CO concentration sensor |
| `uart_id` | id | - | UART bus ID |

## 🔄 Operating Modes Comparison

| Feature | ZE15-CO | ZE07-CO |
|---------|---------|---------|
| **Mode Selection** | Automatic (based on `update_interval`) | Explicit (`mode` parameter) |
| **Initiative/Passive Mode** | ✅ Real-time data | ✅ Real-time data |
| **Q&A Mode** | ✅ Simple request/response | ✅ With dormant support |
| **Dormant Mode** | ❌ Always active | ✅ Power saving between readings |
| **Sensor Fault Detection** | ✅ Available | ❌ Not available |
| **Min Q&A Interval** | 5 seconds | 45 seconds |
| **Warm-up Time** | 30 seconds | 3 minutes |

## 💡 Examples

### Basic Configuration
```yaml
# ZE15-CO - Real-time monitoring
sensor:
  - platform: ze15_co
    co:
      name: "CO Level"

# ZE07-CO - Real-time monitoring
sensor:
  - platform: ze07_co
    co:
      name: "CO Level"
```

### Power-Efficient Monitoring
```yaml
# ZE15-CO - Q&A mode (sensor always active)
sensor:
  - platform: ze15_co
    update_interval: 60s
    co:
      name: "CO Level"
    sensor_fault:
      name: "CO Sensor Status"

# ZE07-CO - Q&A mode with dormant (extends sensor life)
sensor:
  - platform: ze07_co
    mode: QA
    update_interval: 5min  # Battery friendly
    co:
      name: "CO Level"
```

### Advanced Monitoring with Alerts
```yaml
sensor:
  - platform: ze15_co
    co:
      name: "Indoor CO"
      id: co_indoor
      on_value_range:
        - above: 30
          then:
            - logger.log: 
                level: WARN
                format: "CO level elevated: %.1f ppm"
                args: ['x']
        - above: 50
          then:
            - logger.log:
                level: ERROR
                format: "DANGER! CO level: %.1f ppm"
                args: ['x']

binary_sensor:
  - platform: template
    name: "CO Alarm"
    lambda: |-
      return id(co_indoor).state > 50;
    device_class: gas
```

## 🏥 CO Safety Reference

| CO Level (ppm) | Health Effects | Action Required |
|----------------|----------------|-----------------|
| 0-9 | Normal background | None |
| 10-29 | Typical indoor levels | Monitor |
| 30-49 | Potential health effects | Increase ventilation |
| 50-99 | Dangerous | Evacuate & ventilate |
| 100+ | Life-threatening | Immediate evacuation |

## 🔍 Troubleshooting

### No Data Received
- Check power supply (5-12V)
- Verify UART connections
- Ensure 9600 baud rate
- Allow sensor warm-up time (30s for ZE15-CO, 3min for ZE07-CO)

### Mode Issues
- **ZE15-CO**: Check if `update_interval` is set correctly
- **ZE07-CO**: Verify `mode` parameter and TX connection for Q&A mode

### Debug Logging
```yaml
logger:
  level: DEBUG
  logs:
    ze15_co: VERY_VERBOSE
    ze07_co: VERY_VERBOSE
```

## 📚 Documentation

- [Detailed Examples](example/)
- [ZE15-CO Datasheet (PDF)](https://www.winsen-sensor.com/d/files/ze15-co-module-manual-v1_1.pdf)
- [ZE07-CO Datasheet (PDF)](https://www.winsen-sensor.com/d/files/ze07-co-module-1_7.pdf)
- [Winsen Official Website](https://www.winsen-sensor.com/)

## 🤝 Contributing

Contributions welcome! Especially for:
- Additional Winsen CO sensors
- Performance optimizations
- Documentation improvements

## 📄 License

MIT License - see [LICENSE](LICENSE) file for details.

## 🙏 Acknowledgments

- ESPHome development team
- Winsen Electronics for sensor documentation
- Community contributors

## 📞 Support

- 📖 [ESPHome Documentation](https://esphome.io/)
- 💬 [Home Assistant Community](https://community.home-assistant.io/)
- 🐛 [Issue Tracker](https://github.com/Bjk8kds/esphome-winsen-co-sensor)

---

Made with ❤️ for the ESPHome community
