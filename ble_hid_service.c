/*
Copyright (C) 2018,2019 Jim Jiang <jim@lotlab.org>

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


/**
 * @brief HID Report Index Lookup table
 *
 * Mapping the internal ID to HID report id
 *
 */
uint8_t hid_report_map_table[] = {INPUT_REP_INDEX_INVALID,  INPUT_REP_KBD_INDEX,    INPUT_REP_MOUSE_INDEX,
                                  INPUT_REP_CONSUMER_INDEX, INPUT_REP_SYSTEM_INDEX, INPUT_REP_RAW_INDEX};

static bool m_in_boot_mode = false; /**< Current protocol mode. */


BLE_HIDS_DEF(m_hids, /**< Structure used to identify the HID service. */
             NRF_SDH_BLE_TOTAL_LINK_COUNT, INPUT_REPORT_LEN_KEYBOARD, INPUT_REPORT_LEN_MOUSE, INPUT_REPORT_LEN_CONSUMER, INPUT_REPORT_LEN_SYSTEM,
             OUTPUT_REPORT_LEN_KEYBOARD, INPUT_REPORT_LEN_RAW, OUTPUT_REPORT_LEN_RAW);


void service_error_handler(uint32_t nrf_error)
{
    APP_ERROR_HANDLER(nrf_error);
}

uint8_t keyboard_led_val_ble;

__attribute__ ((weak)) bool callBackRawHID(uint8_t *buff);


/**@brief Function for handling the HID Report Characteristic Write event.
 *
 * @param[in]   p_evt   HID service event.
 */
static void on_hid_rep_char_write(ble_hids_evt_t *p_evt)
{
    if (p_evt->params.char_write.char_id.rep_type == BLE_HIDS_REP_TYPE_OUTPUT)
    {
        ret_code_t err_code;
        uint8_t report_val;
        uint8_t report_index = p_evt->params.char_write.char_id.rep_index;

        if (report_index == OUTPUT_REP_KBD_INDEX)
        {
            err_code = ble_hids_outp_rep_get(&m_hids, report_index, OUTPUT_REPORT_LEN_KEYBOARD, 0, m_conn_handle, &report_val);

            if (err_code == NRF_SUCCESS)
            {
                keyboard_led_val_ble = report_val;
            }
        }
        if (report_index == OUTPUT_REP_RAW_INDEX)
        {
            uint8_t buff[OUTPUT_REPORT_LEN_RAW];
            err_code = ble_hids_outp_rep_get(&m_hids, report_index, OUTPUT_REPORT_LEN_RAW, 0, m_conn_handle, buff);
            if (err_code == NRF_SUCCESS)
            {
                callBackRawHID(buff);
            }
        }
    }
}


static void on_hids_evt(ble_hids_t *p_hids, ble_hids_evt_t *p_evt)
{
    switch (p_evt->evt_type)
    {
        case BLE_HIDS_EVT_BOOT_MODE_ENTERED:
            m_in_boot_mode = true;
            break;

        case BLE_HIDS_EVT_REPORT_MODE_ENTERED:
            m_in_boot_mode = false;
            break;

        case BLE_HIDS_EVT_REP_CHAR_WRITE:
            on_hid_rep_char_write(p_evt);
            break;

        case BLE_HIDS_EVT_NOTIF_ENABLED:
            break;

        default:
            // No implementation needed.
            break;
    }
}

static uint8_t *hid_desc_report;
static uint16_t hid_desc_report_len;
/**@brief Function for initializing HID Service.
 */
