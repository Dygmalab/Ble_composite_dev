/**
 * Copyright (c) 2012 - 2021, Nordic Semiconductor ASA
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form, except as embedded into a Nordic
 *    Semiconductor ASA integrated circuit in a product or a software update for
 *    such product, must reproduce the above copyright notice, this list of
 *    conditions and the following disclaimer in the documentation and/or other
 *    materials provided with the distribution.
 *
 * 3. Neither the name of Nordic Semiconductor ASA nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * 4. This software, with or without modification, must only be used with a
 *    Nordic Semiconductor ASA integrated circuit.
 *
 * 5. Any software provided in binary form under this license must not be reverse
 *    engineered, decompiled, modified and/or disassembled.
 *
 * THIS SOFTWARE IS PROVIDED BY NORDIC SEMICONDUCTOR ASA "AS IS" AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY, NONINFRINGEMENT, AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL NORDIC SEMICONDUCTOR ASA OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

/*
 * Manage the low level Bluetooth low energy communication between the Neuron 2
 * and the computer host.
 * Copyright© 2026  Dygma Lab S.L.
 *
 * Configuration guidelines obtained from Nordic bolierplates for BT communications
 * SDK Version: nRF5_SDK_17.1.0
 */


#include "app_error.h"
#include "app_scheduler.h"
#include "app_timer.h"

//#include "ble.h"
//#include "ble_advdata.h"
#include "ble_advertising.h"
#include "ble_bas.h"
#include "ble_conn_params.h"
//#include "ble_conn_state.h"
#include "ble_dis.h"
//#include "ble_dtm.h"
//#include "ble_err.h"
//#include "ble_gap.h"
//#include "ble_hci.h"
//#include "ble_hids.h"
//#include "ble_srv_common.h"
//#include "fds.h"
#include "nrf_ble_gatt.h"
#include "nrf_ble_qwr.h"
#include "nrf_sdh.h"
#include "nrf_sdh_ble.h"
//#include "nrf_sdh_soc.h"
//#include "nrf_pwr_mgmt.h"
#include "peer_manager.h"
#include "peer_manager_handler.h"

#include "Ble_composite_dev.h"
#include "ble_hid_service.h"

//#include "nrf_log.h"
//#include "nrf_log_ctrl.h"
//#include "nrf_log_default_backends.h"


//#ifndef BLE_DEVICE_NAME
//#error "BLE_DEVICE_NAME is not defined"
//#endif /* BLE_DEVICE_NAME */

/* nRF-level EXIT macro */
#define EXIT_IF_ERR_NRF( nrf_err, err, msg ) do{ err = ( nrf_err != NRF_SUCCESS ) ? RESULT_ERR : RESULT_OK; \
                                                 EXIT_IF_ERR( err, msg ); } while(0);

#define BLE_OBSERVER_PRIO                   3               /* Application's BLE observer priority. You shouldn't need to modify this value. */
#define BLE_CONN_CFG_TAG                    1               /* A tag identifying the SoftDevice BLE configuration. */

#define BLE_TX_POWER                        4               /* +4dBm */

#define DIS_MANUFACTURER_NAME               "Dygma Lab"     /* Manufacturer. Will be passed to Device Information Service. */

#define DIS_PNP_ID_VENDOR_ID_SOURCE         0x02            /* Vendor ID Source. */
#define DIS_PNP_ID_VENDOR_ID                BOARD_VENDORID  /* Vendor ID (Defined in Makefile)*/
#define DIS_PNP_ID_PRODUCT_ID               BOARD_PRODUCTID /* Product ID (Defined in Makefile)*/
#define DIS_PNP_ID_PRODUCT_VERSION          0x0001          /* Product Version. */

/* Advertising definitions */
#define ADV_FAST_INTERVAL                   MSEC_TO_UNITS(25, UNIT_0_625_MS)    /* Fast advertising interval (25 ms). */
#define ADV_SLOW_INTERVAL                   MSEC_TO_UNITS(2000, UNIT_0_625_MS)  /* Slow advertising interval (2000 ms). */

#define ADV_FAST_TIMEOUT                    MSEC_TO_UNITS(30000, UNIT_10_MS)    /* The advertising duration of fast advertising (30s). */
#warning "Check the reason for having slow advertising duration set to 1 == 10ms. What happens if we set some standard value like 180s"
#define ADV_SLOW_TIMEOUT                    1

#define ADV_UUIDS_CNT                       (sizeof(adv_uuids) / sizeof(ble_uuid_t))

/* GAP Definitions */
#define GAP_MIN_CONN_INTERVAL               MSEC_TO_UNITS(15, UNIT_1_25_MS)     /* Minimum connection interval (15 ms) based on Apple Guidelines */
#define GAP_MAX_CONN_INTERVAL               MSEC_TO_UNITS(15, UNIT_1_25_MS)     /* Maximum connection interval (15 ms) based on Apple Guidelines */
#define GAP_SLAVE_LATENCY                   3                                   /* Slave latency. */
#define GAP_CONN_SUP_TIMEOUT                MSEC_TO_UNITS(430, UNIT_10_MS)      /* Connection supervisory timeout (430 ms). */

#define CONN_PARAMS_FIRST_UPDATE_DELAY      APP_TIMER_TICKS(5000)               /* Time from initiating event (connect or start of notification) to first time sd_ble_gap_conn_param_update is called (5 seconds). */
#define CONN_PARAMS_NEXT_UPDATE_DELAY       APP_TIMER_TICKS(30000)              /* Time between each call to sd_ble_gap_conn_param_update after the first call (30 seconds). */
#define CONN_PARAMS_MAX_UPDATE_COUNT        3                                   /* Number of attempts before giving up the connection parameter negotiation. */

#define PM_SEC_PARAM_BOND                   1                                   /* Perform bonding. */
#define PM_SEC_PARAM_MITM                   0                                   /* Man In The Middle protection not required. */
#define PM_SEC_PARAM_LESC                   0                                   /* LE Secure Connections not enabled. */
#define PM_SEC_PARAM_KEYPRESS               0                                   /* Keypress notifications not enabled. */
#define PM_SEC_PARAM_IO_CAPABILITIES        BLE_GAP_IO_CAPS_KEYBOARD_ONLY
#define PM_SEC_PARAM_OOB                    0                                   /* Out Of Band data not available. */
#define PM_SEC_PARAM_MIN_KEY_SIZE           7                                   /* Minimum encryption key size. */
#define PM_SEC_PARAM_MAX_KEY_SIZE           16                                  /* Maximum encryption key size. */

#define SCHED_MAX_EVENT_DATA_SIZE           APP_TIMER_SCHED_EVENT_DATA_SIZE     /* Maximum size of scheduler events. */
#ifdef SVCALL_AS_NORMAL_FUNCTION
    #define SCHED_QUEUE_SIZE                20                                  /* Maximum number of events in the scheduler queue. More is needed in case of Serialization. */
#else
    #define SCHED_QUEUE_SIZE                10                                  /* Maximum number of events in the scheduler queue. */
#endif

#define BLE_DEBUG_LOG           0   /* 0 to 4 */
#define BLE_DEBUG_ENCRYPTION    0

#if BLE_DEBUG_LOG
#warning "The BLE Composite Device logs are enabled"
    #define BLE_LOG_ERROR(...)      (BLE_DEBUG_LOG >= 1)NRF_LOG_ERROR( __VA_ARGS__)
    #define BLE_LOG_WARNING(...)    (BLE_DEBUG_LOG >= 2)NRF_LOG_WARNING( __VA_ARGS__)
    #define BLE_LOG_INFO(...)       (BLE_DEBUG_LOG >= 3)NRF_LOG_INFO( __VA_ARGS__)
    #define BLE_LOG_DEBUG(...)      (BLE_DEBUG_LOG >= 4)NRF_LOG_DEBUG( __VA_ARGS__)
    #define BLE_LOG_FLUSH()         NRF_LOG_FLUSH()
#else  /* BLE_DEBUG_LOG */
    #define BLE_LOG_ERROR(...)
    #define BLE_LOG_WARNING(...)
    #define BLE_LOG_INFO(...)
    #define BLE_LOG_DEBUG(...)
    #define BLE_LOG_FLUSH()
#endif /* BLE_DEBUG_LOG */


/* Local types */
typedef struct ble_device_name_ext{ char name[BLE_DEVICE_NAME_LEN + 6]; }PACK ble_device_name_ext_t;


//#define _BLE_DEVICE_NAME_LEN    32  // Same value as flag BLE_DEVICE_NAME_LEN defined in the Ble_manager.h file.
//
//
////MITM Manager
//static bool flag_security_proc_started = false;
//static bool flag_security_proc_failed = false;
//// settings
//static char keyb_ble_name[_BLE_DEVICE_NAME_LEN + 6];  // Plus 6 for " - channel_number\0", where channel_number is a 2 digits number.
//static uint8_t connected_device_name[_BLE_DEVICE_NAME_LEN];  // Declared as uint8_t * because that is what the SDK uses.
//static uint8_t connected_device_address[BLE_GAP_ADDR_LEN];
//
//static bool active_whitelist_flag = false;
//static uint8_t current_channel = 0xFF;
//
//static bool flag_ble_innited = false;
//static bool flag_ble_connected = false;
//static bool flag_ble_is_adv_mode = false;
//static bool flag_ble_is_idle = false;
//uint16_t m_conn_handle = BLE_CONN_HANDLE_INVALID; /* Handle of the current connection. */
//static pm_peer_id_t m_peer_id;                           /* Device reference handle to the current bonded central. */
//static bool flag_peer_deleted = false;
//static bool flag_all_peers_deleted = false;
//static bool flag_connected_device_name_changed = false;

