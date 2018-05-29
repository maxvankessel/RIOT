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
#ifndef GSM_PPP_H
#define GSM_PPP_H

#include <stdint.h>

#include "gsm.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Setup netdev for gsm point to point layer
 *
 * @param[in] dev   device to operate on
 */
void gsm_ppp_setup(gsm_t *dev);

#ifdef __cplusplus
}
#endif

#endif /* GSM_PPP_H */
