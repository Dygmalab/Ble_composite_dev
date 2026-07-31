/*
Copyright (C) 2018,2019 Jim Jiang <jim@lotlab.org>
Copyright (C) 2026  Dygma Lab S.L.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include <strings.h>

#include "app_error.h"
#include "ble.h"
#include "ble_hids.h"

#include "Ble_composite_dev.h"
#include "ble_hid_service.h"
#include "hid_device.h"

/* nRF-level EXIT macro */
#define EXIT_IF_ERR_NRF( nrf_err, err, msg ) do{ err = ( nrf_err != NRF_SUCCESS ) ? RESULT_ERR : RESULT_OK; \
                                                 EXIT_IF_ERR( err, msg ); } while(0);

#define SEC_CURRENT SEC_JUST_WORKS
#define BASE_USB_HID_SPEC_VERSION 0x0101 /**< Version number of base USB HID Specification implemented by this application. */

//Make this not hardcorded
#define INPUT_REPORT_LEN_KEYBOARD 29  /**< Maximum length of the Input Report characteristic. */
#define OUTPUT_REPORT_LEN_KEYBOARD 1 /**< Maximum length of Output Report. */
#define INPUT_REPORT_LEN_MOUSE 5
#define INPUT_REPORT_LEN_SYSTEM 1
#define INPUT_REPORT_LEN_CONSUMER 8
#define INPUT_REP_INDEX_INVALID 0xFF /** Invalid index **/

enum input_report_index
{
    INPUT_REP_KBD_INDEX,
    INPUT_REP_MOUSE_INDEX,
    INPUT_REP_CONSUMER_INDEX,
    INPUT_REP_SYSTEM_INDEX,
    INPUT_REP_RAW_INDEX,
    INPUT_REP_COUNT
};

enum
{
    REPORT_ID_KEYBOARD = 1,
    REPORT_ID_MOUSE,
    REPORT_ID_CONSUMER_CONTROL,
    REPORT_ID_SYSTEM_CONTROL,
    REPORT_ID_RAW
};

enum output_report_index
{
    OUTPUT_REP_KBD_INDEX,
    OUTPUT_REP_RAW_INDEX,
    OUTPUT_REP_COUNT
};

/* HID Report Index Lookup table - Mapping the internal ID to HID report id */
const uint8_t hid_report_map_table[] = { INPUT_REP_INDEX_INVALID,  INPUT_REP_KBD_INDEX,    INPUT_REP_MOUSE_INDEX,
                                         INPUT_REP_CONSUMER_INDEX, INPUT_REP_SYSTEM_INDEX, INPUT_REP_RAW_INDEX };

typedef struct
{
    /* BLE Hids */
    ble_hids_t * p_ble_hids;

    /* Report arrays */
    ble_hids_inp_rep_init_t input_report_array[ INPUT_REP_COUNT ];
    ble_hids_outp_rep_init_t output_report_array[ OUTPUT_REP_COUNT ];

    /* HID descriptor */
    const uint8_t * p_desc_report;
    uint16_t desc_report_len;

    /* Flags */
    bool_t in_boot_mode;
} blehid_t;

static blehid_t blehid;

/***********************************************************************/
/*                       HID Report Setup Macros                       */
/***********************************************************************/

/** Quick HID param setup macro
 *
 * @param _name: name to setup
 * @param _len: report max length
 * @param _id: report id
 * @param _type: report type
 */
#define HID_REP_SETUP(_name, _len, _id, _type) \
    {                                          \
        _name.max_len = _len;                  \
        _name.rep_ref.report_id = _id;         \
        _name.rep_ref.report_type = _type;     \
        _name.sec.wr = SEC_CURRENT;            \
        _name.sec.rd = SEC_CURRENT;            \
    }

/** Setup Input report
 *
 * @param _name: name to setup
 * @param _len: report max length
 * @param _id: report id
 */
#define HID_REP_IN_SETUP(_name, _len, _id)                       \
    {                                                            \
        HID_REP_SETUP(_name, _len, _id, BLE_HIDS_REP_TYPE_INPUT) \
        _name.sec.cccd_wr = SEC_CURRENT;                         \
    }

/** Setup Output report
 *
 * @param _name: name to setup
 * @param _len: report max length
 * @param _id: report id
 */