//BLE_BAS_DEF(m_bas);                 /* Structure used to identify the battery service. */
//NRF_BLE_GATT_DEF(m_gatt);           /* GATT module instance. */
//NRF_BLE_QWR_DEF(m_qwr);             /* Context for the Queued Write module.*/
//BLE_ADVERTISING_DEF(m_advertising); /* Advertising module instance. */

typedef struct
{
    ble_device_name_t device_name_local;
    ble_device_name_ext_t device_name_local_ext;      /* Extended local BLE name consisting of the device_name_local and the current channel id */

    uint8_t current_channel_id;

    /* BLE connection handle */
    uint16_t ble_conn_handle;

    /* BLE GAP */
//    ble_device_name_t gap_peer_device_name;
    ble_device_addr_t gap_peer_device_addr;

    /* BLE GATT */
    nrf_ble_gatt_t * p_ble_gatt;

    /* BLE Adv */
    ble_advertising_t * p_ble_adv;

    /* Services */
    nrf_ble_qwr_t * p_ble_qwr;  /* Instance of the Queued Write module */
    ble_bas_t * p_ble_bas;      /* Instance of the Battery Service */

    /* Event callback */
    void * p_instance;
    blecdev_event_cb event_cb;
} blecdev_t;

static blecdev_t blecdev;

//static result_t _blestack_init( blecdev_t * p_blecdev );
//static void scheduler_init(void);
//static void gatt_init(void);
//static void services_init(void);
//static void conn_params_init(void);
//static void peer_manager_init(void);
//
//static void on_adv_evt(ble_adv_evt_t ble_adv_evt);
//static void ble_advertising_error_handler(uint32_t nrf_error);
//static void identities_set(pm_peer_id_list_skip_t skip);
//
//static void qwr_init(void);
//static void nrf_qwr_error_handler(uint32_t nrf_error);
//static void dis_init(void);
//static void bas_init(void);
////static void service_error_handler(uint32_t nrf_error);
//
//static void conn_params_error_handler(uint32_t nrf_error);
//
//static void peer_manager_event_handler(pm_evt_t const *p_evt);
//static void whitelist_set(pm_peer_id_list_skip_t skip);
//
//static void ble_event_handler(ble_evt_t const *ble_event, void *context);
//static void save_connected_device_name(uint8_t *name, uint16_t len);
//
//EventHandlerDeviceName_t evenHandlerDeviceName = NULL;

static INLINE void _process_event_cb( blecdev_t * p_blecdev, blecdev_event_type_t event_type );

static INLINE void _gap_peer_addr_set( blecdev_t * p_blecdev, const ble_gap_addr_t * p_peer_gap_addr );

static INLINE result_t _pm_whitelist_set( pm_peer_id_list_skip_t skip );
static INLINE result_t _pm_identities_set( pm_peer_id_list_skip_t skip );

/*****************************************************************/
/*                           Softdevice                          */
/*****************************************************************/

static INLINE result_t _sd_init( blecdev_t * p_blecdev )
{
    /*
     * There is no SD initialization.
     * Keeping _sd_init just for code-styling purpose
     */

    return RESULT_OK;
}

static INLINE bool_t _sd_is_enabled( blecdev_t * p_blecdev )
{
    return nrf_sdh_is_enabled();
}

static INLINE result_t _sd_enable( blecdev_t * p_blecdev )
{
    ret_code_t err_code;
    result_t result = RESULT_ERR;

    if( _sd_is_enabled( p_blecdev ) == true )
    {
        ASSERT_DYGMA( false, "The softdevice is already enabled" );
        return RESULT_ERR;
    }

    err_code = nrf_sdh_enable_request();
    APP_ERROR_CHECK( err_code );
    EXIT_IF_ERR_NRF( err_code, result, "nrf_sdh_enable_request failed" );

_EXIT:
    ASSERT_DYGMA( _sd_is_enabled( p_blecdev ) == true, "Softdevice expected to be enabled at this point" );
    return result;
}

static INLINE result_t _sd_disable( blecdev_t * p_blecdev )
{
    ret_code_t err_code;
    result_t result = RESULT_ERR;

    if( _sd_is_enabled( p_blecdev ) == false )
    {
        /* The softdevice is already disabled */
        return RESULT_OK;
    }

    err_code = nrf_sdh_disable_request();
    APP_ERROR_CHECK( err_code );
    EXIT_IF_ERR_NRF( err_code, result, "nrf_sdh_disable_request failed" );

_EXIT:
    ASSERT_DYGMA( _sd_is_enabled( p_blecdev ) == false, "Softdevice expected to be disabled at this point" );
    return result;
}

/*****************************************************************/
/*                           BLE Stack                           */
/*****************************************************************/

static void _ble_evt_handler( ble_evt_t const * p_ble_event, void * p_context );

static INLINE result_t _ble_init( blecdev_t * p_blecdev )
{
    p_blecdev->ble_conn_handle = BLE_CONN_HANDLE_INVALID;

    /* Register a handler for BLE events. */
    NRF_SDH_BLE_OBSERVER( ble_observer, BLE_OBSERVER_PRIO, _ble_evt_handler, &blecdev );

    return RESULT_OK;
}

static INLINE result_t _ble_enable( blecdev_t * p_blecdev )
{
    ret_code_t err_code;
    result_t result = RESULT_ERR;

    /* Configure the BLE stack using the default settings. Fetch the start address of the application RAM. */
    uint32_t ram_start_addr = 0;
    err_code = nrf_sdh_ble_default_cfg_set( BLE_CONN_CFG_TAG, &ram_start_addr );
    APP_ERROR_CHECK( err_code );
    EXIT_IF_ERR_NRF( err_code, result, "nrf_sdh_ble_default_cfg_set failed" );

    /* Enable BLE stack. */
    err_code = nrf_sdh_ble_enable( &ram_start_addr );
    APP_ERROR_CHECK( err_code );
    EXIT_IF_ERR_NRF( err_code, result, "nrf_sdh_ble_enable failed" );

_EXIT:
    return result;
}

static INLINE result_t _ble_disable( blecdev_t * p_blecdev )
{
    /*
     * There is no nrf_sdh_ble_disable function. The actual disable should be done just by calling the _sd_disable.
     * Keeping _ble_disable just for code-styling purpose
     */

    return RESULT_OK;
}

static INLINE void _ble_gap_evt_connected_handler( blecdev_t * p_blecdev, const ble_gap_evt_t * p_gap_evt )
{
    ret_code_t err_code;
    const ble_gap_evt_connected_t * p_connected_evt = &p_gap_evt->params.connected;

    ASSERT_DYGMA( p_blecdev->ble_conn_handle == BLE_CONN_HANDLE_INVALID, "Unexpected new BLE GAP connection." );

    BLE_LOG_INFO("<<< BLE connected >>>");

//    flag_ble_is_adv_mode = false;

    /* Save the current connection handle */
    p_blecdev->ble_conn_handle = p_gap_evt->conn_handle;

    /* Save the address of the peer */
    _gap_peer_addr_set( p_blecdev, &p_connected_evt->peer_addr );

    err_code = nrf_ble_qwr_conn_handle_assign( p_blecdev->p_ble_qwr, p_blecdev->ble_conn_handle );
    ASSERT_DYGMA( err_code == NRF_SUCCESS, "nrf_ble_qwr_conn_handle_assign failed" );
    APP_ERROR_CHECK(err_code);

    err_code = sd_ble_gap_tx_power_set( BLE_GAP_TX_POWER_ROLE_CONN, p_blecdev->ble_conn_handle, BLE_TX_POWER );
    ASSERT_DYGMA( err_code == NRF_SUCCESS, "sd_ble_gap_tx_power_set failed" );
    APP_ERROR_CHECK(err_code);
}

//static INLINE void _ble_gap_evt_data_length_update_request_handler( blecdev_t * p_blecdev, const ble_gap_evt_t * p_gap_evt )
//{
//    ret_code_t err_code;
////    const ble_gap_evt_data_length_update_request_t * p_data_length_update_request_evt = &p_gap_evt->params.data_length_update_request;
//
//    ASSERT_DYGMA( p_gap_evt->conn_handle == p_blecdev->ble_conn_handle, "Unexpected change of BLE GAP connection handle." );
//
//    /* Let the softdevice to negotiate the values automatically  */
//    err_code = sd_ble_gap_data_length_update( p_blecdev->ble_conn_handle, NULL, NULL);
//    ASSERT_DYGMA( err_code == NRF_SUCCESS, "sd_ble_gap_data_length_update failed" );
//    APP_ERROR_CHECK(err_code);
//}
//
//static INLINE void _ble_gatts_evt_exchange_mtu_request_handler( blecdev_t * p_blecdev, const ble_gatts_evt_t * p_gatts_evt )
//{
//    const ble_gatts_evt_exchange_mtu_request_t * p_exchange_mtu_request_evt = &p_gatts_evt->params.exchange_mtu_request;
//
//    ASSERT_DYGMA( p_gatts_evt->conn_handle == p_blecdev->ble_conn_handle, "Unexpected change of BLE GAP connection handle." );
//}

static INLINE void _ble_gap_evt_phy_update_request_handler( blecdev_t * p_blecdev, const ble_gap_evt_t * p_gap_evt )
{
    ret_code_t err_code;
//    const ble_gap_evt_phy_update_request_t * p_phy_update_request_evt = &p_gap_evt->params.phy_update_request;

    ASSERT_DYGMA( p_gap_evt->conn_handle == p_blecdev->ble_conn_handle, "Unexpected change of BLE GAP connection handle." );

    BLE_LOG_DEBUG("<<< BLE: PHY update request >>>");

    ble_gap_phys_t const phys = {
        .tx_phys = BLE_GAP_PHY_AUTO,
        .rx_phys = BLE_GAP_PHY_AUTO,
    };
    err_code = sd_ble_gap_phy_update( p_blecdev->ble_conn_handle, &phys );
    ASSERT_DYGMA( err_code == NRF_SUCCESS, "sd_ble_gap_phy_update failed" );
    APP_ERROR_CHECK(err_code);
}

