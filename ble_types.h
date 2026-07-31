
#ifndef __BLE_TYPES_H_
#define __BLE_TYPES_H_

#include "dl_middleware.h"
#include "peer_manager_types.h"

#define BLE_DEVICE_NAME_LEN         32      /* Same value as flag _BLE_DEVICE_NAME_LEN defined in the Ble_composite_dev.c file. */
#define BLE_DEVICE_ADDRESS_LEN      6
#define BLE_BOND_CODE_LEN           6

#define BLE_INPUT_REPORT_LEN_RAW    200     /* Maximum length of the Input Report characteristic. */
#define BLE_OUTPUT_REPORT_LEN_RAW   200     /* Maximum length of Output Report. */

typedef struct ble_device_name{ char name[BLE_DEVICE_NAME_LEN]; }PACK ble_device_name_t;
typedef struct ble_device_addr{ uint8_t addr[BLE_DEVICE_ADDRESS_LEN]; }PACK ble_device_addr_t;
typedef struct ble_bond_code{ uint8_t code[BLE_DEVICE_ADDRESS_LEN]; }PACK ble_bond_code_t;

#endif /* __BLE_TYPES_H_ */
