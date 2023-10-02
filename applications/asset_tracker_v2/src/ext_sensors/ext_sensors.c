/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <stdio.h>
#include <string.h>
#include <zephyr/drivers/sensor.h>
#include <stdlib.h>
#include <math.h>

#include "ext_sensors.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(ext_sensors, CONFIG_EXTERNAL_SENSORS_LOG_LEVEL);

/* Convert to s/m2 depending on the maximum measured range used for adxl362. */
#if IS_ENABLED(CONFIG_ADXL362_ACCEL_RANGE_2G)
#define ADXL362_RANGE_MAX_M_S2 19.6133
#elif IS_ENABLED(CONFIG_ADXL362_ACCEL_RANGE_4G)
#define ADXL362_RANGE_MAX_M_S2 39.2266
#elif IS_ENABLED(CONFIG_ADXL362_ACCEL_RANGE_8G)
#define ADXL362_RANGE_MAX_M_S2 78.4532
#endif

/* This is derived from the sensitivity values in the datasheet. */
#define ADXL362_THRESHOLD_RESOLUTION_DECIMAL_MAX 2000

#if IS_ENABLED(CONFIG_ADXL362_ACCEL_ODR_12_5)
#define ADXL362_TIMEOUT_MAX_S 5242.88
#elif IS_ENABLED(CONFIG_ADXL362_ACCEL_ODR_25)
#define ADXL362_TIMEOUT_MAX_S 2621.44
#elif IS_ENABLED(CONFIG_ADXL362_ACCEL_ODR_50)
#define ADXL362_TIMEOUT_MAX_S 1310.72
#elif IS_ENABLED(CONFIG_ADXL362_ACCEL_ODR_100)
#define ADXL362_TIMEOUT_MAX_S 655.36
#elif IS_ENABLED(CONFIG_ADXL362_ACCEL_ODR_200)
#define ADXL362_TIMEOUT_MAX_S 327.68
#elif IS_ENABLED(CONFIG_ADXL362_ACCEL_ODR_400)
#define ADXL362_TIMEOUT_MAX_S 163.84
#endif

#define ADXL362_TIMEOUT_RESOLUTION_MAX 65536

/* Local accelerometer threshold value. Used to filter out unwanted values in
 * the callback from the accelerometer.
 */
double threshold = ADXL362_RANGE_MAX_M_S2;

struct env_sensor {
	enum sensor_channel channel;
	const struct device *dev;
	struct k_spinlock lock;
};

/** Sensor struct for the low-power accelerometer */
static struct env_sensor accel_sensor_lp = {
	.channel = SENSOR_CHAN_ACCEL_XYZ,
	.dev = DEVICE_DT_GET(DT_ALIAS(accelerometer)),
};

static struct sensor_trigger adxl362_sensor_trigger_motion = {
		.chan = SENSOR_CHAN_ACCEL_XYZ,
		.type = SENSOR_TRIG_MOTION
};

static struct sensor_trigger adxl362_sensor_trigger_stationary = {
		.chan = SENSOR_CHAN_ACCEL_XYZ,
		.type = SENSOR_TRIG_STATIONARY
};

static ext_sensor_handler_t evt_handler;

static void accelerometer_trigger_handler(const struct device *dev,
					  const struct sensor_trigger *trig)
{
	int err = 0;
	struct sensor_value data[ACCELEROMETER_CHANNELS];
	struct ext_sensor_evt evt = {0};

	switch (trig->type) {
	case SENSOR_TRIG_MOTION:
	case SENSOR_TRIG_STATIONARY:

		if (sensor_sample_fetch(dev) < 0) {
			LOG_ERR("Sample fetch error");
			return;
		}

		err = sensor_channel_get(dev, SENSOR_CHAN_ACCEL_XYZ, &data[0]);
		if (err) {
			LOG_ERR("sensor_channel_get, error: %d", err);
			return;
		}

		evt.value_array[0] = sensor_value_to_double(&data[0]);
		evt.value_array[1] = sensor_value_to_double(&data[1]);
		evt.value_array[2] = sensor_value_to_double(&data[2]);

		if (trig->type == SENSOR_TRIG_MOTION) {
			evt.type = EXT_SENSOR_EVT_ACCELEROMETER_ACT_TRIGGER;
			LOG_DBG("Activity detected");
		} else {
			evt.type = EXT_SENSOR_EVT_ACCELEROMETER_INACT_TRIGGER;
			LOG_DBG("Inactivity detected");
		}
		evt_handler(&evt);

		break;
	default:
		LOG_ERR("Unknown trigger: %d", trig->type);
	}
}

