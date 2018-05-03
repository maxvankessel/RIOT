/*
 * Copyright (C) 2018 Max van Kessel
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @defgroup    drivers_pppos Point to Point Protocol over Serial network device
 * @ingroup     drivers_netdev
 * @brief       PPPoS network device over @ref drivers_periph_uart
 * @note        PPPoS uses HDLC like framing
 * @{
 *
 * @file
 * @brief   PPPoS device definitions
 *
 * @author  Max van Kessel
 */
#ifndef PPPOS_H
#define PPPOS_H

#include <stdint.h>

#include "cib.h"
#include "net/netdev.h"
#include "periph/uart.h"
#include "tsrb.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief   UART buffer size used for TX and RX buffers
 *
 * Reduce this value if your expected traffic does not include full IPv6 MTU
 * sized packets.
 *
 * @pre Needs to be power of two and `<= INT_MAX`
 */
#ifndef PPPOS_BUFSIZE
#define PPPOS_BUFSIZE (2048U)
#endif

#ifndef PPPOS_MAX_IDLE_TIME_MS
#define PPPOS_MAX_IDLE_TIME_MS (100 * US_PER_MS)
#endif

enum {
    PPP_RX_IDLE = 0,
    PPP_RX_STARTED,
    PPP_RX_ADDRESS,
    PPP_RX_CONTROL,
    PPP_RX_PROTOCOL,
    PPP_RX_DATA,
    PPP_RX_FINISHED,
};

/**
 * @brief   Configuration parameters for PPP over Serial
 */
typedef struct {
    uart_t uart;        /**< UART interface the device is connected to */
    uint32_t baudrate;  /**< baudrate to use with pppos_params_t::uart */
    gpio_t ring;        /**< ring indicator */
    gpio_t dcd;         /**< data carrier detect indicator */
    gpio_t dtr;         /**< data terminal ready indicator */
} pppos_params_t;

/**
 * @brief   Device descriptor for PPP over Serial
 *
 * @extends netdev_t
 */
typedef struct {
    netdev_t netdev;                        /**< parent class */
    pppos_params_t config;                  /**< configuration parameters */
    tsrb_t inbuf;                           /**< RX buffer */
    char rxmem[PPPOS_BUFSIZE];              /**< memory used by RX buffer */

    uint16_t fcs;
    uint8_t esc;
    uint16_t prot;
    uint8_t state;

    struct {
        uint32_t rx;
        uint32_t tx;
    } accm;

    uint32_t last_xmit;

} pppos_t;

/**
 * @brief   Setup a PPP over serial device
 *
 * @param[in] dev       device descriptor
 * @param[in] params    parameters for device initialization
 */
void pppos_setup(pppos_t *dev, const pppos_params_t *params);

#ifdef __cplusplus
}
#endif

#endif /* PPPOS_H */
/** @} */
