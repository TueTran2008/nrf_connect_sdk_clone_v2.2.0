/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 * BYTECH-JSC
 * TUETD modify":D
 */

#include "led.h"

#include <zephyr/kernel.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <hal/nrf_gpio.h>
#include <string.h>
#include <stdlib.h>
#include <zephyr/device.h>

#include "macros_common.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(led, CONFIG_MODULE_LED_LOG_LEVEL);

#define BLINK_FREQ_MS 1000
/* Maximum number of LED_UNITS. 1 RGB LED = 1 UNIT of 3 LEDS */
#define LED_UNIT_MAX 2
#define NUM_COLORS_RGB 3
#define BASE_10 10
#define DT_LABEL_AND_COMMA(node_id) DT_PROP(node_id, label),
#define GPIO_DT_SPEC_GET_AND_COMMA(node_id) GPIO_DT_SPEC_GET(node_id, gpios),

/* The following arrays are populated compile time from the .dts*/
static const char *const led_labels[] = {DT_FOREACH_CHILD(DT_PATH(user_leds), DT_LABEL_AND_COMMA) };

static const struct gpio_dt_spec leds[] = { DT_FOREACH_CHILD(DT_PATH(user_leds),
							     GPIO_DT_SPEC_GET_AND_COMMA) };

enum led_type {
	LED_MONOCHROME,
	LED_COLOR,
};

struct user_config {
	bool blink;
	bool current_status;
	/*Control blink speed*/
	uint8_t blink_duration; 
	uint8_t blink_current_tick;
	enum led_color color;
};

struct led_unit_cfg {
	uint8_t led_no;
	enum led_type unit_type;
	union {
		const struct gpio_dt_spec *mono;
		const struct gpio_dt_spec *color[NUM_COLORS_RGB];
	} type;
	struct user_config user_cfg;
};

static uint8_t leds_num;
static bool initialized;
static struct led_unit_cfg led_units[LED_UNIT_MAX];

/**
 * @brief Configures fields for a RGB LED
 */
static int configure_led_color(uint8_t led_unit, uint8_t led_color, uint8_t led)
{
	if (!device_is_ready(leds[led].port)) {
		LOG_ERR("LED GPIO controller not ready");
		return -ENODEV;
	}

	led_units[led_unit].type.color[led_color] = &leds[led];
	led_units[led_unit].unit_type = LED_COLOR;

	return gpio_pin_configure_dt(led_units[led_unit].type.color[led_color],
				     GPIO_OUTPUT_INACTIVE);
}

/**
 * @brief Configures fields for a monochrome LED
 */
static int config_led_monochrome(uint8_t led_unit, uint8_t led)
{
	if (!device_is_ready(leds[led].port)) {
		LOG_ERR("LED GPIO controller not ready");
		return -ENODEV;
	}

	led_units[led_unit].type.mono = &leds[led];
	led_units[led_unit].unit_type = LED_MONOCHROME;
	led_units[led_unit].user_cfg.blink_duration = 1;
	led_units[led_unit].user_cfg.blink_current_tick = 0;
	led_units[led_unit].user_cfg.current_status = false;
	return gpio_pin_configure_dt(led_units[led_unit].type.mono, GPIO_OUTPUT_INACTIVE);
}

/**
 * @brief Parses the device tree for LED settings.
 */
static int led_device_tree_parse(void)
{
	/*Config all leds as monochrome:D*/
	int ret;
	for (uint8_t i = 0; i < leds_num; i++) {
		char *end_ptr = NULL;
		//uint32_t led_unit = strtoul(led_labels[i], &end_ptr, BASE_10);
		// if (led_labels[i] == end_ptr) {
		// 	LOG_ERR("No match for led unit. The dts is likely not properly formatted");
		// 	return -ENXIO;
		// }
		ret = config_led_monochrome(i, i);
		LOG_INF("Config led: %s", led_labels[i]);
		if (ret) {			
			return ret;
		}
		// else{

		// }
	}
	return 0;
}

/**
 * @brief Internal handling to set the status of a led unit
 */
static int led_set_int(uint8_t led_unit, enum led_color color)
{
	int ret;

	if (led_units[led_unit].unit_type == LED_MONOCHROME) {
		if (color) {
			ret = gpio_pin_set_dt(led_units[led_unit].type.mono, 1);
			if (ret) {
				return ret;
			}
		} else {
			ret = gpio_pin_set_dt(led_units[led_unit].type.mono, 0);
			if (ret) {
				return ret;
			}
		}
	}
	return 0;
}

static void led_blink_work_handler(struct k_work *work);

K_WORK_DEFINE(led_blink_work, led_blink_work_handler);

/**
 * @brief Submit a k_work on timer expiry.
 */
void led_blink_timer_handler(struct k_timer *dummy)
{
	k_work_submit(&led_blink_work);
}

K_TIMER_DEFINE(led_blink_timer, led_blink_timer_handler, NULL);

/**
 * @brief Periodically invoked by the timer to blink LEDs.
 */
static void led_blink_work_handler(struct k_work *work)
{
	int ret;
	//static bool on_phase;

	for (uint8_t i = 0; i < leds_num; i++) {
		if (led_units[i].user_cfg.blink) {
			if (led_units[i].user_cfg.current_status == false) {
				if(led_units[i].user_cfg.blink_current_tick++ >= led_units[i].user_cfg.blink_duration)
				{
					led_units[i].user_cfg.blink_current_tick = 0;
					led_units[i].user_cfg.current_status = true; 
					ret = led_set_int(i, LED_COLOR_RED);
					ERR_CHK(ret);
				}

			}else {
				led_units[i].user_cfg.blink_current_tick = 0;
				led_units[i].user_cfg.current_status = false; 
				ret = led_set_int(i, LED_COLOR_OFF);
				ERR_CHK(ret);
			}
		}
	}

	//on_phase = !on_phase;
}

static int led_set(uint8_t led_unit, enum led_color color, bool blink)
{
	int ret;

	if (!initialized) {
		return -EPERM;
	}

	ret = led_set_int(led_unit, color);
	if (ret) {
		return ret;
	}

	led_units[led_unit].user_cfg.blink = blink;
	led_units[led_unit].user_cfg.color = color;

	return 0;
}

int led_on(uint8_t led_unit, ...)
{
	if (led_units[led_unit].unit_type == LED_MONOCHROME) {
		return led_set(led_unit, LED_ON, LED_SOLID);
	}
	else
	{
		LOG_ERR("Failed to set LED\r\n");
	}
}

int led_blink(uint8_t led_unit, uint8_t on_off_duration)
{
	if (led_units[led_unit].unit_type == LED_MONOCHROME) {
		return led_set(led_unit, LED_ON, LED_BLINK);
	}
	else
	{
		LOG_ERR("Failed to set led blink\r\n");
	}
}

int led_off(uint8_t led_unit)
{
	return led_set(led_unit, LED_COLOR_OFF, LED_SOLID);
}

int led_init(void)
{
	int ret;

	if (initialized) {
		return -EPERM;
	}

	__ASSERT(ARRAY_SIZE(leds) != 0, "No LEDs found in dts");

	leds_num = ARRAY_SIZE(leds);

	ret = led_device_tree_parse();
	if (ret) {
		return ret;
	}

	k_timer_start(&led_blink_timer, K_MSEC(BLINK_FREQ_MS / 10), K_MSEC(BLINK_FREQ_MS / 10));
	initialized = true;
	return 0;
}
