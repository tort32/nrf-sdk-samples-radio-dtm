/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/device.h>
#include <zephyr/sys/printk.h>

#include "transport/dtm_transport.h"

/* Re-entry into the Adafruit UF2 bootloader: hold sw0 at boot to request it.
 * Only built for boards that produce a UF2 image and have a sw0 button.
 */
#if defined(CONFIG_BUILD_OUTPUT_UF2) && defined(CONFIG_SOC_SERIES_NRF52X) && \
	DT_NODE_HAS_STATUS(DT_ALIAS(sw0), okay)
#include <nrf.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/reboot.h>

/* Button must stay pressed for this long before the bootloader is requested. */
#define DFU_BUTTON_HOLD_MS 50

/* Adafruit nRF52 bootloader DFU_MAGIC_UF2_RESET, read from GPREGRET at reset. */
#define ADAFRUIT_DFU_MAGIC_UF2_RESET 0x57

static const struct gpio_dt_spec dfu_button = GPIO_DT_SPEC_GET(DT_ALIAS(sw0), gpios);

static void dfu_button_check(void)
{
	int64_t start;

	if (!gpio_is_ready_dt(&dfu_button) ||
	    gpio_pin_configure_dt(&dfu_button, GPIO_INPUT) < 0) {
		return;
	}

	/* Let the pull-up settle. */
	k_msleep(1);

	start = k_uptime_get();
	while (gpio_pin_get_dt(&dfu_button) > 0) {
		if ((k_uptime_get() - start) > DFU_BUTTON_HOLD_MS) {
			printk("DFU requested\n");
			NRF_POWER->GPREGRET = ADAFRUIT_DFU_MAGIC_UF2_RESET;
			sys_reboot(SYS_REBOOT_COLD);
		}
		k_msleep(1);
	}
}
#else
static inline void dfu_button_check(void)
{
}
#endif

int main(void)
{
	int err;
	union dtm_tr_packet cmd;

	dfu_button_check();
	
	printk("Starting Direct Test Mode sample\n");

	err = dtm_tr_init();
	if (err) {
		printk("Error initializing DTM transport: %d\n", err);
		return err;
	}

	for (;;) {
		cmd = dtm_tr_get();
		err = dtm_tr_process(cmd);
		if (err) {
			printk("Error processing command: %d\n", err);
			return err;
		}
	}
}