static void _ble_evt_handler( ble_evt_t const * p_ble_event, void * p_context )
{
//    ret_code_t err_code;
    blecdev_t * p_blecdev = ( blecdev_t *)p_context;

    switch ( p_ble_event->header.evt_id )
    {
//        case BLE_GATTC_EVT_CHAR_VAL_BY_UUID_READ_RSP:
//        {
//            ble_gattc_evt_char_val_by_uuid_read_rsp_t *rd_rsp = (ble_gattc_evt_char_val_by_uuid_read_rsp_t *)&ble_event->evt.gattc_evt.params.char_val_by_uuid_read_rsp;
//
//            if (rd_rsp->count)
//            {
//                ble_gattc_handle_value_t hdl_value = {0, NULL};
//
//                if ( NRF_SUCCESS == sd_ble_gattc_evt_char_val_by_uuid_read_rsp_iter((ble_gattc_evt_t *)&ble_event->evt.gattc_evt,
//                                                                                   &hdl_value) )
//                {
//                    save_connected_device_name(hdl_value.p_value, rd_rsp->value_len);
//
//                    if(evenHandlerDeviceName != NULL)
//                    {
//                        evenHandlerDeviceName();
//                    }
//                }
//            }
//        }
//        break;
//
//#if BLE_DEBUG_ENCRYPTION
//        case BLE_GAP_EVT_AUTH_STATUS:
//        {
//            /*
//             * If the peer ignores the request, a BLE_GAP_EVT_AUTH_STATUS event occurs with the status
//             * BLE_GAP_SEC_STATUS_TIMEOUT. Otherwise, the peer initiates security, in which case
//             * things happen as if the peer had initiated security itself. See PM_EVT_CONN_SEC_START
//             * for information about peer-initiated security.
//             */
//            if (ble_event->evt.gap_evt.params.auth_status.auth_status == BLE_GAP_SEC_STATUS_TIMEOUT)
//            {
//                NRF_LOG_DEBUG("<<< BLE: Security request fail. >>>");
//                NRF_LOG_FLUSH();
//            }
//            else
//            {
//                NRF_LOG_DEBUG("<<< BLE: Security request accepted by the master and initiated. >>>");
//                NRF_LOG_FLUSH();
//            }
//        }
//        break;
//#endif
//
//        case BLE_GAP_EVT_AUTH_KEY_REQUEST:
//        {
//            BLE_LOG_INFO("<<< BLE_GAP_EVT_AUTH_KEY_REQUEST >>>");
//            BLE_LOG_FLUSH();
//        }
//        break;

        case BLE_GAP_EVT_CONNECTED:

            _ble_gap_evt_connected_handler( p_blecdev, &p_ble_event->evt.gap_evt );

            break;

#warning "This is handled in the nrf_ble_gatt module"
//        case BLE_GAP_EVT_DATA_LENGTH_UPDATE_REQUEST:
//
//            _ble_gap_evt_data_length_update_request_handler( p_blecdev, &p_ble_event->evt.gap_evt );
//
//            break;

//        case BLE_GAP_EVT_DISCONNECTED:
//        {
//            BLE_LOG_INFO("<<< BLE disconnected >>>");
//
//            flag_ble_connected = false;
//
//            m_conn_handle = BLE_CONN_HANDLE_INVALID;
//        }
//        break;

        case BLE_GAP_EVT_PHY_UPDATE_REQUEST:

            _ble_gap_evt_phy_update_request_handler( p_blecdev, &p_ble_event->evt.gap_evt );

            break;

        case BLE_GAP_EVT_PHY_UPDATE:

            /* Informational event which we ignore currently */

            break;

#warning "This is handled in the nrf_ble_gatt module"
//        case BLE_GATTS_EVT_EXCHANGE_MTU_REQUEST:
//
//            _ble_gatts_evt_exchange_mtu_request_handler( p_blecdev, &p_ble_event->evt.gatts_evt );
//
//            break;

//        case BLE_GATTS_EVT_HVN_TX_COMPLETE:
//        {
//            //Here should be the call to the ble hid service
//            BLE_LOG_DEBUG("<<< BLE: Report sent >>>");
//        }
//        break;
//
//        case BLE_GATTC_EVT_TIMEOUT:
//        {
//// Disconnect on GATT Client timeout event.
//            BLE_LOG_DEBUG("<<< BLE: GATT Client Timeout >>>");
//            err_code = sd_ble_gap_disconnect(ble_event->evt.gattc_evt.conn_handle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
//            APP_ERROR_CHECK(err_code);
//        }
//        break;
//
//        case BLE_GATTS_EVT_TIMEOUT:
//        {
//// Disconnect on GATT Server timeout event.
//            BLE_LOG_DEBUG("<<< BLE: GATT Server Timeout >>>");
//            err_code = sd_ble_gap_disconnect(ble_event->evt.gatts_evt.conn_handle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
//            APP_ERROR_CHECK(err_code);
//        }
//        break;

#warning "Comment these when the development is finished"
        case BLE_GAP_EVT_ADV_SET_TERMINATED:
        case BLE_GAP_EVT_AUTH_KEY_REQUEST:
        case BLE_GAP_EVT_CONN_PARAM_UPDATE:
        case BLE_GAP_EVT_DATA_LENGTH_UPDATE_REQUEST:
        case BLE_GAP_EVT_DATA_LENGTH_UPDATE:
        case BLE_GAP_EVT_SEC_PARAMS_REQUEST:
        case BLE_GATTC_EVT_EXCHANGE_MTU_RSP:
        case BLE_GATTS_EVT_EXCHANGE_MTU_REQUEST:

            /*
             * These events are handled in the SDK low level modules (nrf_ble_gatt, ble_advertising etc.)
             */

            break;

        default:

            ASSERT_DYGMA( false, "Unhandled BLE event" );

            break;
    }
}

/*****************************************************************/
/*                           Scheduler                           */
/*****************************************************************/

static result_t _scheduler_init( blecdev_t * p_blecdev )
{
    /* Function for the Event Scheduler initialization. */
    APP_SCHED_INIT( SCHED_MAX_EVENT_DATA_SIZE, SCHED_QUEUE_SIZE );

    return RESULT_OK;
}

/*****************************************************************/
/*                              GAP                              */
/*****************************************************************/

static INLINE result_t _gap_channel_update( blecdev_t * p_blecdev );

static INLINE result_t _gap_init( blecdev_t * p_blecdev )
{
    /*
     * There is no gap initialization function. GAP configuration depends on the SD being enabled first.
     * Keeping _gap_init just for code-styling purpose
     */

    memset( &p_blecdev->gap_peer_device_addr, 0x00, sizeof(p_blecdev->gap_peer_device_addr) );

    return RESULT_OK;
}

static INLINE result_t _gap_enable( blecdev_t * p_blecdev )
{
    /*
        Function for the GAP initialization.
        This function sets up all the necessary GAP (Generic Access Profile) parameters of the
        device including the device name, appearance, and the preferred connection parameters.
    */

    ret_code_t err_code;
    result_t result = RESULT_ERR;

    ble_gap_conn_params_t gap_conn_params;
    ble_gap_conn_sec_mode_t sec_mode;

    BLE_GAP_CONN_SEC_MODE_SET_ENC_WITH_MITM( &sec_mode );

    err_code = sd_ble_gap_device_name_set( &sec_mode, (const uint8_t *)p_blecdev->device_name_local_ext.name, strlen( p_blecdev->device_name_local_ext.name ) );
    APP_ERROR_CHECK( err_code );
    EXIT_IF_ERR_NRF( err_code, result, "sd_ble_gap_device_name_set failed" );

    err_code = sd_ble_gap_appearance_set( BLE_APPEARANCE_HID_KEYBOARD );
    APP_ERROR_CHECK( err_code );
    EXIT_IF_ERR_NRF( err_code, result, "sd_ble_gap_appearance_set failed" );

    memset( &gap_conn_params, 0, sizeof( gap_conn_params ) );

    gap_conn_params.min_conn_interval = GAP_MIN_CONN_INTERVAL;
    gap_conn_params.max_conn_interval = GAP_MAX_CONN_INTERVAL;
    gap_conn_params.slave_latency = GAP_SLAVE_LATENCY;
    gap_conn_params.conn_sup_timeout = GAP_CONN_SUP_TIMEOUT;

    err_code = sd_ble_gap_ppcp_set( &gap_conn_params );
    APP_ERROR_CHECK( err_code );
    EXIT_IF_ERR_NRF( err_code, result, "sd_ble_gap_ppcp_set failed" );

    result = _gap_channel_update( p_blecdev );
    EXIT_IF_ERR( result, "_gap_channel_update failed" );

_EXIT:
    return result;
}

static INLINE result_t _gap_disable( blecdev_t * p_blecdev )
{
    /*
     * There is no GAP disable function. The actual disable should be done just by calling the _sd_disable.
     * Keeping _gap_disable just for code-styling purpose
     */

    return RESULT_OK;
}

static INLINE result_t _gap_addr_get( blecdev_t * p_blecdev, ble_gap_addr_t * p_gap_addr )
{
    ret_code_t err_code;
    result_t result = RESULT_ERR;

    err_code = sd_ble_gap_addr_get( p_gap_addr );
    APP_ERROR_CHECK(err_code);
    EXIT_IF_ERR_NRF( err_code, result, "sd_ble_gap_addr_get failed" );

_EXIT:
    return result;
}

