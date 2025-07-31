import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, uart, binary_sensor
from esphome.const import (
    CONF_ID,
    CONF_UPDATE_INTERVAL,
    DEVICE_CLASS_CARBON_MONOXIDE,
    STATE_CLASS_MEASUREMENT,
    UNIT_PARTS_PER_MILLION,
    ENTITY_CATEGORY_DIAGNOSTIC,
)

CONF_CO = "co"
CONF_SENSOR_FAULT = "sensor_fault"

DEPENDENCIES = ["uart"]

ze15_co_ns = cg.esphome_ns.namespace("ze15_co")
ZE15COSensor = ze15_co_ns.class_("ZE15COSensor", cg.Component, uart.UARTDevice)

def validate_update_interval(config):
    """Validate update interval for Q&A mode"""
    if CONF_UPDATE_INTERVAL in config:
        interval = config[CONF_UPDATE_INTERVAL]
        interval_ms = interval.total_milliseconds
        if interval_ms < 5000:  # 5 seconds minimum
            interval_seconds = interval_ms / 1000
            raise cv.Invalid(
                f"update_interval must be at least 5s for Q&A mode. "
                f"You set {interval_seconds}s. "
                "For real-time monitoring, remove update_interval to use Initiative mode."
            )
    return config

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(ZE15COSensor),
            cv.Optional(CONF_UPDATE_INTERVAL): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_CO): sensor.sensor_schema(
                unit_of_measurement=UNIT_PARTS_PER_MILLION,
                icon="mdi:molecule-co",
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_CARBON_MONOXIDE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_SENSOR_FAULT): binary_sensor.binary_sensor_schema(
                icon="mdi:alert-circle",
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
                device_class="problem",
            ),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(uart.UART_DEVICE_SCHEMA),
    validate_update_interval,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    # If update_interval is set, use Q&A mode
    # If not set, use Initiative mode
    if CONF_UPDATE_INTERVAL in config:
        interval = config[CONF_UPDATE_INTERVAL]
        interval_ms = max(5000, int(interval.total_milliseconds if hasattr(interval, 'total_milliseconds') else interval))
        cg.add(var.set_update_interval(interval_ms))
        cg.add(var.set_qa_mode_enabled(True))
    else:
        # No update_interval = Initiative mode
        cg.add(var.set_qa_mode_enabled(False))
        
    if CONF_CO in config:
        sens = await sensor.new_sensor(config[CONF_CO])
        cg.add(var.set_co_sensor(sens))

    if CONF_SENSOR_FAULT in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_SENSOR_FAULT])
        cg.add(var.set_sensor_fault_binary(sens))