#define HID_REP_OUT_SETUP(_name, _len, _id) HID_REP_SETUP(_name, _len, _id, BLE_HIDS_REP_TYPE_OUTPUT)

///** Setup Feature report
// *
// * @param _name: name to setup
// * @param _len: report max length
// * @param _id: report id
// */
//#define HID_REP_FEATURE_SETUP(_name, _len, _id) HID_REP_SETUP(_name, _len, _id, BLE_HIDS_REP_TYPE_FEATURE)


__attribute__ ((weak)) bool callBackRawHID(uint8_t *buff);


/**@brief Function for handling the HID Report Characteristic Write event.
 *
 * @param[in]   p_evt   HID service event.
 */
static INLINE void _hids_evt_rep_char_write_process( blehid_t * p_blehid, ble_hids_evt_t *p_evt )
{
    if (p_evt->params.char_write.char_id.rep_type == BLE_HIDS_REP_TYPE_OUTPUT)
    {
        ret_code_t err_code;
        uint8_t report_val;
        uint8_t report_index = p_evt->params.char_write.char_id.rep_index;
        uint16_t ble_conn_handle = blecdev_conn_handle_get();

        if (report_index == OUTPUT_REP_KBD_INDEX)
        {
            err_code = ble_hids_outp_rep_get( p_blehid->p_ble_hids, report_index, OUTPUT_REPORT_LEN_KEYBOARD, 0, ble_conn_handle, &report_val );
            ASSERT_DYGMA( err_code == NRF_SUCCESS, "ble_hids_outp_rep_get failed" );
        }
        if (report_index == OUTPUT_REP_RAW_INDEX)
        {
            uint8_t buff[BLE_OUTPUT_REPORT_LEN_RAW];
            err_code = ble_hids_outp_rep_get( p_blehid->p_ble_hids, report_index, BLE_OUTPUT_REPORT_LEN_RAW, 0, ble_conn_handle, buff );
            ASSERT_DYGMA( err_code == NRF_SUCCESS, "ble_hids_outp_rep_get failed" );
            if (err_code == NRF_SUCCESS)
            {
                callBackRawHID(buff);
            }
        }
    }
}

static INLINE void _hids_evt_handler( blehid_t * p_blehid, ble_hids_evt_t * p_evt )
{
    switch ( p_evt->evt_type )
    {
        case BLE_HIDS_EVT_BOOT_MODE_ENTERED:

            p_blehid->in_boot_mode = true;

            break;

        case BLE_HIDS_EVT_REPORT_MODE_ENTERED:

            p_blehid->in_boot_mode = false;

            break;

        case BLE_HIDS_EVT_REP_CHAR_WRITE:

            _hids_evt_rep_char_write_process( p_blehid, p_evt );

            break;

        case BLE_HIDS_EVT_NOTIF_ENABLED:
        case BLE_HIDS_EVT_REPORT_READ:

            /*
             * According to the SDK examples - No implementation needed.
             */

            break;

        default:

            ASSERT_DYGMA( false, "Unhandled BLE HIDS event" );

            break;
    }
}

static void _hids_evt_handler_nrf( ble_hids_t * p_hids, ble_hids_evt_t * p_evt )
{
    ASSERT_DYGMA( blehid.p_ble_hids == p_hids, "BLE HIDS inconsistent" );

    _hids_evt_handler( &blehid, p_evt );
}

void _hids_srv_error_handler_nrf( uint32_t nrf_error )
{
    APP_ERROR_HANDLER( nrf_error );
    ASSERT_DYGMA( false, "Unhandled BLE HID SRV error" );
}


