/***************************************************************************//**
 *   @file    app_config.c
 *   @brief   Application configurations module (platform-agnostic)
 *   @details This module performs the system configurations
********************************************************************************
 * Copyright (c) 2024 Analog Devices, Inc.
 * All rights reserved.
 *
 * This software is proprietary to Analog Devices, Inc. and its licensors.
 * By using this software you agree to the terms of the associated
 * Analog Devices Software License Agreement.
*******************************************************************************/

/******************************************************************************/
/***************************** Include Files **********************************/
/******************************************************************************/

#include <stdbool.h>
#include "app_config.h"
#include "no_os_util.h"
#include "common.h"
#include "no_os_error.h"
#include "no_os_uart.h"
#include "no_os_gpio.h"
#include "no_os_i2c.h"
#include "no_os_eeprom.h"
#include "no_os_irq.h"
#include "no_os_pwm.h"

/******************************************************************************/
/************************ Macros/Constants ************************************/
/******************************************************************************/

/******************************************************************************/
/******************** Variables and User Defined Data Types *******************/
/******************************************************************************/

/* UART init parameters for IIO comm port */
struct no_os_uart_init_param uart_iio_comm_init_params = {
	.device_id = 0,
	.baud_rate = IIO_UART_BAUD_RATE,
	.size = NO_OS_UART_CS_8,
	.parity = NO_OS_UART_PAR_NO,
	.stop = NO_OS_UART_STOP_1_BIT,
#if defined(USE_VIRTUAL_COM_PORT)
	.platform_ops = &vcom_ops,
	.extra = &vcom_extra_init_params
#else
	.platform_ops = &uart_ops,
	.extra = &uart_extra_init_params
#endif
};

/* UART init parameters for console comm port */
struct no_os_uart_init_param uart_console_stdio_init_params = {
	.device_id = 0,
	.baud_rate = IIO_UART_BAUD_RATE,
	.size = NO_OS_UART_CS_8,
	.parity = NO_OS_UART_PAR_NO,
	.stop = NO_OS_UART_STOP_1_BIT,
#if defined(USE_VIRTUAL_COM_PORT)
	/* If virtual com port is primary IIO comm port, use physical port for stdio
	 * console. Applications which does not support VCOM, should not satisfy this
	 * condition */
	.platform_ops = &uart_ops,
	.extra = &uart_extra_init_params
#else
#if defined(CONSOLE_STDIO_PORT_AVAILABLE)
	/* Applications which uses phy COM port as primary IIO comm port,
	 * can use VCOM as console stdio port provided it is available.
	 * Else, alternative phy com port can be used for console stdio ops if available */
	.platform_ops = &vcom_ops,
	.extra = &vcom_extra_init_params
#endif
#endif
};

/* Trigger GPIO IRQ parameters */
struct no_os_irq_init_param trigger_gpio_irq_params = {
	.irq_ctrl_id = 0,
	.platform_ops = &trigger_gpio_irq_ops,
	.extra = &trigger_gpio_irq_extra_params
};

/* PWM init parameters */
static struct no_os_pwm_init_param pwm_init_params = {
	.id = 0,
	.period_ns = CONV_TRIGGER_PERIOD_NSEC(MAX_SAMPLING_RATE),
	.duty_cycle_ns = CONV_TRIGGER_DUTY_CYCLE_NSEC(MAX_SAMPLING_RATE),
	.platform_ops = &pwm_ops,
	.extra = &pwm_extra_init_params
};

/* I2C init parameters */
static struct no_os_i2c_init_param no_os_i2c_init_params = {
	.device_id = 0,
	.platform_ops = &i2c_ops,
	.max_speed_hz = 100000,
	.extra = &i2c_extra_init_params
};

/* LDAC GPIO init parameters */
struct no_os_gpio_init_param ldac_gpio_params = {
	.number = LDAC_GPIO,
	.pull = NO_OS_PULL_NONE,
	.platform_ops = &dac_gpio_ops,
	.extra = &ldac_gpio_extra_init_params
};

/* CLEAR GPIO init parameters */
struct no_os_gpio_init_param clear_gpio_params = {
	.number = CLEAR_GPIO,
	.pull = NO_OS_PULL_NONE,
	.platform_ops = &dac_gpio_ops,
	.extra = &clear_gpio_extra_init_params
};

/* EEPROM init parameters */
static struct eeprom_24xx32a_init_param eeprom_extra_init_params = {
	.i2c_init = &no_os_i2c_init_params
};

/* EEPROM init parameters */
static struct no_os_eeprom_init_param eeprom_init_params = {
	.device_id = 0,
	.platform_ops = &eeprom_24xx32a_ops,
	.extra = &eeprom_extra_init_params
};

/* UART IIO Descriptor */
struct no_os_uart_desc *uart_iio_com_desc;

/* UART console descriptor */
struct no_os_uart_desc *uart_console_stdio_desc;

/* EEPROM descriptor */
struct no_os_eeprom_desc *eeprom_desc;

/* PWM descriptor */
struct no_os_pwm_desc *pwm_desc;

/* Trigger GPIO descriptor */
struct no_os_gpio_desc *trigger_gpio_desc;

/* Trigger GPIO interrupt descriptor */
struct no_os_irq_ctrl_desc *trigger_irq_desc;

/******************************************************************************/
/************************** Functions Declarations ****************************/
/******************************************************************************/

/******************************************************************************/
/************************** Functions Definitions *****************************/
/******************************************************************************/

/**
 * @brief 	Initialize the UART peripheral
 * @return	0 in case of success, negative error code otherwise
 */
static int32_t init_uart(void)
{
	int32_t ret;

	ret = no_os_uart_init(&uart_iio_com_desc, &uart_iio_comm_init_params);
	if (ret) {
		return ret;
	}

#if defined(CONSOLE_STDIO_PORT_AVAILABLE)
	/* Initialize the serial link for console stdio communication */
	ret = no_os_uart_init(&uart_console_stdio_desc,
			      &uart_console_stdio_init_params);
	if (ret) {
		return ret;
	}
#endif

	return 0;
}

/**
 * @brief 	Initialize the trigger GPIO and associated IRQ event
 * @return	0 in case of success, negative error code otherwise
 */
static int32_t gpio_trigger_init(void)
{
	int32_t ret;

	/* Initialize the IRQ controller */
	ret = no_os_irq_ctrl_init(&trigger_irq_desc, &trigger_gpio_irq_params);
	if (ret) {
		return ret;
	}

	return 0;
}

/**
 * @brief 	Initialize the PWM interface.
 * @return	0 in case of success, negative error code otherwise.
 */
int32_t init_pwm(void)
{
	int32_t ret;

	/* Initialize the PWM interface to generate PWM signal
	 * for recurring function calls */
	ret = no_os_pwm_init(&pwm_desc, &pwm_init_params);
	if (ret) {
		return ret;
	}

	/* Disable the PWM interface.*/
	ret = no_os_pwm_disable(pwm_desc);
	if (ret) {
		return ret;
	}

	return 0;
}

/**
 * @brief 	Initialize the system peripherals
 * @return	0 in case of success, negative error code otherwise
 */
int32_t init_system(void)
{
	int32_t ret;

	ret = init_uart();
	if (ret) {
		return ret;
	}

	ret = gpio_trigger_init();
	if (ret) {
		return ret;
	}

#if defined(USE_SDRAM)
	ret = sdram_init();
	if (ret) {
		return ret;
	}
#endif

	ret = eeprom_init(&eeprom_desc, &eeprom_init_params);
	if (ret) {
		return ret;
	}

	return 0;
}