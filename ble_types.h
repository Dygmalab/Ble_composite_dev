
#ifndef __BLE_TYPES_H_
#define __BLE_TYPES_H_

#include "dl_middleware.h"
#include "peer_manager_types.h"

#define BLE_DEVICE_NAME_LEN     32  // Same value as flag _BLE_DEVICE_NAME_LEN defined in the Ble_composite_dev.c file.
#define BLE_DEVICE_ADDRESS_LEN  6

typedef struct ble_device_name{ char name[BLE_DEVICE_NAME_LEN]; }PACK ble_device_name_t;
typedef struct ble_device_addr { uint8_t addr[BLE_DEVICE_ADDRESS_LEN]; }PACK ble_device_addr_t;

#endif /* __BLE_TYPES_H_ */
