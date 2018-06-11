/*
 * Copyright (C) 2018 Max van Kessel
 *
 * This file is subject to the terms and conditions of the GNU Lesser General
 * Public License v2.1. See the file LICENSE in the top level directory for more
 * details.
 */

/**
 * @defgroup    drivers_gsm_ppp Point to Point (over Serial)
 * @ingroup     drivers_gsm
 * @brief       A generic implementation of the GSM PPP(oS)
 *
 * @{
 *
 * @file
 * @brief   GSM-independent PPPoS driver
 *
 * @author  Max van Kessel
 */
#ifndef PPP_INTERNAL_H
#define PPP_INTERNAL_H

#include "gsm.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Handle for gsm point to point layer
 *
 * @param[in] dev   device to operate on
 */
void gsm_ppp_handle(gsm_t *dev);

#ifdef __cplusplus
}
#endif

#endif /* PPP_INTERNAL_H */