static INLINE result_t _init( blehid_t * p_blehid )
{
    /* BLE Hids instance */
    BLE_HIDS_DEF(ble_hids, /**< Structure used to identify the HID service. */
                 NRF_SDH_BLE_TOTAL_LINK_COUNT, INPUT_REPORT_LEN_KEYBOARD, INPUT_REPORT_LEN_MOUSE, INPUT_REPORT_LEN_CONSUMER, INPUT_REPORT_LEN_SYSTEM,
                 OUTPUT_REPORT_LEN_KEYBOARD, BLE_INPUT_REPORT_LEN_RAW, BLE_OUTPUT_REPORT_LEN_RAW);

    p_blehid->p_ble_hids = &ble_hids;

    memset((void *)p_blehid->input_report_array, 0, sizeof(ble_hids_inp_rep_init_t) * INPUT_REP_COUNT);
    memset((void *)p_blehid->output_report_array, 0, sizeof(ble_hids_outp_rep_init_t) * OUTPUT_REP_COUNT);

    // Initialize HID Service
    HID_REP_IN_SETUP(p_blehid->input_report_array[INPUT_REP_KBD_INDEX], INPUT_REPORT_LEN_KEYBOARD, REPORT_ID_KEYBOARD);

    // keyboard led report
    HID_REP_OUT_SETUP(p_blehid->output_report_array[OUTPUT_REP_KBD_INDEX], OUTPUT_REPORT_LEN_KEYBOARD, REPORT_ID_KEYBOARD);

    HID_REP_IN_SETUP(p_blehid->input_report_array[INPUT_REP_MOUSE_INDEX], INPUT_REPORT_LEN_MOUSE, REPORT_ID_MOUSE);
    // system input report
    HID_REP_IN_SETUP(p_blehid->input_report_array[INPUT_REP_SYSTEM_INDEX], INPUT_REPORT_LEN_SYSTEM, REPORT_ID_SYSTEM_CONTROL);
    // consumer input report
    HID_REP_IN_SETUP(p_blehid->input_report_array[INPUT_REP_CONSUMER_INDEX], INPUT_REPORT_LEN_CONSUMER, REPORT_ID_CONSUMER_CONTROL);

    // Raw input report
    HID_REP_IN_SETUP(p_blehid->input_report_array[INPUT_REP_RAW_INDEX], BLE_INPUT_REPORT_LEN_RAW, REPORT_ID_RAW);
    // Raw output report
    HID_REP_OUT_SETUP(p_blehid->output_report_array[OUTPUT_REP_RAW_INDEX], BLE_OUTPUT_REPORT_LEN_RAW, REPORT_ID_RAW);

    /* Flags */
    p_blehid->in_boot_mode = false;

    return RESULT_OK;
}

static INLINE result_t _enable( blehid_t * p_blehid )
{
    ret_code_t err_code;
    result_t result = RESULT_ERR;

    ble_hids_init_t hids_init;

    /* Prepare the HIDS initialization structure */
    memset( &hids_init, 0, sizeof( hids_init ) );

    hids_init.evt_handler = _hids_evt_handler_nrf;
    hids_init.error_handler = _hids_srv_error_handler_nrf;
    hids_init.is_kb = true;
    hids_init.is_mouse = true;
    hids_init.inp_rep_count = INPUT_REP_COUNT;
    hids_init.p_inp_rep_array = p_blehid->input_report_array;
    hids_init.outp_rep_count = OUTPUT_REP_COUNT;
    hids_init.p_outp_rep_array = p_blehid->output_report_array;
    hids_init.feature_rep_count = 0;
    hids_init.p_feature_rep_array = NULL;
    hids_init.rep_map.data_len = p_blehid->desc_report_len;
    hids_init.rep_map.p_data = (uint8_t *)p_blehid->p_desc_report;
    hids_init.hid_information.bcd_hid = BASE_USB_HID_SPEC_VERSION;
    hids_init.hid_information.b_country_code = 0;
    hids_init.hid_information.flags = HID_INFO_FLAG_REMOTE_WAKE_MSK | HID_INFO_FLAG_NORMALLY_CONNECTABLE_MSK;
    hids_init.included_services_count = 0;
    hids_init.p_included_services_array = NULL;

    hids_init.rep_map.rd_sec = SEC_CURRENT;
    hids_init.hid_information.rd_sec = SEC_CURRENT;

    hids_init.boot_kb_inp_rep_sec.cccd_wr = SEC_CURRENT;
    hids_init.boot_kb_inp_rep_sec.rd = SEC_CURRENT;

    hids_init.boot_kb_outp_rep_sec.rd = SEC_CURRENT;
    hids_init.boot_kb_outp_rep_sec.wr = SEC_CURRENT;

    hids_init.boot_mouse_inp_rep_sec.cccd_wr = SEC_CURRENT;
    hids_init.boot_mouse_inp_rep_sec.wr = SEC_CURRENT;
    hids_init.boot_mouse_inp_rep_sec.rd = SEC_CURRENT;

    hids_init.protocol_mode_rd_sec = SEC_CURRENT;
    hids_init.protocol_mode_wr_sec = SEC_CURRENT;
    hids_init.ctrl_point_wr_sec = SEC_CURRENT;

    err_code = ble_hids_init( p_blehid->p_ble_hids, &hids_init );
    APP_ERROR_CHECK(err_code);
    EXIT_IF_ERR_NRF( err_code, result, "nrf_ble_gatt_init failed" );

_EXIT:
    return result;
}

