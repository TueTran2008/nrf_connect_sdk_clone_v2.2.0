
#include <zephyr/kernel.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <hal/nrf_gpio.h>
#include <string.h>
#include <stdlib.h>
#include <zephyr/device.h>
#include <stdbool.h>
#include "hw_output.h"
#include "macros_common.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(hw_output, 5);

#define DT_LABEL_AND_COMMA(node_id) DT_PROP(node_id, label),
#define GPIO_DT_SPEC_GET_AND_COMMA(node_id) GPIO_DT_SPEC_GET(node_id, gpios),

/* The following arrays are populated compile time from the .dts*/
static const char *const output_names[] = {DT_FOREACH_CHILD(DT_PATH(inout), DT_LABEL_AND_COMMA)};

static const struct gpio_dt_spec outputs[] = {DT_FOREACH_CHILD(DT_PATH(inout), GPIO_DT_SPEC_GET_AND_COMMA)};

static bool initialized = false;
static uint8_t outputs_num = 0;


int hw_output_init()
{
	if (initialized) {
        LOG_WRN("hw output already is initialized\r\n");
		return -EPERM;
	}
    outputs_num = ARRAY_SIZE(outputs);
    for(uint8_t output_index = 0; output_index < outputs_num; output_index++)
    {
        if (!device_is_ready(outputs[output_index].port)) {
            LOG_ERR("LED GPIO controller not ready:%s", output_names[output_index]);
            return -ENODEV;
        }
        gpio_pin_configure_dt(&outputs[output_index], GPIO_OUTPUT_INACTIVE | GPIO_OUTPUT| GPIO_ACTIVE_HIGH);
    }
    initialized = true;
    return 0;
}
int hw_output_set_level(gpio_output_t output_pin, int level)
{
    int ret = 0;
    ret = gpio_pin_set_dt(&outputs[output_pin], level);
    if(ret)
    {
        LOG_ERR("Fail to set :%s state to :%d\r\n", output_names[output_pin], level);
        return ret;
    }
    return ret;
}
bool hw_output_get_level(gpio_output_t output_pin)
{
    return gpio_pin_get_dt(&outputs[output_pin]);
}