static INLINE result_t _gap_addr_set( blecdev_t * p_blecdev, ble_gap_addr_t * p_gap_addr )
{
    ret_code_t err_code;
    result_t result = RESULT_ERR;

    err_code = sd_ble_gap_addr_set( p_gap_addr );
    APP_ERROR_CHECK(err_code);
    EXIT_IF_ERR_NRF( err_code, result, "sd_ble_gap_addr_set failed" );

_EXIT:
    return result;
}

static INLINE result_t _gap_channel_update( blecdev_t * p_blecdev )
{
    result_t result = RESULT_ERR;
    ble_gap_addr_t gap_addr;

    /* Get the current GAP address */
    result = _gap_addr_get( p_blecdev, &gap_addr );
    EXIT_IF_ERR( result, "_gap_addr_get failed" );

    if( gap_addr.addr[0] == p_blecdev->current_channel_id )
    {
        return RESULT_OK;
    }

    /* The GAP address channel id differs from the one currently set. So let's replace it */
    BLE_LOG_DEBUG("BLE: Updating channel %i to channel %i", gap_addr.addr[0], p_blecdev->current_channel_id);

    gap_addr.addr[0] = p_blecdev->current_channel_id;

    result = _gap_addr_set( p_blecdev, &gap_addr );
    EXIT_IF_ERR( result, "_gap_addr_set failed" );

_EXIT:

    if ( result != RESULT_OK )
    {
        BLE_LOG_DEBUG("BLE: Channel update failed.");
    }

    return result;
}

static INLINE void _gap_peer_addr_set( blecdev_t * p_blecdev, const ble_gap_addr_t * p_peer_gap_addr )
{
    ASSERT_DYGMA( sizeof(p_blecdev->gap_peer_device_addr) == sizeof( p_peer_gap_addr->addr ), "The BLE device address data is not consistent" );

    memcpy( &p_blecdev->gap_peer_device_addr, p_peer_gap_addr->addr, sizeof(p_blecdev->gap_peer_device_addr) );
    BLE_LOG_INFO("BLE: peer addr saved = %02X %02X %02X %02X %02X %02X",
                  p_blecdev->gap_peer_device_addr.addr[0], p_blecdev->gap_peer_device_addr.addr[1],
                  p_blecdev->gap_peer_device_addr.addr[2], p_blecdev->gap_peer_device_addr.addr[3],
                  p_blecdev->gap_peer_device_addr.addr[4], p_blecdev->gap_peer_device_addr.addr[5]);
}

//void save_connected_device_address(ble_gap_addr_t gapAddr)
//{
//    memcpy(connected_device_address, gapAddr.addr, BLE_GAP_ADDR_LEN);
//    BLE_LOG_INFO("BLE: peer addr saved = %02X %02X %02X %02X %02X %02X",
//                  connected_device_address[0], connected_device_address[1],
//                  connected_device_address[2], connected_device_address[3],
//                  connected_device_address[4], connected_device_address[5]);
//}
//
//uint8_t *get_connected_device_address(void)
//{
//    return connected_device_address;
//}

/*****************************************************************/
/*                              GATT                             */
/*****************************************************************/

static void _gatt_evt_handler_nrf( nrf_ble_gatt_t * p_gatt, nrf_ble_gatt_evt_t const * p_evt )
{
    switch( p_evt->evt_id )
    {
        case NRF_BLE_GATT_EVT_ATT_MTU_UPDATED:
        case NRF_BLE_GATT_EVT_DATA_LENGTH_UPDATED:

#warning "NRF_BLE_GATT_EVT_DATA_LENGTH_UPDATED is not processed"
            /*
             * We currently do not process these events. Is this something we should care?
             */

            break;

        default:

            ASSERT_DYGMA( false, "Unhandled BLE GATT event" );

            break;
    }
}

static result_t _gatt_init( blecdev_t * p_blecdev )
{
    /*
        Function for initializing the GATT module.
    */
    ret_code_t err_code;
    result_t result = RESULT_ERR;

    /* BLE GATT instance  */
    NRF_BLE_GATT_DEF( ble_gatt );           /* GATT module instance. */
    p_blecdev->p_ble_gatt = &ble_gatt;

    err_code = nrf_ble_gatt_init( p_blecdev->p_ble_gatt, _gatt_evt_handler_nrf );
    APP_ERROR_CHECK( err_code );
    EXIT_IF_ERR_NRF( err_code, result, "nrf_ble_gatt_init failed" );

_EXIT:
    return result;
}

/*****************************************************************/
/*                          Advertising                          */
/*****************************************************************/

static ble_uuid_t adv_uuids[] = {{BLE_UUID_HUMAN_INTERFACE_DEVICE_SERVICE, BLE_UUID_TYPE_BLE}};

static void _adv_evt_handler_nrf( ble_adv_evt_t ble_adv_evt );
static void _adv_error_handler_nrf( uint32_t nrf_error );

static result_t _adv_init( blecdev_t * p_blecdev )
{
    /* Advertising module instance. */
    BLE_ADVERTISING_DEF( ble_adv );
    p_blecdev->p_ble_adv = &ble_adv;

    return RESULT_OK;
}

static result_t _adv_conf( blecdev_t * p_blecdev, bool_t whitelisting )
{
    ret_code_t err_code;
    result_t result = RESULT_ERR;

    ble_advertising_init_t init;

    memset(&init, 0, sizeof(init));

    init.advdata.name_type = BLE_ADVDATA_FULL_NAME;
    init.advdata.include_appearance = true;
    init.advdata.flags = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE;
    init.advdata.uuids_complete.uuid_cnt = ADV_UUIDS_CNT;
    init.advdata.uuids_complete.p_uuids = adv_uuids;

    init.config.ble_adv_whitelist_enabled = whitelisting;
    init.config.ble_adv_directed_high_duty_enabled = true;
    init.config.ble_adv_directed_enabled = false;
    init.config.ble_adv_directed_interval = 0;
    init.config.ble_adv_directed_timeout = 0;
    init.config.ble_adv_fast_enabled = true;
    init.config.ble_adv_fast_interval = ADV_FAST_INTERVAL;
    init.config.ble_adv_fast_timeout = ADV_FAST_TIMEOUT;
    init.config.ble_adv_slow_enabled = true;
    init.config.ble_adv_slow_interval = ADV_SLOW_INTERVAL;
    init.config.ble_adv_slow_timeout = ADV_SLOW_TIMEOUT;

    init.evt_handler = _adv_evt_handler_nrf;
    init.error_handler = _adv_error_handler_nrf;

    err_code = ble_advertising_init( p_blecdev->p_ble_adv, &init);
    APP_ERROR_CHECK(err_code);
    EXIT_IF_ERR_NRF( err_code, result, "nrf_ble_gatt_init failed" );

    ble_advertising_conn_cfg_tag_set( p_blecdev->p_ble_adv, BLE_CONN_CFG_TAG );

//    flag_ble_is_adv_mode = true;

_EXIT:
    return result;
}

static INLINE void _adv_evt_whitelist_request_handle( blecdev_t * p_blecdev )
{
    ret_code_t err_code;
    result_t result;

//    flag_ble_is_adv_mode = false;

    ble_gap_addr_t whitelist_addrs[BLE_GAP_WHITELIST_ADDR_MAX_COUNT];
    ble_gap_irk_t whitelist_irks[BLE_GAP_WHITELIST_ADDR_MAX_COUNT];
    uint32_t addr_cnt = BLE_GAP_WHITELIST_ADDR_MAX_COUNT;
    uint32_t irk_cnt = BLE_GAP_WHITELIST_ADDR_MAX_COUNT;

    err_code = pm_whitelist_get( whitelist_addrs, &addr_cnt, whitelist_irks, &irk_cnt );
    ASSERT_DYGMA( err_code == NRF_SUCCESS, "pm_whitelist_get failed" );
    APP_ERROR_CHECK(err_code);

//    if (err_code == NRF_ERROR_NOT_FOUND)
//    {
//        BLE_LOG_DEBUG("BLE: The device was deleted can not connect.");
//    }
//
//    BLE_LOG_DEBUG("BLE: pm_whitelist_get() returns %d addr in whitelist and %d irk whitelist.", addr_cnt, irk_cnt);

    // Set the correct identities list (no excluding peers with no Central Address Resolution).
    result = _pm_identities_set( PM_PEER_ID_LIST_SKIP_NO_IRK );
    ASSERT_DYGMA( result == RESULT_OK, "_pm_identities_set failed" );

    // Apply the whitelist.
    err_code = ble_advertising_whitelist_reply( p_blecdev->p_ble_adv, whitelist_addrs, addr_cnt, whitelist_irks, irk_cnt );
    ASSERT_DYGMA( err_code == NRF_SUCCESS, "ble_advertising_whitelist_reply failed" );
    APP_ERROR_CHECK(err_code);
}