static INLINE result_t _disable( blehid_t * p_blehid )
{
    /*
     * There is no BLE HIDS disable function. The actual disable should be done just by calling the _sd_disable.
     * Keeping this for code-styling purpose
     */

    /* Flags */
    p_blehid->in_boot_mode = false;

    return RESULT_OK;
}

static INLINE uint32_t _send_key( blehid_t * p_blehid, uint8_t index, const uint8_t *pattern, uint8_t len )
{
    ret_code_t err_code = NRF_SUCCESS;
    uint16_t ble_conn_handle = blecdev_conn_handle_get();

    if ( p_blehid->in_boot_mode == true )
    {
        if (index == 0)
        {
            err_code = ble_hids_boot_kb_inp_rep_send( p_blehid->p_ble_hids, len, (uint8_t *)pattern, ble_conn_handle);
        }
    }
    else
    {
        err_code = ble_hids_inp_rep_send( p_blehid->p_ble_hids, index, len, (uint8_t *)pattern, ble_conn_handle);
    }
    return err_code;
}

static INLINE result_t _send_report( blehid_t * p_blehid, uint8_t report_id, const uint8_t * p_key_pattern, uint8_t key_pattern_len )
{
    ret_code_t err_code;
    // check if report id overflow
    if (report_id >= sizeof(hid_report_map_table)) return false;
    // convert report id to index
    uint8_t report_index = hid_report_map_table[report_id];
    // check if this function is disable
    if (report_index == INPUT_REP_INDEX_INVALID) return false;

    err_code = _send_key( p_blehid, report_index, p_key_pattern, key_pattern_len);
    // check if send success, otherwise enqueue this.
    if (err_code == NRF_ERROR_RESOURCES)
    {
        return false;
    }

    if ((err_code != NRF_SUCCESS) && (err_code != NRF_ERROR_INVALID_STATE) && (err_code != NRF_ERROR_RESOURCES) && (err_code != NRF_ERROR_BUSY) &&
        (err_code != BLE_ERROR_GATTS_SYS_ATTR_MISSING) && (err_code != NRF_ERROR_FORBIDDEN))
    {
        APP_ERROR_HANDLER(err_code);
    }
    return true;
}

static INLINE void _set_report_descriptor( blehid_t * p_blehid, const uint8_t * p_desc_report, uint16_t len )
{
    p_blehid->p_desc_report = p_desc_report;
    p_blehid->desc_report_len = len;
}

/***********************************************************************/
/*                                 API                                 */
/***********************************************************************/

/**@brief Function for initializing BLE HID module.
 */
result_t blehid_init( void )
{
    return _init( &blehid );
}

/**@brief Function for enabling BLE HID module.
 */
result_t blehid_enable( void )
{
    return _enable( &blehid );
}

/**@brief Function for disabling BLE HID module.
 */
result_t blehid_disable( void )
{
    return _disable( &blehid );
}

/**@brief Function for sending sample key presses to the peer.
 *
 * @param[in]   report_id         Packet report ID. 0:keyboard, 1:mouse, 2:system, 3:consumer.
 * @param[in]   key_pattern_len   Pattern length.
 * @param[in]   p_key_pattern     Pattern to be sent.
 */
bool blehid_send_report( uint8_t report_id, const uint8_t * p_key_pattern, uint8_t key_pattern_len )
{
    return _send_report( &blehid, report_id, p_key_pattern, key_pattern_len );
}

void blehid_set_report_descriptor( const uint8_t * p_desc_report, uint16_t len )
{
    _set_report_descriptor( &blehid, p_desc_report, len );
}