int ext_sensors_init(ext_sensor_handler_t handler)
{
	struct ext_sensor_evt evt = {0};

	if (handler == NULL) {
		LOG_ERR("External sensor handler NULL!");
		return -EINVAL;
	}

	evt_handler = handler;

	if (!device_is_ready(accel_sensor_lp.dev)) {
		LOG_ERR("Low-power accelerometer device is not ready");
		evt.type = EXT_SENSOR_EVT_ACCELEROMETER_ERROR;
		evt_handler(&evt);
	}

	return 0;
}

int ext_sensors_accelerometer_threshold_set(double threshold, bool upper)
{
	int err, input_value;
	double range_max_m_s2 = ADXL362_RANGE_MAX_M_S2;
	struct ext_sensor_evt evt = {0};

	if ((threshold > range_max_m_s2) || (threshold <= 0.0)) {
		LOG_ERR("Invalid %s threshold value: %f", upper?"activity":"inactivity", threshold);
		return -ENOTSUP;
	}

	/* Convert threshold value into 11-bit decimal value relative
	 * to the configured measuring range of the accelerometer.
	 */
	threshold = (threshold *
		(ADXL362_THRESHOLD_RESOLUTION_DECIMAL_MAX / range_max_m_s2));

	/* Add 0.5 to ensure proper conversion from double to int. */
	threshold = threshold + 0.5;
	input_value = (int)threshold;

	if (input_value >= ADXL362_THRESHOLD_RESOLUTION_DECIMAL_MAX) {
		input_value = ADXL362_THRESHOLD_RESOLUTION_DECIMAL_MAX - 1;
	} else if (input_value < 0) {
		input_value = 0;
	}

	const struct sensor_value data = {
		.val1 = input_value
	};

	enum sensor_attribute attr = upper ? SENSOR_ATTR_UPPER_THRESH : SENSOR_ATTR_LOWER_THRESH;

	/* SENSOR_CHAN_ACCEL_XYZ is not supported by the driver in this case. */
	err = sensor_attr_set(accel_sensor_lp.dev,
		SENSOR_CHAN_ACCEL_X,
		attr,
		&data);
	if (err) {
		LOG_ERR("Failed to set accelerometer threshold value");
		LOG_ERR("Device: %s, error: %d",
			accel_sensor_lp.dev->name, err);
		evt.type = EXT_SENSOR_EVT_ACCELEROMETER_ERROR;
		evt_handler(&evt);
		return err;
	}
	return 0;
}

int ext_sensors_inactivity_timeout_set(double inact_time)
{
	int err, inact_time_decimal;
	struct ext_sensor_evt evt = {0};

	if (inact_time > ADXL362_TIMEOUT_MAX_S || inact_time < 0) {
		LOG_ERR("Invalid timeout value");
		return -ENOTSUP;
	}

	inact_time = inact_time / ADXL362_TIMEOUT_MAX_S * ADXL362_TIMEOUT_RESOLUTION_MAX;
	inact_time_decimal = (int) (inact_time + 0.5);
	inact_time_decimal = MIN(inact_time_decimal, ADXL362_TIMEOUT_RESOLUTION_MAX);
	inact_time_decimal = MAX(inact_time_decimal, 0);

	const struct sensor_value data = {
		.val1 = inact_time_decimal
	};

	err = sensor_attr_set(accel_sensor_lp.dev,
			      SENSOR_CHAN_ACCEL_XYZ,
			      SENSOR_ATTR_HYSTERESIS,
			      &data);
	if (err) {
		LOG_ERR("Failed to set accelerometer inactivity timeout value");
		LOG_ERR("Device: %s, error: %d", accel_sensor_lp.dev->name, err);
		evt.type = EXT_SENSOR_EVT_ACCELEROMETER_ERROR;
		evt_handler(&evt);
		return err;
	}
	return 0;
}

int ext_sensors_accelerometer_trigger_callback_set(bool enable)
{
	int err;
	struct ext_sensor_evt evt = {0};

	sensor_trigger_handler_t handler = enable ? accelerometer_trigger_handler : NULL;

	err = sensor_trigger_set(accel_sensor_lp.dev, &adxl362_sensor_trigger_motion, handler);
	if (err) {
		goto error;
	}
	err = sensor_trigger_set(accel_sensor_lp.dev, &adxl362_sensor_trigger_stationary, handler);
	if (err) {
		goto error;
	}
		return 0;
error:
	LOG_ERR("Could not set trigger for device %s, error: %d",
		accel_sensor_lp.dev->name, err);
	evt.type = EXT_SENSOR_EVT_ACCELEROMETER_ERROR;
	evt_handler(&evt);
	return err;
}
