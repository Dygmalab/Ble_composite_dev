/* -*- mode: c++ -*-
 * Manage the low level Bluetooth low energy communication between the Neuron 2
 * and the computer host.
 * Copyright© 2020  Dygma Lab S.L.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 * 
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Configuration guidelines obtained from Nordic bolierplates for BT communications
 * SDK Version: nRF5_SDK_17.1.0
 * 
 * Author: Juan Hauara @JuanHauara
 */

#ifndef __BLE_COMPOSITE_DEVICE_H__
#define __BLE_COMPOSITE_DEVICE_H__


#ifdef __cplusplus
extern "C"
{
#endif

#include "ble.h"
#include "ble_err.h"
#include "ble_hci.h"
#include "ble_srv_common.h"
#include "ble_advertising.h"
#include "ble_advdata.h"
#include "ble_hids.h"
#include "ble_bas.h"
#include "ble_dis.h"
#include "ble_conn_params.h"

#include "app_scheduler.h"
#include "nrf_sdh.h"
#include "nrf_sdh_soc.h"
#include "nrf_sdh_ble.h"
#include "app_timer.h"
#include "peer_manager.h"
#include "fds.h"
#include "ble_conn_state.h"
#include "nrf_ble_gatt.h"
#include "nrf_ble_qwr.h"
#include "nrf_pwr_mgmt.h"
#include "peer_manager_handler.h"
#include <ble_gap.h>
#include "ble_dtm.h"

#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"


#if COMPILE_DEFY_KEYBOARD
    #define BLE_DEVICE_NAME                 "Defy BLE"      /* Name of device. Will be included in the advertising data. */
#elif COMPILE_RAISE2_KEYBOARD
    #define BLE_DEVICE_NAME                 "Raise2"        /* Name of device. Will be included in the advertising data. */
#endif

#define MANUFACTURER_NAME                   "Dygma Lab"     /* Manufacturer. Will be passed to Device Information Service. */

#define APP_BLE_OBSERVER_PRIO               3               /* Application's BLE observer priority. You shouldn't need to modify this value. */
#define APP_BLE_CONN_CFG_TAG                1               /* A tag identifying the SoftDevice BLE configuration. */

#define PNP_ID_VENDOR_ID_SOURCE             0x02            /* Vendor ID Source. */

// Note: USB VENDOR ID and PRODUCT ID are defined in the Makefile.

#define PNP_ID_PRODUCT_VERSION              0x0001          /* Product Version. */

#define APP_ADV_FAST_INTERVAL               0x0028          /* Fast advertising interval (in units of 0.625 ms. This value corresponds to 25 ms.). */
#define APP_ADV_SLOW_INTERVAL               0x0C80          /* Slow advertising interval (in units of 0.625 ms. This value corrsponds to 2 seconds). */

/*
    Time the Neuron remains in fast advertising after restarting the
    system = 6000 units of 10ms = 1 minute.
*/
#define APP_ADV_FAST_DURATION               3000
/*
    Time the Neuron remains in slow advertising after the
    APP_ADV_FAST_DURATION time ends = 12000 units of 10ms = 2 minutes.
*/
#define APP_ADV_SLOW_DURATION               1

/*lint -emacro(524, MIN_CONN_INTERVAL) // Loss of precision */
#define MIN_CONN_INTERVAL                   MSEC_TO_UNITS(15, UNIT_1_25_MS)     /* Minimum connection interval (15 ms) based on Apple Guidelines */
#define MAX_CONN_INTERVAL                   MSEC_TO_UNITS(15, UNIT_1_25_MS)     /* Maximum connection interval (15 ms) based on Apple Guidelines */
#define SLAVE_LATENCY                       3                                   /* Slave latency. */
#define CONN_SUP_TIMEOUT                    MSEC_TO_UNITS(430, UNIT_10_MS)      /* Connection supervisory timeout (430 ms). */

#define FIRST_CONN_PARAMS_UPDATE_DELAY      APP_TIMER_TICKS(5000)               /* Time from initiating event (connect or start of notification) to first time sd_ble_gap_conn_param_update is called (5 seconds). */
#define NEXT_CONN_PARAMS_UPDATE_DELAY       APP_TIMER_TICKS(30000)              /* Time between each call to sd_ble_gap_conn_param_update after the first call (30 seconds). */
#define MAX_CONN_PARAMS_UPDATE_COUNT        3                                   /* Number of attempts before giving up the connection parameter negotiation. */

#define SEC_PARAM_BOND                      1                                   /* Perform bonding. */
#define SEC_PARAM_MITM                      0                                   /* Man In The Middle protection not required. */
#define SEC_PARAM_LESC                      0                                   /* LE Secure Connections not enabled. */
#define SEC_PARAM_KEYPRESS                  0                                   /* Keypress notifications not enabled. */
#define SEC_PARAM_IO_CAPABILITIES           BLE_GAP_IO_CAPS_KEYBOARD_ONLY
#define SEC_PARAM_OOB                       0                                   /* Out Of Band data not available. */
#define SEC_PARAM_MIN_KEY_SIZE              7                                   /* Minimum encryption key size. */
#define SEC_PARAM_MAX_KEY_SIZE              16                                  /* Maximum encryption key size. */


#define SCHED_MAX_EVENT_DATA_SIZE           APP_TIMER_SCHED_EVENT_DATA_SIZE     /* Maximum size of scheduler events. */
#ifdef SVCALL_AS_NORMAL_FUNCTION
#define SCHED_QUEUE_SIZE                    20                                  /* Maximum number of events in the scheduler queue. More is needed in case of Serialization. */
#else
#define SCHED_QUEUE_SIZE                    10                                  /* Maximum number of events in the scheduler queue. */
#endif


    extern uint16_t m_conn_handle; /* Handle of the current connection. */

    void ble_module_init(void);
    void update_current_channel(void);
    void ble_run(void);
    bool ble_connected(void);
    bool ble_innited(void);
    void ble_disconnect(void);
    void ble_adv_stop(void);
    void advertising_init(void);
    void gap_params_init(void);

    bool get_flag_security_proc_started(void);
    void clear_flag_security_proc_started(void);
    bool get_flag_security_proc_failed(void);
    void clear_flag_security_proc_failed(void);
    void ble_send_encryption_pin(char const *pin_number);
    bool ble_get_flag_connection_name_changed();
    void ble_set_flag_connection_name_changed(bool flag);
    void ble_goto_advertising_mode(void);
    void ble_goto_white_list_advertising_mode(void);
    bool ble_is_advertising_mode(void);
    bool ble_is_idle(void);
    void delete_peers(void);
    void delete_peer_by_id(pm_peer_id_t peer_id);
    pm_peer_id_t get_next_peer_id(pm_peer_id_t peer_id);

    void ble_battery_level_update(uint8_t battery_level);

    ble_gap_addr_t gap_addr_get(void);
    bool gap_addr_set(ble_gap_addr_t* gap_addr);

    //Setters for config
    uint8_t *get_connected_device_address(void);
    typedef void(*EventHandlerDeviceName_t)(void);
    void ble_get_device_name(EventHandlerDeviceName_t evenHandlerDeviceName);
    uint8_t *get_connected_device_name_ptr(void);
    pm_peer_id_t get_connected_peer_id(void);
    void set_device_name(const char* device_name);
    void set_current_channel(uint8_t channel);
    void set_whitelist(bool active);

#ifdef __cplusplus
}
#endif


#endif // __BLE_COMPOSITE_DEVICE_H__
