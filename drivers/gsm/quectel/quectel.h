/*
 * Copyright (C) 2018 Max van Kessel <maxvankessel@betronic.nl>
 *
 * This file is subject to the terms and conditions of the GNU Lesser General
 * Public License v2.1. See the file LICENSE in the top level directory for more
 * details.
 */

/**
 * @defgroup    drivers_gsm_quectel Quectel
 * @ingroup     drivers_gsm
 * @brief       A generic implementation for the Quectel gsm modules
 *
 * @{
 *
 * @file
 * @brief
 *
 * @author  Max van Kessel <maxvankessel@betronic.nl>
 */

#ifndef QUECTEL_H
#define QUECTEL_H

#include <stdint.h>
#include <stdbool.h>

#include "gpio.h"
#include "gsm.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef QUECTEL_POWER_KEY_ON_TIME_US
#define QUECTEL_POWER_KEY_ON_TIME_US            (2000 * US_PER_MS)
#endif

#ifndef QUECTEL_STATUS_DETECT_TIMEOUT_US
#define QUECTEL_STATUS_DETECT_TIMEOUT_US       (1000 * US_PER_MS)
#endif

typedef struct gsm_quectel_params {
    gsm_params_t base;              /**< gsm base parameters */
    gpio_t      power_pin;          /**< quectel power pin*/
    bool        invert_power_pin;   /**< select inversion of power pin */
    gpio_t      status_pin;         /**< quectel status pin (modem output) */
    bool        invert_status_pin;  /**< select inversion of status pin */
    gpio_t      reset_pin;          /**< quectel reset pin*/
    bool        invert_reset_pin;   /**< select inversion of reset pin*/
    gpio_t      dtr_pin;            /**< quectel dtr pin (modem intput) */
    gpio_t      dcd_pin;            /**< quectel dcd pin (modem output) */
} gsm_quectel_params_t;

typedef struct gsm_quectel {
    gsm_t base;

} gsm_quectel_t;




extern const gsm_generic_driver_t gsm_quectel_driver;


#ifdef __cplusplus
}
#endif

#endif /* QUECTEL_H */
