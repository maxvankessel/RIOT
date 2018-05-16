/*
 * Copyright (C) 2015 José Ignacio Alamos <jialaos@uc.cl>
 *
 * This file is subject to the terms and conditions of the GNU Lesser General
 * Public License v2.1. See the file LICENSE in the top level directory for
 * more details.
 */

/**
 * @defgroup    net_hdlc_hdr    HDLC header
 * @ingroup     net_hdlc
 * @brief       HDLC header architecture
 *
 * @{
 *
 * @file
 * @brief       Definitions for HDLC header
 *
 * @author      José Ignacio Alamos <jialamos@uc.cl>
 */
#ifndef HDLC_HDR_H
#define HDLC_HDR_H

#include <stdint.h>
#include "byteorder.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Data type to represent an HDLC header.
 *
 *  +----------+----------+----------+
 *  |   Flag   | Address  | Control  |
 *  | 01111110 | 11111111 | 00000011 |
 *  +----------+----------+----------+
 *  +--------------------------------+
 *  |              Data              |
 *  |              8 x n             |
 *  +--------------------------------+
 *  +----------+----------+-----------------
 *  |   FCS    |   Flag   | Inter-frame Fill
 *  |16/32 bits| 01111110 | or next Address
 *  +----------+----------+-----------------
 *
 */
typedef struct __attribute__((packed)) {
    uint8_t address;            /**< Source address field of HDLC header */
    uint8_t control;            /**< Control field of HDLC header */
} hdlc_hdr_t;

#ifdef __cplusplus
}
#endif

#endif /* HDLC_HDR_H */
/** @} */
