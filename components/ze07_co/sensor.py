import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, uart
from esphome.const import (
    CONF_ID,
    CONF_UPDATE_INTERVAL,
    DEVICE_CLASS_CARBON_MONOXIDE,
    STATE_CLASS_MEASUREMENT,
    UNIT_PARTS_PER_MILLION,
)

CONF_CO = "co"
CONF_MODE = "mode"

DEPENDENCIES = ["uart"]

ze07_co_ns = cg.esphome_ns.namespace("ze07_co")
ZE07COSensor = ze07_co_ns.class_("ZE07COSensor", cg.Component, uart.UARTDevice)
ZE07COMode = ze07_co_ns.enum("ZE07COMode")

MODES = {
    "PASSIVE": ZE07COMode.MODE_PASSIVE,
    "QA": ZE07COMode.MODE_QA,
}

def validate_update_interval(config):
    """Validate update interval for Q&A mode"""
    if config.get(CONF_MODE) == "QA" and CONF_UPDATE_INTERVAL in config:
        interval = config[CONF_UPDATE_INTERVAL]
        # Get milliseconds value from TimePeriodMilliseconds object
        interval_ms = interval.total_milliseconds
        if interval_ms < 30000:  # 30 seconds in milliseconds
            # Convert back to seconds for display
            interval_seconds = interval_ms / 1000
            raise cv.Invalid(
                f"update_interval must be at least 30s for Q&A mode. "
                f"You set {interval_seconds}s. "
                "The sensor needs time to warm up and take accurate readings."
            )
    return config

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(ZE07COSensor),
            cv.Optional(CONF_MODE, default="PASSIVE"): cv.enum(MODES, upper=True),
            cv.Optional(CONF_UPDATE_INTERVAL): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_CO): sensor.sensor_schema(
                unit_of_measurement=UNIT_PARTS_PER_MILLION,
                icon="mdi:molecule-co",
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_CARBON_MONOXIDE,
                state_class=STATE_CLASS_MEASUREMENT,
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

    # Set mode first
    cg.add(var.set_mode(config[CONF_MODE]))

    # Handle update interval
    if config[CONF_MODE] == "QA":
        # For Q&A mode, use configured interval or default 60s
        if CONF_UPDATE_INTERVAL in config:
            interval = config[CONF_UPDATE_INTERVAL]
        else:
            interval = 60000  # Default 60 seconds in milliseconds
        interval_ms = max(30000, int(interval.total_milliseconds if hasattr(interval, 'total_milliseconds') else interval))
        cg.add(var.set_update_interval(interval_ms))
    # For PASSIVE mode, update_interval is ignored (sensor sends data every second)
        
    if CONF_CO in config:
        sens = await sensor.new_sensor(config[CONF_CO])
        cg.add(var.set_co_sensor(sens))
