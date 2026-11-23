/*
 * Manage the low level Bluetooth low energy communication between the Neuron 2
 * and the computer host.
 * Copyright© 2026  Dygma Lab S.L.
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
 */

#ifndef __BLE_COMPOSITE_DEVICE_H__
#define __BLE_COMPOSITE_DEVICE_H__


#ifdef __cplusplus
extern "C"
{
#endif

#include "dl_middleware.h"

#include "ble_types.h"

    typedef enum
    {
        BLECDEV_EVENT_TYPE_ADVERTISING = 1,
        BLECDEV_EVENT_TYPE_ADVERTISING_FAILED,
        BLECDEV_EVENT_TYPE_SEC_BOND_CODE_REQ,
        BLECDEV_EVENT_TYPE_SEC_BOND_SUCCESS,
        BLECDEV_EVENT_TYPE_SEC_BOND_FAILED,
        BLECDEV_EVENT_TYPE_PEER_CONNECTED,

        BLECDEV_EVENT_TYPE_PEER_APP_DATA_STORED,
        BLECDEV_EVENT_TYPE_PEER_APP_DATA_STORE_FAILED,
        BLECDEV_EVENT_TYPE_PEER_ERASED,
        BLECDEV_EVENT_TYPE_PEER_ERASE_FAILED,

//#warning "Remove BLECDEV_EVENT_TYPE_PEER_DEVICE_NAME from the blecdev_event_types. Eventually, add it to the peer_control types"
        BLECDEV_EVENT_TYPE_PEER_DEVICE_NAME,
    } blecdev_event_type_t;

    typedef struct
    {
        pm_peer_id_t peer_id;
        ble_device_addr_t peer_addr;
    } blecdev_evt_sec_bond_success_param_t;

    typedef struct
    {
        pm_peer_id_t peer_id;
    } blecdev_evt_sec_bond_failed_param_t;

    typedef struct
    {
        pm_peer_id_t peer_id;
    } blecdev_evt_peer_connected_param_t;

    typedef struct
    {
        ble_device_name_t peer_name;
    } blecdev_evt_peer_device_name_param_t;

    typedef union
    {
        blecdev_evt_sec_bond_success_param_t sec_bond_success;
        blecdev_evt_sec_bond_failed_param_t  sec_bond_failed;
        blecdev_evt_peer_connected_param_t   peer_connected;

        blecdev_evt_peer_device_name_param_t peer_device_name;
    } blecdev_evt_param_t;

    typedef void (* blecdev_event_cb)( void * p_instance, blecdev_event_type_t event_type, blecdev_evt_param_t * p_param );

    typedef struct
    {
        ble_device_name_t device_name_local;

        /* Event callback */
        void * p_instance;
        blecdev_event_cb event_cb;
    } blecdev_conf_t;

    typedef struct
    {
        uint8_t current_channel_id;
    } blecdev_enable_conf_t;

    extern result_t blecdev_init( const blecdev_conf_t * p_config );
    extern result_t blecdev_enable( const blecdev_enable_conf_t * p_enable_config );
    extern result_t blecdev_disable( void );

    extern result_t blecdev_adv_start( void );
    extern result_t blecdev_peer_conn_start( pm_peer_id_t peer_id );

    extern result_t blecdev_sec_bond_code_send( ble_bond_code_t * p_bond_code );

    extern uint16_t blecdev_conn_handle_get( void );

    extern uint32_t blecdev_peer_cnt_get( void );
    extern result_t blecdev_peer_list_get( pm_peer_id_t * p_peer_list, uint32_t * p_peer_cnt );
    extern result_t blecdev_peer_app_data_store( pm_peer_id_t peer_id, void * p_data, uint32_t len );
    extern result_t blecdev_peer_app_data_get( pm_peer_id_t peer_id, void * p_data, uint32_t * p_len );
    extern result_t blecdev_peer_erase( pm_peer_id_t peer_id );

    extern result_t blecdev_battery_level_set( uint8_t battery_level );

    extern void blecdev_run( void );

#ifdef __cplusplus
}
#endif


#endif // __BLE_COMPOSITE_DEVICE_H__