static INLINE void _adv_evt_handler( blecdev_t * p_blecdev, ble_adv_evt_t ble_adv_evt )
{
    /**@brief Function for handling advertising events.
     *
     * @details This function will be called for advertising events which are passed to the application.
     *
     * @param[in] ble_adv_evt  Advertising event.
     */

//    ret_code_t err_code;

    switch ( ble_adv_evt )
    {
//        case BLE_ADV_EVT_DIRECTED_HIGH_DUTY:
//        {
//            flag_ble_is_adv_mode = true;
//            flag_ble_is_idle = false;
//            BLE_LOG_INFO("<<< BLE: High Duty Directed advertising. >>>");
//        }
//        break;
//
//        case BLE_ADV_EVT_DIRECTED:
//        {
//            flag_ble_is_adv_mode = true;
//            flag_ble_is_idle = false;
//            BLE_LOG_INFO("<<< BLE: Directed advertising. >>>");
//        }
//        break;
//
//        case BLE_ADV_EVT_FAST:
//        {
//            flag_ble_is_idle = false;
//            flag_ble_is_adv_mode = true;
//            BLE_LOG_INFO("<<< BLE: Fast advertising. >>>");
//        }
//        break;
//
//        case BLE_ADV_EVT_SLOW:
//        {
//            flag_ble_is_adv_mode = true;
//            BLE_LOG_INFO("<<< BLE: Slow advertising. >>>");
//        }
//        break;
//
//        case BLE_ADV_EVT_FAST_WHITELIST:
//        {
//            flag_ble_is_adv_mode = true;
//            flag_ble_is_idle = false;
//            BLE_LOG_INFO("<<< BLE: Fast advertising with whitelist. >>>");
//        }
//        break;
//
//        case BLE_ADV_EVT_SLOW_WHITELIST:
//        {
//            flag_ble_is_adv_mode = true;
//            flag_ble_is_idle = false;
//            BLE_LOG_INFO("<<< BLE: Slow advertising with whitelist. >>>");
//        }
//        break;

        case BLE_ADV_EVT_DIRECTED_HIGH_DUTY:
        case BLE_ADV_EVT_DIRECTED:
        case BLE_ADV_EVT_FAST:
        case BLE_ADV_EVT_SLOW:
        case BLE_ADV_EVT_FAST_WHITELIST:
        case BLE_ADV_EVT_SLOW_WHITELIST:

            _process_event_cb( p_blecdev, BLECDEV_EVENT_TYPE_ADVERTISING );

            break;

//        case BLE_ADV_EVT_IDLE:
//        {
//            flag_ble_is_adv_mode = false;
//            flag_ble_is_idle = true;
//            BLE_LOG_INFO("<<< BLE: Going to sleep.. >>>");
//            BLE_LOG_FINAL_FLUSH();
//        }
//        break;

        case BLE_ADV_EVT_IDLE:

            _process_event_cb( p_blecdev, BLECDEV_EVENT_TYPE_ADVERTISING_FAILED );

            break;

        case BLE_ADV_EVT_WHITELIST_REQUEST:

            _adv_evt_whitelist_request_handle( p_blecdev );

            break;

//        case BLE_ADV_EVT_PEER_ADDR_REQUEST:
//        {
//            flag_ble_is_adv_mode = false;
//
//            pm_peer_data_bonding_t peer_bonding_data;
//
//            // Only Give peer address if we have a handle to the bonded peer.
//            if (m_peer_id != PM_PEER_ID_INVALID)
//            {
//                err_code = pm_peer_data_bonding_load(m_peer_id, &peer_bonding_data);
//                if (err_code != NRF_ERROR_NOT_FOUND)
//                {
//                    APP_ERROR_CHECK(err_code);
//
//                    // Manipulate identities to exclude peers with no Central Address Resolution.
//                    identities_set(PM_PEER_ID_LIST_SKIP_ALL);
//
//                    ble_gap_addr_t *p_peer_addr = &(peer_bonding_data.peer_ble_id.id_addr_info);
//                    err_code = ble_advertising_peer_addr_reply(&m_advertising, p_peer_addr);
//                    APP_ERROR_CHECK(err_code);
//                }
//            }
//        }
//        break;
//
//        default:
//        {
//            flag_ble_is_adv_mode = false;
//        }
//        break;

        default:

            ASSERT_DYGMA( false, "Unhandled BLE Adv event" );

            break;
    }
}

static void _adv_evt_handler_nrf( ble_adv_evt_t ble_adv_evt )
{
    _adv_evt_handler( &blecdev, ble_adv_evt );
}

static void _adv_error_handler_nrf( uint32_t nrf_error )
{
    /*
        Function for handling advertising errors.

        param[in] nrf_error  Error code containing information about what went wrong.
    */

    ASSERT_DYGMA( false, "Unhandled BLE Adv error" );
}

static result_t _adv_start_base( blecdev_t * p_blecdev, bool_t whitelisting )
{
    ret_code_t err_code;
    result_t result = RESULT_ERR;

    /* Configure the advertising module */
    result = _adv_conf( p_blecdev, whitelisting );
    EXIT_IF_ERR( result, "_adv_conf failed" );

    /* Set the advertising Tx power */
    err_code = sd_ble_gap_tx_power_set( BLE_GAP_TX_POWER_ROLE_ADV, p_blecdev->p_ble_adv->adv_handle, BLE_TX_POWER );
    APP_ERROR_CHECK(err_code);
    EXIT_IF_ERR_NRF( err_code, result, "sd_ble_gap_tx_power_set failed" );

    /* Start advertising. */
    err_code = ble_advertising_start( p_blecdev->p_ble_adv, BLE_ADV_MODE_FAST );
    if (err_code == NRF_ERROR_CONN_COUNT)
    {
        BLE_LOG_INFO("BLE: Maximum connection count exceeded.");
        ASSERT_DYGMA( false, "BLE: Maximum connection count exceeded." );

        return RESULT_ERR;
    }
    APP_ERROR_CHECK(err_code);
    EXIT_IF_ERR_NRF( err_code, result, "ble_advertising_start failed" );

    BLE_LOG_INFO("BLE: Advertising mode.");

_EXIT:
    return result;
}

static INLINE result_t _adv_start( blecdev_t * p_blecdev )
{
    return _adv_start_base( p_blecdev, false );
}

static INLINE result_t _adv_start_whitelist( blecdev_t * p_blecdev )
{
    result_t result = RESULT_ERR;

    /*
        The PM_PEER_ID_LIST_SKIP_NO_ID_ADDR argument specifies that peers that do not have a standard public
        BLE address (i.e., only have an Identity Resolving Key) should not be included in the peer ID list.
    */
    result = _pm_whitelist_set( PM_PEER_ID_LIST_SKIP_NO_ID_ADDR );
    EXIT_IF_ERR( result, "_pm_whitelist_set failed" );

    result = _adv_start_base( p_blecdev, true );
    EXIT_IF_ERR( result, "_adv_start_base failed" );

_EXIT:
    return result;
}

///**@brief Function for disabling advertising and scanning.
// */
//void ble_adv_stop(void)
//{
//    ret_code_t ret = sd_ble_gap_adv_stop(m_advertising.adv_handle);
//    if ((ret != NRF_SUCCESS) &&
//        (ret != NRF_ERROR_INVALID_STATE) &&
//        (ret != BLE_ERROR_INVALID_ADV_HANDLE))
//    {
//        APP_ERROR_CHECK(ret);
//    }
//
//    flag_ble_is_adv_mode = false;
//}

/*****************************************************************/
/*                            Services                           */
/*****************************************************************/

/*
    Function for handling Queued Write Module errors.
    A pointer to this function will be passed to each service which may need to inform the
    application about an error.
    nrf_error: Error code containing information about what went wrong.
*/
static void _qwr_error_handler_nrf( uint32_t nrf_error )
{
    ASSERT_DYGMA( false, "Unhandled BLE QWR error" );
}

/*
    Function for initializing the Queued Write Module.
*/
static INLINE result_t _qwr_init( blecdev_t * p_blecdev )
{
    ret_code_t err_code;

    result_t result = RESULT_ERR;
    nrf_ble_qwr_init_t qwr_init = {0};

    /* BLE QWR instance  */
    NRF_BLE_QWR_DEF( ble_qwr );           /* Context for the Queued Write module.*/
    p_blecdev->p_ble_qwr = &ble_qwr;

    qwr_init.error_handler = _qwr_error_handler_nrf;

    err_code = nrf_ble_qwr_init( p_blecdev->p_ble_qwr, &qwr_init );
    APP_ERROR_CHECK( err_code );
    EXIT_IF_ERR_NRF( err_code, result, "nrf_ble_qwr_init failed" );

_EXIT:
    return result;
}

/*
    Function for initializing Device Information Service.
*/
static INLINE result_t _dis_init( blecdev_t * p_blecdev )
{
    /*
     * The DIS service can be initialized only after the BLE is enabled. Hence that is done withing the _dis_enable function
     */

    return RESULT_OK;
}

/*
    Function for enabling Device Information Service.
*/
static INLINE result_t _dis_enable( blecdev_t * p_blecdev )
{
    ret_code_t err_code;
    result_t result = RESULT_ERR;

    ble_dis_init_t dis_init;
    ble_dis_pnp_id_t pnp_id;

    /* Prepare the PnP structure */
    pnp_id.vendor_id_source = DIS_PNP_ID_VENDOR_ID_SOURCE;
    pnp_id.vendor_id = DIS_PNP_ID_VENDOR_ID;
    pnp_id.product_id = DIS_PNP_ID_PRODUCT_ID;
    pnp_id.product_version = DIS_PNP_ID_PRODUCT_VERSION;

    /* Prepare the DIS initialization structure */
    memset( &dis_init, 0, sizeof( dis_init ) );

    ble_srv_ascii_to_utf8( &dis_init.manufact_name_str, DIS_MANUFACTURER_NAME );
    dis_init.p_pnp_id = &pnp_id;
    dis_init.dis_char_rd_sec = SEC_JUST_WORKS;

    err_code = ble_dis_init( &dis_init );
    APP_ERROR_CHECK( err_code );
    EXIT_IF_ERR_NRF( err_code, result, "ble_dis_init failed" );

_EXIT:
    return result;
}

static INLINE result_t _dis_disable( blecdev_t * p_blecdev )
{
    /*
     * There is no DIS disable function. The actual disable should be done just by calling the _sd_disable.
     * Keeping _dis_enable just for code-styling purpose
     */

    return RESULT_OK;
}