void hids_init()
{
    ret_code_t err_code;
    ble_hids_init_t hids_init_obj;

    static ble_hids_inp_rep_init_t input_report_array[INPUT_REP_COUNT];
    static ble_hids_outp_rep_init_t output_report_array[OUTPUT_REP_COUNT];

    memset((void *)input_report_array, 0, sizeof(ble_hids_inp_rep_init_t) * INPUT_REP_COUNT);
    memset((void *)output_report_array, 0, sizeof(ble_hids_outp_rep_init_t) * OUTPUT_REP_COUNT);

    // Initialize HID Service
    HID_REP_IN_SETUP(input_report_array[INPUT_REP_KBD_INDEX], INPUT_REPORT_LEN_KEYBOARD, REPORT_ID_KEYBOARD);

    // keyboard led report
    HID_REP_OUT_SETUP(output_report_array[OUTPUT_REP_KBD_INDEX], OUTPUT_REPORT_LEN_KEYBOARD, REPORT_ID_KEYBOARD);

    HID_REP_IN_SETUP(input_report_array[INPUT_REP_MOUSE_INDEX], INPUT_REPORT_LEN_MOUSE, REPORT_ID_MOUSE);
    // system input report
    HID_REP_IN_SETUP(input_report_array[INPUT_REP_SYSTEM_INDEX], INPUT_REPORT_LEN_SYSTEM, REPORT_ID_SYSTEM_CONTROL);
    // consumer input report
    HID_REP_IN_SETUP(input_report_array[INPUT_REP_CONSUMER_INDEX], INPUT_REPORT_LEN_CONSUMER, REPORT_ID_CONSUMER_CONTROL);

    // Raw input report
    HID_REP_IN_SETUP(input_report_array[INPUT_REP_RAW_INDEX], INPUT_REPORT_LEN_RAW, REPORT_ID_RAW);
    // Raw output report
    HID_REP_OUT_SETUP(output_report_array[OUTPUT_REP_RAW_INDEX], OUTPUT_REPORT_LEN_RAW, REPORT_ID_RAW);

    memset(&hids_init_obj, 0, sizeof(hids_init_obj));

    hids_init_obj.evt_handler = on_hids_evt;
    hids_init_obj.error_handler = service_error_handler;
    hids_init_obj.is_kb = true;
    hids_init_obj.is_mouse = true;
    hids_init_obj.inp_rep_count = INPUT_REP_COUNT;
    hids_init_obj.p_inp_rep_array = input_report_array;
    hids_init_obj.outp_rep_count = OUTPUT_REP_COUNT;
    hids_init_obj.p_outp_rep_array = output_report_array;
    hids_init_obj.feature_rep_count = 0;
    hids_init_obj.p_feature_rep_array = NULL;
    hids_init_obj.rep_map.data_len = hid_desc_report_len;
    hids_init_obj.rep_map.p_data = hid_desc_report;
    hids_init_obj.hid_information.bcd_hid = BASE_USB_HID_SPEC_VERSION;
    hids_init_obj.hid_information.b_country_code = 0;
    hids_init_obj.hid_information.flags = HID_INFO_FLAG_REMOTE_WAKE_MSK | HID_INFO_FLAG_NORMALLY_CONNECTABLE_MSK;
    hids_init_obj.included_services_count = 0;
    hids_init_obj.p_included_services_array = NULL;

    hids_init_obj.rep_map.rd_sec = SEC_CURRENT;
    hids_init_obj.hid_information.rd_sec = SEC_CURRENT;

    hids_init_obj.boot_kb_inp_rep_sec.cccd_wr = SEC_CURRENT;
    hids_init_obj.boot_kb_inp_rep_sec.rd = SEC_CURRENT;

    hids_init_obj.boot_kb_outp_rep_sec.rd = SEC_CURRENT;
    hids_init_obj.boot_kb_outp_rep_sec.wr = SEC_CURRENT;

    hids_init_obj.boot_mouse_inp_rep_sec.cccd_wr = SEC_CURRENT;
    hids_init_obj.boot_mouse_inp_rep_sec.wr = SEC_CURRENT;
    hids_init_obj.boot_mouse_inp_rep_sec.rd = SEC_CURRENT;

    hids_init_obj.protocol_mode_rd_sec = SEC_CURRENT;
    hids_init_obj.protocol_mode_wr_sec = SEC_CURRENT;
    hids_init_obj.ctrl_point_wr_sec = SEC_CURRENT;

    err_code = ble_hids_init(&m_hids, &hids_init_obj);
    APP_ERROR_CHECK(err_code);
}

static uint32_t send_key(ble_hids_t *p_hids, uint8_t index, uint8_t *pattern, uint8_t len)
{
    ret_code_t err_code = NRF_SUCCESS;
    if (m_in_boot_mode)
    {
        if (index == 0)
        {
            err_code = ble_hids_boot_kb_inp_rep_send(p_hids, len, pattern, m_conn_handle);
        }
    }
    else
    {
        err_code = ble_hids_inp_rep_send(p_hids, index, len, pattern, m_conn_handle);
    }
    return err_code;
}


/**@brief Function for sending sample key presses to the peer.
 *
 * @param[in]   report_id         Packet report ID. 0:keyboard, 1:mouse, 2:system, 3:consumer.
 * @param[in]   key_pattern_len   Pattern length.
 * @param[in]   p_key_pattern     Pattern to be sent.
 */
bool ble_send_report(uint8_t report_id, const uint8_t *p_key_pattern, uint8_t key_pattern_len)
{
    ret_code_t err_code;
    // check if report id overflow
    if (report_id >= sizeof(hid_report_map_table)) return false;
    // convert report id to index
    uint8_t report_index = hid_report_map_table[report_id];
    // check if this function is disable
    if (report_index == INPUT_REP_INDEX_INVALID) return false;

    err_code = send_key(&m_hids, report_index, p_key_pattern, key_pattern_len);
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

void ble_set_report_descriptor(const uint8_t *desc_report, uint16_t len)
{
    hid_desc_report = desc_report;
    hid_desc_report_len = len;
}
