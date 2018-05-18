/*
 * Copyright (C) 2016 José Ignacio Alamos <jialamos@uc.cl>
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @defgroup    net_gnrc_ppp Definitions of LCP
 * @ingroup     net_gnrc_ppp
 * @{
 *
 * @file
 * @brief  Definitions and configuration of Link Control Protocol
 *
 * @author  José Ignacio Alamos <jialamos@uc.cl>
 */
#ifndef GNRC_PPP_LCP_H
#define GNRC_PPP_LCP_H

#include "net/gnrc/ppp/fsm.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GNRC_PPP_LCP_OPT_MRU (1)                 /**< code for MRU */
#define GNRC_PPP_LCP_OPT_ACCM (2)                /**< code for ACCM */
#define GNRC_PPP_LCP_OPT_AUTH (3)                /**< code for auth */

#define GNRC_PPP_LCP_MAX_MRU (2000)              /**< max MRU of ppp device */
#define GNRC_PPP_LCP_DEFAULT_MRU (1500)          /**< default value of MRU */
#define GNRC_PPP_LCP_DEFAULT_ACCM (0xFFFFFFFF)   /**< default value of Async Control Character Map */
#define GNRC_PPP_LCP_DEFAULT_AUTH (PPPTYPE_PAP)  /**< default authentication method */

#define GNRC_PPP_LCP_RESTART_TIMER (3000000U)    /**< value of restart time for LCP */


/**
 * @brief LCP options
 */
typedef enum {
    LCP_MRU,    /**< Maximum Received Unit option */
    LCP_ACCM,   /**< Async Control Character Map option */
    LCP_AUTH,   /**< Authentication method option */
    LCP_NUMOPTS
} lcp_options_t;


/**
 * @brief definition of LCP protocol
 * @extends ppp_fsm_t
 */
typedef struct gnrc_ppp_lcp {
    gnrc_ppp_fsm_t fsm;                      /**< base fsm class */
    gnrc_ppp_fsm_conf_t lcp_opts[LCP_NUMOPTS];   /**< configuration options for LCP */
    uint8_t pr_id;                      /** last protocol reject id */
    uint16_t peer_mru;                  /**< MRU of peer */
    uint16_t mru;                       /**< MRU of ppp device */
    uint8_t local_auth;                 /**< local authentication method */
    uint8_t remote_auth;                /**< peer authentication method */
    uint8_t monitor_id;                 /**< last monitor packet id*/
} gnrc_ppp_lcp_t;

struct gnrc_ppp_fsm_t;

/**
 * @brief init function for LCP protocol
 *
 * @param ppp_dev poitner to gnrc ppp interface
 * @param lcp pointer to lcp protocol
 *
 * @return 0
 */
int lcp_init(gnrc_netdev2_t *ppp_dev);
gnrc_ppp_protocol_t *lcp_get_static_pointer(void);

#ifdef __cplusplus
}
#endif

#endif /* GNRC_PPP_LCP_H */
/** @} */