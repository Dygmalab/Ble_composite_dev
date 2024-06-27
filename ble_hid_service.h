/*
    Copyright (C) 2018,2019 Jim Jiang <jim@lotlab.org>
    Copyright (C) 2020  Dygma Lab S.L.

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

#pragma once
#ifdef __cplusplus
extern "C"
{
#endif

#include <stdbool.h>
#include <stdint.h>

#define INPUT_REPORT_LEN_RAW 200  /**< Maximum length of the Input Report characteristic. */
#define OUTPUT_REPORT_LEN_RAW 200 /**< Maximum length of Output Report. */

void hids_init();
void ble_set_report_descriptor(const uint8_t *desc_report, uint16_t len);
bool ble_send_report(uint8_t report_id, const uint8_t *p_key_pattern,uint8_t key_pattern_len);

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

/** Setup Feature report
 * 
 * @param _name: name to setup
 * @param _len: report max length
 * @param _id: report id
 */
#define HID_REP_FEATURE_SETUP(_name, _len, _id) HID_REP_SETUP(_name, _len, _id, BLE_HIDS_REP_TYPE_FEATURE)

#ifdef __cplusplus
}
#endif