/*
    Function for initializing Battery Service.
*/
static INLINE result_t _bas_init( blecdev_t * p_blecdev )
{
    BLE_BAS_DEF( ble_bas );                 /* Structure used to identify the battery service. */
    p_blecdev->p_ble_bas = &ble_bas;

    return RESULT_OK;
}

static INLINE result_t _bas_enable( blecdev_t * p_blecdev )
{
    ret_code_t err_code;
    result_t result = RESULT_ERR;

    ble_bas_init_t bas_init;

    memset( &bas_init, 0, sizeof( bas_init ) );

    bas_init.evt_handler = NULL;
    bas_init.support_notification = true;
    bas_init.p_report_ref = NULL;
    bas_init.initial_batt_level = 100;

    bas_init.bl_rd_sec = SEC_JUST_WORKS;
    bas_init.bl_cccd_wr_sec = SEC_JUST_WORKS;
    bas_init.bl_report_rd_sec = SEC_JUST_WORKS;

    err_code = ble_bas_init( p_blecdev->p_ble_bas, &bas_init );
    APP_ERROR_CHECK(err_code);
    EXIT_IF_ERR_NRF( err_code, result, "ble_bas_init failed" );

_EXIT:
    return result;
}

static INLINE result_t _bas_disable( blecdev_t * p_blecdev )
{
    /*
     * There is no BAS disable function. The actual disable should be done just by calling the _sd_disable.
     * Keeping _bas_disable just for code-styling purpose
     */

    return RESULT_OK;
}

/*
    Function for initializing services that will be used by the application.
*/
static INLINE result_t _services_init( blecdev_t * p_blecdev )
{
    result_t result = RESULT_ERR;

    result = _qwr_init( p_blecdev );
    EXIT_IF_ERR( result, "_qwr_init failed" );

    result = _dis_init( p_blecdev );
    EXIT_IF_ERR( result, "_dis_init failed" );

    result = _bas_init( p_blecdev );
    EXIT_IF_ERR( result, "_bas_init failed" );

    result = blehid_init();
    EXIT_IF_ERR( result, "blehid_init failed" );

_EXIT:
    return result;
}

/*
    Function for enabling the services.
*/
static INLINE result_t _services_enable( blecdev_t * p_blecdev )
{
    result_t result = RESULT_ERR;

//    result = _qwr_enable( p_blecdev );
//    EXIT_IF_ERR( result, "_qwr_enable failed" );

    result = _dis_enable( p_blecdev );
    EXIT_IF_ERR( result, "_dis_enable failed" );

    result = _bas_enable( p_blecdev );
    EXIT_IF_ERR( result, "_bas_enable failed" );

    result = blehid_enable();
    EXIT_IF_ERR( result, "blehid_enable failed" );

_EXIT:
    return result;
}

/*
    Function for disabling the services.
*/
static INLINE result_t _services_disable( blecdev_t * p_blecdev )
{
    result_t result = RESULT_ERR;

//    result = _qwr_disable( p_blecdev );
//    EXIT_IF_ERR( result, "_qwr_disable failed" );

    result = _dis_disable( p_blecdev );
    EXIT_IF_ERR( result, "_dis_disable failed" );

    result = _bas_disable( p_blecdev );
    EXIT_IF_ERR( result, "_bas_disable failed" );

    result = blehid_disable();
    EXIT_IF_ERR( result, "blehid_disable failed" );

_EXIT:
    return result;
}

/*****************************************************************/
/*                           Connection                          */
/*****************************************************************/

static void _conn_params_error_handler_nrf( uint32_t nrf_error );

static INLINE result_t _conn_params_init( blecdev_t * p_blecdev )
{
    /*
     * The BLE connection parameters can be initialized only after the BLE is enabled. Hence that is done withing the _conn_params_enable function
     */

    return RESULT_OK;
}

static INLINE result_t _conn_params_enable( blecdev_t * p_blecdev )
{
    ret_code_t err_code;
    result_t result = RESULT_ERR;

    ble_conn_params_init_t cp_init;

    memset(&cp_init, 0, sizeof(cp_init));

    cp_init.p_conn_params = NULL;
    cp_init.first_conn_params_update_delay = CONN_PARAMS_FIRST_UPDATE_DELAY;
    cp_init.next_conn_params_update_delay = CONN_PARAMS_NEXT_UPDATE_DELAY;
    cp_init.max_conn_params_update_count = CONN_PARAMS_MAX_UPDATE_COUNT;
    cp_init.start_on_notify_cccd_handle = BLE_GATT_HANDLE_INVALID;
    cp_init.disconnect_on_fail = false;
    cp_init.evt_handler = NULL;
    cp_init.error_handler = _conn_params_error_handler_nrf;

    err_code = ble_conn_params_init(&cp_init);
    APP_ERROR_CHECK(err_code);
    EXIT_IF_ERR_NRF( err_code, result, "ble_conn_params_init failed" );

_EXIT:
    return result;
}

static INLINE result_t _conn_params_disable( blecdev_t * p_blecdev )
{
    ret_code_t err_code;
    result_t result = RESULT_ERR;

    err_code = ble_conn_params_stop();
    APP_ERROR_CHECK(err_code);
    EXIT_IF_ERR_NRF( err_code, result, "ble_conn_params_stop failed" );

_EXIT:
    return result;

}

static void _conn_params_error_handler_nrf( uint32_t nrf_error )
{
    /*
        Function for handling a Connection Parameters error.
        nrf_error: Error code containing information about what went wrong.
    */
    ASSERT_DYGMA( false, "Unhandled BLE Adv error" );
}

/*****************************************************************/
/*                          Peer Manager                         */
/*****************************************************************/

static void _pm_evt_handler_nrf( pm_evt_t const *p_evt );

static result_t _pm_init( blecdev_t * p_blecdev )
{
    /*
        Function for the Peer Manager initialization.
    */

    ret_code_t err_code;
    result_t result = RESULT_ERR;

    err_code = pm_init();
    APP_ERROR_CHECK( err_code );

    // Set security parameters:
    ble_gap_sec_params_t sec_param;
    memset( &sec_param, 0, sizeof( ble_gap_sec_params_t ) );
    sec_param.bond = PM_SEC_PARAM_BOND;
    sec_param.mitm = PM_SEC_PARAM_MITM;
    sec_param.lesc = PM_SEC_PARAM_LESC;
    sec_param.keypress = PM_SEC_PARAM_KEYPRESS;
    sec_param.io_caps = PM_SEC_PARAM_IO_CAPABILITIES;
    sec_param.oob = PM_SEC_PARAM_OOB;
    sec_param.min_key_size = PM_SEC_PARAM_MIN_KEY_SIZE;
    sec_param.max_key_size = PM_SEC_PARAM_MAX_KEY_SIZE;
    sec_param.kdist_own.enc = 1;
    sec_param.kdist_own.id = 1;
    sec_param.kdist_peer.enc = 1;
    sec_param.kdist_peer.id = 1;

    err_code = pm_sec_params_set( &sec_param );
    APP_ERROR_CHECK( err_code );
    EXIT_IF_ERR_NRF( err_code, result, "pm_sec_params_set failed" );

    err_code = pm_register( _pm_evt_handler_nrf );
    APP_ERROR_CHECK( err_code );
    EXIT_IF_ERR_NRF( err_code, result, "pm_register failed" );

_EXIT:
    return result;
}

static INLINE void _pm_evt_con_sec_start_handler( blecdev_t * p_blecdev, pm_evt_t const * p_evt )
{
    const pm_conn_sec_start_evt_t * p_conn_sec_start_evt = &p_evt->params.conn_sec_start;

    ASSERT_DYGMA( p_evt->conn_handle == p_blecdev->ble_conn_handle, "Unexpected change of BLE connection handle." );

    BLE_LOG_DEBUG("<<< BLE: Security procedure started. >>>");

    switch( p_conn_sec_start_evt->procedure )
    {
        case PM_CONN_SEC_PROCEDURE_BONDING:

            _process_event_cb( p_blecdev, BLECDEV_EVENT_TYPE_SEC_CODE_REQ );

            break;

//        case PM_CONN_SEC_PROCEDURE_ENCRYPTION:
//        case PM_CONN_SEC_PROCEDURE_PAIRING:

        default:

            ASSERT_DYGMA( false, "Unhandled BLE PM security procedure." );

            break;

    }
}

