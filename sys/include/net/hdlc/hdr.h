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

typedef enum {
    HDLC_TYPE_RECEIVE_READY     = 0,
    HDLC_TYPE_RECEIVE_NOT_READY = (1 << 1),
    HDLC_TYPE_REJECT            = (1 << 0),
    HDLC_TYPE_SELECTIVE_REJECT  = (3 << 0),
} hdlc_type_t;

typedef enum {
    HDLC_FRAME_TYPE_INFORMATION = 0,
    HDLC_FRAME_TYPE_SUPERVISORY = 1,
    HDLC_FRAME_TYPE_UNNUMBERED = 3,
} hdlc_frame_type_t;

/**
 * @brief HDLC Supervisory Frames
 */
typedef struct __attribute__((packed)) {
    uint8_t id : 2;
    hdlc_type_t type : 2;
    uint8_t poll_final : 1;
    uint8_t sequence_no : 3;
} hdlc_control_s_frame_t;

/**
 * @brief HDLC Information Frames
 */
typedef struct __attribute__((packed)) {
    uint8_t id : 1;
    uint8_t send_sequence_no : 3;
    uint8_t poll_final : 1;
    uint8_t sequence_no : 3;
} hdlc_control_i_frame_t;

/**
 * @brief HDLC Unnumbered Frames
 */
typedef struct __attribute__((packed)) {
    uint8_t id : 2;
    hdlc_type_t type : 2;
    uint8_t poll_final : 1;
    hdlc_type_t type_x : 3;
} hdlc_control_u_frame_t;

/**
 * @brief Data type to represent an HDLC header.
 */
typedef struct __attribute__((packed)) {
    uint8_t address;            /**< Address field oh HDLC header */
    union{
        hdlc_control_s_frame_t s;            /**< Control field of HDLC header */
        hdlc_control_i_frame_t i;
        hdlc_control_u_frame_t u;
        uint8_t frame;
    }control;
    network_uint16_t protocol;  /**< Protocol field of HDLC header */
} hdlc_hdr_t;

#ifdef __cplusplus
}
#endif

#endif /* HDLC_HDR_H */
/** @} */
