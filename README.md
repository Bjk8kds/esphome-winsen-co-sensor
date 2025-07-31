# ESPHome Winsen CO Sensors

[![ESPHome](https://img.shields.io/badge/ESPHome-Compatible-blue.svg)](https://esphome.io/)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

ESPHome custom/external components for Winsen electrochemical carbon monoxide (CO) sensors.

> 🟢 **NB**: Example of using uart debug and commands directly in yaml (without this custom component), please check [example](example/).

## 📦 Supported Sensors

| Sensor | Range | Status | Component |
|--------|-------|--------|-----------|
| **ZE15-CO** | 0-500 ppm | ✅ Ready | `ze15_co` |
| **ZE07-CO** | 0-1000 ppm | 🚧 In Progress | `ze07_co` |

## 🌟 Features

- 📊 **Carbon Monoxide (CO)** measurement in ppm
- 🔄 **Automatic mode selection** based on configuration
- 🚨 **Sensor fault detection** 
- 🏠 **Home Assistant** integration ready
- 📡 **UART communication** at 9600 baud
- 🎯 **High precision**: 0.1 ppm resolution

## 📦 Installation

### External Component

```yaml
external_components:
  - source: github://Bjk8kds/esphome-winsen-co-sensor
    components: [ ze15_co ]
```

### Local Installation

1. Copy the `components/ze15_co` folder to your ESPHome configuration directory
2. Use the component in your YAML configuration

## 🔌 Wiring (ZE15-CO)

| ZE15-CO Pin | ESP32 Pin | Function |
|-------------|-----------|----------|
| PIN 15 | 5V | Power Supply (5-12V DC) |
| PIN 5, 14 | GND | Ground |
| PIN 8 | GPIO16 | TX → ESP RX |
| PIN 7 | GPIO17 | RX ← ESP TX (for Q&A mode) |

> ⚠️ **Note**: ESP TX to sensor RX connection is only required for Q&A mode

## ⚙️ Configuration

The component **automatically selects the operating mode** based on your configuration:

### Real-time Monitoring (Initiative Mode)
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

### Periodic Monitoring (Q&A Mode)
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

## 📊 Configuration Parameters

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `update_interval` | time | - | If set (≥5s): Q&A mode<br>If not set: Initiative mode |
| `co` | sensor | - | CO concentration sensor |
| `sensor_fault` | binary_sensor | - | Sensor fault detection |
| `uart_id` | id | - | UART bus ID |

## 🔄 Operating Modes

### Initiative Mode (Default)
- **Configuration**: Do NOT set `update_interval`
- **Behavior**: Sensor sends data every second automatically
- **Use case**: Real-time monitoring, Home Assistant automation
- **Power**: Higher consumption (continuous operation)

### Q&A Mode
- **Configuration**: Set `update_interval` (minimum 5 seconds)
- **Behavior**: ESP requests data at specified intervals
- **Use case**: Battery-powered devices, periodic logging
- **Power**: More efficient (sensor responds only when asked)

### Mode Behavior
- The sensor **automatically switches** between modes based on commands
- Sending a read command switches sensor to Q&A mode
- After 30 seconds without commands, sensor returns to Initiative mode
- In Q&A mode configuration, only Q&A responses are processed (Initiative data is ignored)

## 💡 Examples

### Basic Configuration
```yaml
# Minimal setup - Real-time monitoring
sensor:
  - platform: ze15_co
    co:
      name: "CO Level"
```

### Advanced Monitoring
```yaml
sensor:
  # Real-time indoor monitoring
  - platform: ze15_co
    uart_id: uart_indoor
    co:
      name: "Indoor CO"
      id: co_indoor
      filters:
        - sliding_window_moving_average:
            window_size: 10
            send_every: 10
            
  # Periodic outdoor monitoring (Q&A mode)
  - platform: ze15_co
    uart_id: uart_outdoor
    update_interval: 5min
    co:
      name: "Outdoor CO"
    sensor_fault:
      name: "Outdoor Sensor Fault"

binary_sensor:
  - platform: template
    name: "CO Alarm"
    lambda: |-
      return id(co_indoor).state > 50;
    device_class: gas
```

### With Safety Alerts
```yaml
sensor:
  - platform: ze15_co
    co:
      name: "CO Level"
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
- Allow 30s sensor warm-up

### Mode Issues
- **Initiative mode not working**: Check RX connection
- **Q&A mode not working**: Check TX connection, verify `update_interval` ≥ 5s
- **Unexpected mode switching**: Normal behavior after 30s timeout

### Debug Logging
```yaml
logger:
  level: DEBUG
  logs:
    ze15_co: VERY_VERBOSE
```

## 📚 Documentation

- [Detailed Examples](example/)
- [ZE15-CO Datasheet (PDF)](https://www.winsen-sensor.com/d/files/ze15-co-module-manual-v1_1.pdf)
- [Winsen Official Website](https://www.winsen-sensor.com/)

## 🤝 Contributing

Contributions welcome! Especially for:
- ZE07-CO implementation
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