static void _pm_evt_handler_nrf( pm_evt_t const *p_evt )
{
    blecdev_t * p_blecdev = &blecdev;   /* There is no external context possibly registered to the peer manager. Hence we set it here. */

    pm_handler_on_pm_evt(p_evt);
    pm_handler_disconnect_on_sec_failure(p_evt);
    pm_handler_flash_clean(p_evt);

    switch (p_evt->evt_id)
    {
        case PM_EVT_CONN_CONFIG_REQ:

            /*
             * We ignore this event on this level which makes the peer manager take control of the peer connection. In rare cases
             * the alternative is to use function 'pm_conn_exclude' for excluding this connection from the peer manager processing.
             * We are not using this option.
             */

            break;

//        case PM_EVT_CONN_SEC_START:
//
//            flag_security_proc_started = true;
//            flag_security_proc_failed = false;
//
//            BLE_LOG_DEBUG("<<< BLE: Security procedure started. >>>");
//            BLE_LOG_FLUSH();
//
//            break;

        case PM_EVT_CONN_SEC_START:

            _pm_evt_con_sec_start_handler( p_blecdev, p_evt );

            break;

//        case PM_EVT_CONN_SEC_FAILED:
//        {
//            flag_security_proc_started = false;
//            flag_security_proc_failed = true;
//
//            BLE_LOG_DEBUG("<<< BLE: Security procedure failed. >>>");
//            BLE_LOG_FLUSH();
//        }
//        break;
//
//        case PM_EVT_CONN_SEC_SUCCEEDED:
//        {
//            BLE_LOG_DEBUG("<<< BLE: PM_EVT_CONN_SEC_SUCCEEDED >>>");
//            BLE_LOG_FLUSH();
//
//            flag_ble_connected = true;
//            flag_security_proc_failed = false;
//            m_peer_id = p_evt->peer_id;
//        }
//        break;
//
//        case PM_EVT_PEER_DELETE_SUCCEEDED:
//        {
//            BLE_LOG_DEBUG("<<< BLE: PM_EVT_PEER_DELETE_SUCCEEDED >>>");
//            BLE_LOG_FLUSH();
//
//            flag_peer_deleted = true;
//        }
//        break;
//
//        case PM_EVT_PEERS_DELETE_SUCCEEDED:
//        {
//            BLE_LOG_DEBUG("<<< BLE: PM_EVT_PEERS_DELETE_SUCCEEDED >>>");
//            BLE_LOG_FLUSH();
//
//            flag_all_peers_deleted = true;
//        }
//        break;
//
//        case PM_EVT_PEER_DATA_UPDATE_SUCCEEDED:
//        {
//            if (p_evt->params.peer_data_update_succeeded.flash_changed && (p_evt->params.peer_data_update_succeeded.data_id == PM_PEER_DATA_ID_BONDING))
//            {
//                BLE_LOG_DEBUG("<<< BLE: New Bond, adding peer to the whitelist. >>>");
//                // Note: You should check on what kind of white list policy your application should use.
//
//                /*
//                    If a new pairing has been created, update the whitelist to include it.
//
//                    The PM_PEER_ID_LIST_SKIP_NO_ID_ADDR argument specifies that peers that do not have a standard public
//                    BLE address (i.e., only have an Identity Resolving Key) should not be included in the peer ID list.
//                */
//                whitelist_set(PM_PEER_ID_LIST_SKIP_NO_ID_ADDR);
//            }
//        }
//        break;

#warning "Comment these when the development is finished"
        case PM_EVT_CONN_SEC_PARAMS_REQ:

            /*
             * These events are handled within peer manager
             */

            break;

        default:

            ASSERT_DYGMA( false, "Unhandled BLE PM event" );

            break;
    }
}

static INLINE result_t _pm_whitelist_set( pm_peer_id_list_skip_t skip )
{
    /*
        Function for setting filtered whitelist.
        Obtains, from the flash memory, a list of paired devices (peers) that have previously been
        connected and setting them as a whitelist for future connections.
        The devices on this whitelist are the only ones your device will allow to connect when it is
        in advertising mode.

        skip: Filter passed to pm_peer_id_list() function.
    */

    ret_code_t err_code;
    result_t result = RESULT_ERR;

    pm_peer_id_t peer_ids[ BLE_GAP_WHITELIST_ADDR_MAX_COUNT ];
    uint32_t peer_id_count = BLE_GAP_WHITELIST_ADDR_MAX_COUNT;

    /*
        pm_peer_id_list(peer_ids, &peer_id_count, PM_PEER_ID_INVALID, skip); is used to obtain a list of peer IDs from
        the data stored in flash memory. These peer IDs represent devices that have previously been paired with.
        The function can filter peer IDs based on several criteria, which are specified in the 'skip' argument.

        This function starts searching from first_peer_id. IDs ordering is the same as for pm_next_peer_id_get().
        If the first_peer_id is PM_PEER_ID_INVALID, the function starts searching from the first ID. The function
        looks for the ID's number specified by p_list_size. Only those IDs that match skip_id are added to the list.
        The number of returned elements is determined by p_list_size.

        Warning:
            The size of the p_peer_list buffer must be equal or greater than p_list_size.

        Parameters:
            [out]       p_peer_list: Pointer to peer IDs list buffer.
            [in, out]   p_list_size: The amount of IDs to return / The number of returned IDs.
            [in]        first_peer_id: The first ID from which the search begins.
                                       IDs ordering is the same as for pm_next_peer_id_get().
            [in]        skip_id: It determines which peer ID will be added to list.

        Return values:
            NRF_SUCCESS                 If the ID list has been filled out.
            NRF_ERROR_INVALID_PARAM     If skip_id was invalid.
            NRF_ERROR_NULL              If peer_list or list_size was NULL.
            NRF_ERROR_INVALID_STATE     If the Peer Manager is not initialized.
    */
    err_code = pm_peer_id_list( peer_ids, &peer_id_count, PM_PEER_ID_INVALID, skip );
    APP_ERROR_CHECK( err_code );
    EXIT_IF_ERR_NRF( err_code, result, "pm_peer_id_list failed" );

    BLE_LOG_INFO("BLE: Peers in whitelist: %d, MAX_PEERS_WLIST: %d", peer_id_count, BLE_GAP_WHITELIST_ADDR_MAX_COUNT);

    /*
        Function for setting or clearing the whitelist.

        When using the S13x SoftDevice v3.x, this function sets or clears the whitelist.
        When using the S13x SoftDevice v2.x, this function caches a list of peers that
        can be retrieved later by pm_whitelist_get to pass to the Advertising Module.

        To clear the current whitelist, pass either NULL as p_peers or zero as peer_cnt.

        Parameters:
            [in]    p_peers: The peers to add to the whitelist. Pass NULL to clear the current whitelist.
            [in]    peer_cnt: The number of peers to add to the whitelist. The number must not be greater
                              than BLE_GAP_WHITELIST_ADDR_MAX_COUNT. Pass zero to clear the current whitelist.

        Return values:
            NRF_SUCCESS                     If the whitelist was successfully set or cleared.
            BLE_GAP_ERROR_WHITELIST_IN_USE  If a whitelist is already in use and cannot be set.
            BLE_ERROR_GAP_INVALID_BLE_ADDR  If a peer in p_peers has an address that cannot be used for whitelisting.
            NRF_ERROR_NOT_FOUND             If any of the peers in p_peers cannot be found.
            NRF_ERROR_DATA_SIZE             If peer_cnt is greater than BLE_GAP_WHITELIST_ADDR_MAX_COUNT.
            NRF_ERROR_INVALID_STATE         If the Peer Manager is not initialized.
    */
    err_code = pm_whitelist_set( peer_ids, peer_id_count );
    BLE_LOG_INFO( "BLE: pm_whitelist_set() returns %d", err_code );
    APP_ERROR_CHECK( err_code );
    EXIT_IF_ERR_NRF( err_code, result, "pm_whitelist_set failed" );

_EXIT:
    return result;
}

static INLINE result_t _pm_identities_set( pm_peer_id_list_skip_t skip )
{
    /*
        Function for setting filtered device identities.
        skip: Filter passed to @ref pm_peer_id_list.
    */

    ret_code_t err_code;
    result_t result = RESULT_ERR;

    pm_peer_id_t peer_ids[ BLE_GAP_DEVICE_IDENTITIES_MAX_COUNT ];
    uint32_t peer_id_count = BLE_GAP_DEVICE_IDENTITIES_MAX_COUNT;

    err_code = pm_peer_id_list( peer_ids, &peer_id_count, PM_PEER_ID_INVALID, skip );
    APP_ERROR_CHECK(err_code);
    EXIT_IF_ERR_NRF( err_code, result, "pm_peer_id_list failed" );

    err_code = pm_device_identities_list_set( peer_ids, peer_id_count );
    APP_ERROR_CHECK(err_code);
    EXIT_IF_ERR_NRF( err_code, result, "pm_device_identities_list_set failed" );

_EXIT:
    return result;
}

//
////static void service_error_handler(uint32_t nrf_error)
////{
////    /*
////        Function for handling Service errors.
////        A pointer to this function will be passed to each service which may need to inform the
////        application about an error.
////
////        nrf_error: Error code containing information about what went wrong.
////    */
////    APP_ERROR_HANDLER(nrf_error);
////}
//
//bool get_flag_security_proc_started(void)
//{
//    return flag_security_proc_started;
//}
//
//void clear_flag_security_proc_started(void)
//{
//    flag_security_proc_started = false;
//}
//
//bool get_flag_security_proc_failed(void)
//{
//    return flag_security_proc_failed;
//}
//
//void clear_flag_security_proc_failed(void)
//{
//    flag_security_proc_failed = false;
//}
//
//void ble_send_encryption_pin(char const *pin_number)
//{
//    ret_code_t err_code = sd_ble_gap_auth_key_reply(m_conn_handle,
//                                                    BLE_GAP_AUTH_KEY_TYPE_PASSKEY,
//                                                    (const uint8_t *)pin_number);
//    APP_ERROR_CHECK(err_code);
//}
//
//bool ble_is_advertising_mode(void)
//{
//    return flag_ble_is_adv_mode;
//}
//
//bool ble_is_idle(void)
//{
//    return flag_ble_is_idle;
//}
//
//void ble_disconnect(void)
//{
//    if (!ble_connected()) return;
//
//    if (m_conn_handle != BLE_CONN_HANDLE_INVALID)
//    {
//        sd_ble_gap_disconnect(m_conn_handle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
//        while (flag_ble_connected) ble_run(); // Wait until disconnecting procedure ends.
//    }
//}
//
//void delete_peers(void)
//{
//    /*
//        Clear bond information from persistent storage.
//        First you need to disconnect BLE.
//    */
//
//    ret_code_t err_code;
//
//    BLE_LOG_INFO("BLE: Deleting paired devices...");
//
//    flag_all_peers_deleted = false;
//    err_code = pm_peers_delete();
//    APP_ERROR_CHECK(err_code);
//
//    while (!flag_all_peers_deleted)
//        ble_run(); // Wait until delet procedure ends.
//    BLE_LOG_INFO("BLE: Done");
//}
//
///**
// * @brief Function for deleting a bond by its peer id.
// *
// * @details This function is used to delete a bond associated with a specific peer id.
// * The function calls the Nordic SDK function pm_peer_delete() and passes the given peer id.
// * Any error returned by pm_peer_delete() is checked and handled by APP_ERROR_CHECK().
// *
// * @param[in]   peer_id  The id of the peer whose bond we want to delete.
// */
//void delete_peer_by_id(pm_peer_id_t peer_id)
//{
//    if (peer_id == PM_PEER_ID_INVALID) return;
//
//    BLE_LOG_INFO("BLE: Deleting paired device ID=%d from flash memory...", peer_id);
//
//    flag_peer_deleted = false;
//    ret_code_t ret = pm_peer_delete(peer_id);
//    APP_ERROR_CHECK(ret);
//
//    while (!flag_peer_deleted)
//        ble_run(); // Wait until delet procedure ends.
//    BLE_LOG_INFO("BLE: Done");
//}
//
///**
// * @brief Function for getting the next peer id.
// *
// * @details This function wraps the pm_next_peer_id_get() function of the Nordic SDK.
// * It is used to iterate through all peer IDs stored in the flash memory. The input argument
// * defines the starting point for the search of the next peer ID.
// *
// * @param[in]   peer_id  The peer ID from where to get the next peer ID. To get the first peer ID, use PM_PEER_ID_INVALID.
// *
// * @return      The next peer id. If there is no more peer ID, PM_PEER_ID_INVALID will be returned.
// */
//pm_peer_id_t get_next_peer_id(pm_peer_id_t peer_id)
//{
//    return pm_next_peer_id_get(peer_id);
//}
//
//void save_connected_device_name(uint8_t *name, uint16_t len)
//{
//    if (name)  // pass NULL to skip copy
//    {
//        memcpy(connected_device_name, name, len);
//    }
//    flag_connected_device_name_changed = true;
//
//    BLE_LOG_DEBUG("BLE: New connected_device_name = %s, len = %i", connected_device_name, len);
//}
//
//uint8_t *get_connected_device_name_ptr(void)
//{
//    return connected_device_name;
//}
//
//pm_peer_id_t get_connected_peer_id(void)
//{
//    return m_peer_id;
//}
//
//void ble_get_device_name(EventHandlerDeviceName_t evenHandler)
//{
//    evenHandlerDeviceName = evenHandler;  // Set the handler to get the host BLE device name.
//
//    // Ask the soft device to give us the device name.
//    ble_gattc_handle_range_t hdl_range = {.start_handle = 1, .end_handle = 0xffff};
//    ble_uuid_t bleUuid = {BLE_UUID_GAP_CHARACTERISTIC_DEVICE_NAME, BLE_UUID_TYPE_BLE};
//    sd_ble_gattc_char_value_by_uuid_read(m_conn_handle, &bleUuid, &hdl_range);
//}
//
//bool ble_connected(void)
//{
//    return flag_ble_connected;
//}
//
//bool ble_innited(void)
//{
//    return flag_ble_innited;
//}
//
//bool ble_get_flag_connection_name_changed(void)
//{
//    return flag_connected_device_name_changed;
//}
//
//void ble_set_flag_connection_name_changed(bool flag)
//{
//    flag_connected_device_name_changed = flag;
//}
//
//
//void ble_battery_level_update(uint8_t battery_level)
//{
//    if (!ble_connected())
//    {
//        return;
//    }
//
//    ret_code_t err_code;
//
//    err_code = ble_bas_battery_level_update(&m_bas, battery_level, m_conn_handle);
//    if ( (err_code != NRF_SUCCESS) &&
//        (err_code != NRF_ERROR_BUSY) &&
//        (err_code != NRF_ERROR_RESOURCES) &&
//        (err_code != NRF_ERROR_FORBIDDEN) &&
//        (err_code != NRF_ERROR_INVALID_STATE) &&
//        (err_code != BLE_ERROR_GATTS_SYS_ATTR_MISSING) )
//    {
//        APP_ERROR_HANDLER(err_code);
//    }
//}
//
//void set_device_name(const char *device_name)
//{
//    snprintf(keyb_ble_name, sizeof(keyb_ble_name), "%s - %i", device_name, current_channel + 1);
//}
//
//void set_current_channel(uint8_t channel)
//{
//    current_channel = channel;
//}
//
//void set_whitelist(bool whitelisting)
//{
//    active_whitelist_flag = whitelisting;
//}

/*****************************************************************/
/*                 BLE Composite Device control                  */
/*****************************************************************/

static INLINE result_t _init( blecdev_t * p_blecdev, const blecdev_conf_t * p_config )
{
    result_t result = RESULT_ERR;

    /* Save the local config */
    p_blecdev->device_name_local = p_config->device_name_local;

    p_blecdev->p_instance = p_config->p_instance;
    p_blecdev->event_cb = p_config->event_cb;

    result = _sd_init( p_blecdev );
    EXIT_IF_ERR( result, "_sd_init failed" );

    result = _ble_init( p_blecdev );
    EXIT_IF_ERR( result, "_ble_init failed" );

    result = _scheduler_init( p_blecdev );
    EXIT_IF_ERR( result, "_scheduler_init failed" );

    result = _gap_init( p_blecdev );
    EXIT_IF_ERR( result, "_gap_init failed" );

    result = _gatt_init( p_blecdev );
    EXIT_IF_ERR( result, "_gatt_init failed" );

    result = _adv_init( p_blecdev );
    EXIT_IF_ERR( result, "_adv_init failed" );

    result = _services_init( p_blecdev );
    EXIT_IF_ERR( result, "_services_init failed" );

    result = _conn_params_init( p_blecdev );
    EXIT_IF_ERR( result, "_conn_params_init failed" );

    result = _pm_init( p_blecdev );
    EXIT_IF_ERR( result, "_pm_init failed" );

//    flag_ble_innited = true;

_EXIT:
    return result;
}

static INLINE result_t _enable( blecdev_t * p_blecdev, const blecdev_enable_conf_t * p_enable_config )
{
    result_t result = RESULT_ERR;

    /* Save the current BLE channel ID and create the local device extended name */
    p_blecdev->current_channel_id = p_enable_config->current_channel_id;

    memset( &p_blecdev->device_name_local_ext, 0x00, sizeof(p_blecdev->device_name_local_ext) );
    snprintf( p_blecdev->device_name_local_ext.name, sizeof(p_blecdev->device_name_local_ext.name)-1, "%s - %i",
              p_blecdev->device_name_local.name, p_blecdev->current_channel_id + 1 );

    /* Enable the softdevice */
    result = _sd_enable( p_blecdev );
    EXIT_IF_ERR( result, "_sd_enable failed" );

    /* Enable the BLE stack */
    result = _ble_enable( p_blecdev );
    EXIT_IF_ERR( result, "_ble_enable failed" );

    /* Enable the GAP module */
    result = _gap_enable( p_blecdev );
    EXIT_IF_ERR( result, "_gap_enable failed" );

    /* Enable the Services */
    result = _services_enable( p_blecdev );
    EXIT_IF_ERR( result, "_services_enable failed" );

    /* Enable the Connection Parameters */
    result = _conn_params_enable( p_blecdev );
    EXIT_IF_ERR( result, "_conn_params_enable failed" );

_EXIT:
    return result;
}

static INLINE result_t _disable( blecdev_t * p_blecdev )
{
    result_t result = RESULT_ERR;

    /* Disable the Connection Parameters */
    result = _conn_params_disable( p_blecdev );
    EXIT_IF_ERR( result, "_conn_params_disable failed" );

    /* Disable the Services */
    result = _services_disable( p_blecdev );
    EXIT_IF_ERR( result, "_services_disable failed" );

    /* Disable the GAP module */
    result = _gap_disable( p_blecdev );
    EXIT_IF_ERR( result, "_gap_disable failed" );

    /* Disable the BLE stack */
    result = _ble_disable( p_blecdev );
    EXIT_IF_ERR( result, "_ble_disable failed" );

    /* Disable the softdevice */
    result = _sd_disable( p_blecdev );
    EXIT_IF_ERR( result, "_sd_disable failed" );

_EXIT:
    return result;
}

static INLINE void _process_event_cb( blecdev_t * p_blecdev, blecdev_event_type_t event_type )
{
    if( p_blecdev->event_cb == NULL )
    {
        return;
    }

    p_blecdev->event_cb( p_blecdev->p_instance, event_type );
}

static INLINE uint16_t _conn_handle_get( blecdev_t * p_blecdev )
{
    return p_blecdev->ble_conn_handle;
}

static INLINE void _run( blecdev_t * p_blecdev )
{
    app_sched_execute();
}

//void ble_run(void)
//{
//    /*
//        Function for handling the idle state (main loop).
//    */
//
//    app_sched_execute();
//
//}

/*****************************************************************/
/*                              API                              */
/*****************************************************************/

result_t blecdev_init( const blecdev_conf_t * p_config )
{
    return _init( &blecdev, p_config );
}

result_t blecdev_enable( const blecdev_enable_conf_t * p_enable_config )
{
    return _enable( &blecdev, p_enable_config );
}

result_t blecdev_disable( void )
{
    return _disable( &blecdev );
}

result_t blecdev_adv_start( void )
{
    return _adv_start( &blecdev );
}

result_t blecdev_adv_start_whitelist( void )
{
    return _adv_start_whitelist( &blecdev );
}

uint16_t blecdev_conn_handle_get( void )
{
    return _conn_handle_get( &blecdev );
}

void blecdev_run( void )
{
    _run( &blecdev );
}
