#include "Ble_composite_dev.h"
#include "ble_hid_service.h"


#define BLUETOOTH_DEBUG_LOG     0   /* 0 to 4 */
#define DEBUG_BLE_ENCRYPTION    1

#define _BLE_DEVICE_NAME_LEN    32


//MITM Manager
static bool flag_security_proc_started = false;
static bool flag_security_proc_failed = false;
// settings
static char defy_ble_name[_BLE_DEVICE_NAME_LEN + 6];  // Plus 6 for " - channel_number\0", where channel_number is a 2 digits number.
static uint8_t connected_device_name[_BLE_DEVICE_NAME_LEN];  // Declared as uint8_t * because that is what the SDK uses.
static uint8_t connected_device_address[BLE_GAP_ADDR_LEN];

static bool active_whitelist_flag = false;
static uint8_t current_channel = 0xFF;

static bool flag_ble_innited = false;
static bool flag_ble_connected = false;
static bool flag_ble_is_adv_mode = false;
static bool flag_ble_is_idle = false;
uint16_t m_conn_handle = BLE_CONN_HANDLE_INVALID; /* Handle of the current connection. */
static pm_peer_id_t m_peer_id;                           /* Device reference handle to the current bonded central. */
static bool flag_peer_deleted = false;
static bool flag_all_peers_deleted = false;
static bool flag_connected_device_name_changed = false;
static ble_uuid_t m_adv_uuids[] = {{BLE_UUID_HUMAN_INTERFACE_DEVICE_SERVICE, BLE_UUID_TYPE_BLE}};

BLE_BAS_DEF(m_bas);                 /* Structure used to identify the battery service. */
NRF_BLE_GATT_DEF(m_gatt);           /* GATT module instance. */
NRF_BLE_QWR_DEF(m_qwr);             /* Context for the Queued Write module.*/
BLE_ADVERTISING_DEF(m_advertising); /* Advertising module instance. */


static void power_management_init(void);
static void ble_stack_init(void);
static void scheduler_init(void);
static void gatt_init(void);
static void services_init(void);
static void conn_params_init(void);
static void peer_manager_init(void);

static void on_adv_evt(ble_adv_evt_t ble_adv_evt);
static void ble_advertising_error_handler(uint32_t nrf_error);
static void identities_set(pm_peer_id_list_skip_t skip);

static void qwr_init(void);
static void nrf_qwr_error_handler(uint32_t nrf_error);
static void dis_init(void);
static void bas_init(void);
//static void service_error_handler(uint32_t nrf_error);

static void conn_params_error_handler(uint32_t nrf_error);

static void peer_manager_event_handler(pm_evt_t const *p_evt);
static void whitelist_set(pm_peer_id_list_skip_t skip);

static void ble_event_handler(ble_evt_t const *ble_event, void *context);
static void save_connected_device_name(uint8_t *name, uint16_t len);

EventHandlerDeviceName_t evenHandlerDeviceName = NULL;


void ble_module_init(void)
{
    power_management_init();
    ble_stack_init();
    scheduler_init();
    gap_params_init();
    gatt_init();
    advertising_init();
    services_init();
    conn_params_init();
    peer_manager_init();
    flag_ble_innited = true;
}

static void power_management_init(void)
{
    /*
        Function for initializing power management.
    */
    ret_code_t err_code;
    err_code = nrf_pwr_mgmt_init();
    APP_ERROR_CHECK(err_code);
}

static void ble_stack_init(void)
{
    /*
        Function for initializing the BLE stack.
        Initializes the SoftDevice and the BLE event interrupt.
    */

    ret_code_t err_code;

    err_code = nrf_sdh_enable_request();
    APP_ERROR_CHECK(err_code);

    // Configure the BLE stack using the default settings.
    // Fetch the start address of the application RAM.
    uint32_t ram_start_addr = 0;
    err_code = nrf_sdh_ble_default_cfg_set(APP_BLE_CONN_CFG_TAG, &ram_start_addr);
    APP_ERROR_CHECK(err_code);

    // Enable BLE stack.
    err_code = nrf_sdh_ble_enable(&ram_start_addr);
    APP_ERROR_CHECK(err_code);

    // Register a handler for BLE events.
    NRF_SDH_BLE_OBSERVER(m_ble_observer, APP_BLE_OBSERVER_PRIO, ble_event_handler, NULL);
}

static void scheduler_init(void)
{
    /*
        Function for the Event Scheduler initialization.
    */
    APP_SCHED_INIT(SCHED_MAX_EVENT_DATA_SIZE, SCHED_QUEUE_SIZE);
}

void gap_params_init(void)
{
    /*
        Function for the GAP initialization.
        This function sets up all the necessary GAP (Generic Access Profile) parameters of the
        device including the device name, appearance, and the preferred connection parameters.
    */

    ret_code_t err_code;
    ble_gap_conn_params_t gap_conn_params;
    ble_gap_conn_sec_mode_t sec_mode;

    BLE_GAP_CONN_SEC_MODE_SET_ENC_WITH_MITM(&sec_mode);

    err_code = sd_ble_gap_device_name_set(&sec_mode, (uint8_t *)defy_ble_name, strlen(defy_ble_name));
    APP_ERROR_CHECK(err_code);

    err_code = sd_ble_gap_appearance_set(BLE_APPEARANCE_HID_KEYBOARD);
    APP_ERROR_CHECK(err_code);

    memset(&gap_conn_params, 0, sizeof(gap_conn_params));

    gap_conn_params.min_conn_interval = MIN_CONN_INTERVAL;
    gap_conn_params.max_conn_interval = MAX_CONN_INTERVAL;
    gap_conn_params.slave_latency = SLAVE_LATENCY;
    gap_conn_params.conn_sup_timeout = CONN_SUP_TIMEOUT;

    err_code = sd_ble_gap_ppcp_set(&gap_conn_params);
    APP_ERROR_CHECK(err_code);
}

static void gatt_init(void)
{
    /*
        Function for initializing the GATT module.
    */
    ret_code_t err_code = nrf_ble_gatt_init(&m_gatt, NULL);
    APP_ERROR_CHECK(err_code);
}

void advertising_init(void)
{
    /*
        Function for initializing the Advertising functionality.
    */

    uint32_t err_code;
    ble_advertising_init_t init;

    memset(&init, 0, sizeof(init));

    init.advdata.name_type = BLE_ADVDATA_FULL_NAME;
    init.advdata.include_appearance = true;
    init.advdata.flags = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE;
    init.advdata.uuids_complete.uuid_cnt = sizeof(m_adv_uuids) / sizeof(m_adv_uuids[0]);
    init.advdata.uuids_complete.p_uuids = m_adv_uuids;

    init.config.ble_adv_whitelist_enabled = active_whitelist_flag;
    init.config.ble_adv_directed_high_duty_enabled = true;
    init.config.ble_adv_directed_enabled = false;
    init.config.ble_adv_directed_interval = 0;
    init.config.ble_adv_directed_timeout = 0;
    init.config.ble_adv_fast_enabled = true;
    init.config.ble_adv_fast_interval = APP_ADV_FAST_INTERVAL;
    init.config.ble_adv_fast_timeout = APP_ADV_FAST_DURATION;
    init.config.ble_adv_slow_enabled = true;
    init.config.ble_adv_slow_interval = APP_ADV_SLOW_INTERVAL;
    init.config.ble_adv_slow_timeout = APP_ADV_SLOW_DURATION;

    init.evt_handler = on_adv_evt;
    init.error_handler = ble_advertising_error_handler;

    err_code = ble_advertising_init(&m_advertising, &init);
    APP_ERROR_CHECK(err_code);

    ble_advertising_conn_cfg_tag_set(&m_advertising, APP_BLE_CONN_CFG_TAG);
}

static void on_adv_evt(ble_adv_evt_t ble_adv_evt)
{
    /**@brief Function for handling advertising events.
     *
     * @details This function will be called for advertising events which are passed to the application.
     *
     * @param[in] ble_adv_evt  Advertising event.
     */

    ret_code_t err_code;

    switch (ble_adv_evt)
    {
        case BLE_ADV_EVT_DIRECTED_HIGH_DUTY:
        {
            flag_ble_is_adv_mode = true;
            flag_ble_is_idle = false;
#if (BLUETOOTH_DEBUG_LOG > 0)
            NRF_LOG_INFO("<<< BLE: High Duty Directed advertising. >>>");
#endif
        }
        break;

        case BLE_ADV_EVT_DIRECTED:
        {
            flag_ble_is_adv_mode = true;
            flag_ble_is_idle = false;
#if (BLUETOOTH_DEBUG_LOG > 0)
            NRF_LOG_INFO("<<< BLE: Directed advertising. >>>");
#endif
        }
        break;

        case BLE_ADV_EVT_FAST:
        {
            flag_ble_is_idle = false;
            flag_ble_is_adv_mode = true;
#if (BLUETOOTH_DEBUG_LOG > 0)
            NRF_LOG_INFO("<<< BLE: Fast advertising. >>>");
#endif
        }
        break;

        case BLE_ADV_EVT_SLOW:
        {
            flag_ble_is_adv_mode = true;
#if (BLUETOOTH_DEBUG_LOG > 0)
            NRF_LOG_INFO("<<< BLE: Slow advertising. >>>");
#endif
        }
        break;

        case BLE_ADV_EVT_FAST_WHITELIST:
        {
            flag_ble_is_adv_mode = true;
            flag_ble_is_idle = false;
#if (BLUETOOTH_DEBUG_LOG > 0)
            NRF_LOG_INFO("<<< BLE: Fast advertising with whitelist. >>>");
#endif
        }
        break;

        case BLE_ADV_EVT_SLOW_WHITELIST:
        {
            flag_ble_is_adv_mode = true;
            flag_ble_is_idle = false;
#if (BLUETOOTH_DEBUG_LOG > 0)
            NRF_LOG_INFO("<<< BLE: Slow advertising with whitelist. >>>");
#endif
        }
        break;

        case BLE_ADV_EVT_IDLE:
        {
            flag_ble_is_adv_mode = false;
            flag_ble_is_idle = true;
#if (BLUETOOTH_DEBUG_LOG > 0)
            NRF_LOG_INFO("<<< BLE: Going to sleep.. >>>");
            NRF_LOG_FINAL_FLUSH();
#endif
        }
        break;

        case BLE_ADV_EVT_WHITELIST_REQUEST:
        {
            flag_ble_is_adv_mode = false;

            ble_gap_addr_t whitelist_addrs[BLE_GAP_WHITELIST_ADDR_MAX_COUNT];
            ble_gap_irk_t whitelist_irks[BLE_GAP_WHITELIST_ADDR_MAX_COUNT];
            uint32_t addr_cnt = BLE_GAP_WHITELIST_ADDR_MAX_COUNT;
            uint32_t irk_cnt = BLE_GAP_WHITELIST_ADDR_MAX_COUNT;

            err_code = pm_whitelist_get(whitelist_addrs, &addr_cnt, whitelist_irks, &irk_cnt);
            if (err_code == NRF_ERROR_NOT_FOUND)
            {
#if (BLUETOOTH_DEBUG_LOG > 2)
                NRF_LOG_DEBUG("BLE: The device was deleted can not connect.");
#endif
            }
            else
            {
                APP_ERROR_CHECK(err_code);
            }

#if (BLUETOOTH_DEBUG_LOG > 2)
            NRF_LOG_DEBUG("BLE: pm_whitelist_get() returns %d addr in whitelist and %d irk whitelist.", addr_cnt, irk_cnt);
#endif

            // Set the correct identities list (no excluding peers with no Central Address Resolution).
            identities_set(PM_PEER_ID_LIST_SKIP_NO_IRK);

            // Apply the whitelist.
            err_code = ble_advertising_whitelist_reply(&m_advertising, whitelist_addrs, addr_cnt, whitelist_irks, irk_cnt);
            APP_ERROR_CHECK(err_code);
        }
        break;

        case BLE_ADV_EVT_PEER_ADDR_REQUEST:
        {
            flag_ble_is_adv_mode = false;

            pm_peer_data_bonding_t peer_bonding_data;

            // Only Give peer address if we have a handle to the bonded peer.
            if (m_peer_id != PM_PEER_ID_INVALID)
            {
                err_code = pm_peer_data_bonding_load(m_peer_id, &peer_bonding_data);
                if (err_code != NRF_ERROR_NOT_FOUND)
                {
                    APP_ERROR_CHECK(err_code);

                    // Manipulate identities to exclude peers with no Central Address Resolution.
                    identities_set(PM_PEER_ID_LIST_SKIP_ALL);

                    ble_gap_addr_t *p_peer_addr = &(peer_bonding_data.peer_ble_id.id_addr_info);
                    err_code = ble_advertising_peer_addr_reply(&m_advertising, p_peer_addr);
                    APP_ERROR_CHECK(err_code);
                }
            }
        }
        break;

        default:
        {
            flag_ble_is_adv_mode = false;
        }
        break;
    }
}

static void ble_advertising_error_handler(uint32_t nrf_error)
{
    /*
        Function for handling advertising errors.

        param[in] nrf_error  Error code containing information about what went wrong.
    */
    APP_ERROR_HANDLER(nrf_error);
}

static void identities_set(pm_peer_id_list_skip_t skip)
{
    /*
        Function for setting filtered device identities.
        skip: Filter passed to @ref pm_peer_id_list.
    */

    pm_peer_id_t peer_ids[BLE_GAP_DEVICE_IDENTITIES_MAX_COUNT];
    uint32_t peer_id_count = BLE_GAP_DEVICE_IDENTITIES_MAX_COUNT;

    ret_code_t err_code = pm_peer_id_list(peer_ids, &peer_id_count, PM_PEER_ID_INVALID, skip);
    APP_ERROR_CHECK(err_code);

    err_code = pm_device_identities_list_set(peer_ids, peer_id_count);
    APP_ERROR_CHECK(err_code);
}

static void services_init(void)
{
    /*
        Function for initializing services that will be used by the application.
    */
    update_current_channel();
    qwr_init();
    dis_init();
    bas_init();
    hids_init();
}

void update_current_channel(void)
{
    ble_gap_addr_t addrGet = gap_addr_get();

    if(current_channel != addrGet.addr[0])
    {
        addrGet.addr[0] = current_channel;

#if (BLUETOOTH_DEBUG_LOG > 1)
        if (gap_addr_set(&addrGet))  // Change BLE channel.
        {
            NRF_LOG_DEBUG("BLE: Channel %i changed to %i", addrGet.addr[0], current_channel);
        }
        else
        {
            NRF_LOG_DEBUG("BLE: Error changing channel.");
        }
#else
        gap_addr_set(&addrGet);  // Change BLE channel.
#endif
    }
}

static void qwr_init(void)
{
    /*
        Function for initializing the Queued Write Module.
    */

    ret_code_t err_code;
    nrf_ble_qwr_init_t qwr_init_obj = {0};

    qwr_init_obj.error_handler = nrf_qwr_error_handler;

    err_code = nrf_ble_qwr_init(&m_qwr, &qwr_init_obj);
    APP_ERROR_CHECK(err_code);
}

static void nrf_qwr_error_handler(uint32_t nrf_error)
{
    /*
        Function for handling Queued Write Module errors.
        A pointer to this function will be passed to each service which may need to inform the
        application about an error.
        nrf_error: Error code containing information about what went wrong.
    */
    APP_ERROR_HANDLER(nrf_error);
}

/*
    Function for initializing Device Information Service.
*/
static void dis_init(void)
{
    ret_code_t err_code;
    ble_dis_init_t dis_init_obj;
    ble_dis_pnp_id_t pnp_id;

    pnp_id.vendor_id_source = PNP_ID_VENDOR_ID_SOURCE;
    pnp_id.vendor_id = PNP_ID_VENDOR_ID;
    pnp_id.product_id = PNP_ID_PRODUCT_ID;
    pnp_id.product_version = PNP_ID_PRODUCT_VERSION;

    memset(&dis_init_obj, 0, sizeof(dis_init_obj));

    ble_srv_ascii_to_utf8(&dis_init_obj.manufact_name_str, MANUFACTURER_NAME);
    dis_init_obj.p_pnp_id = &pnp_id;

    dis_init_obj.dis_char_rd_sec = SEC_JUST_WORKS;

    err_code = ble_dis_init(&dis_init_obj);
    APP_ERROR_CHECK(err_code);
}

/*
    Function for initializing Battery Service.
*/
static void bas_init(void)
{
    ret_code_t err_code;
    ble_bas_init_t bas_init_obj;

    memset(&bas_init_obj, 0, sizeof(bas_init_obj));

    bas_init_obj.evt_handler = NULL;
    bas_init_obj.support_notification = true;
    bas_init_obj.p_report_ref = NULL;
    bas_init_obj.initial_batt_level = 100;

    bas_init_obj.bl_rd_sec = SEC_JUST_WORKS;
    bas_init_obj.bl_cccd_wr_sec = SEC_JUST_WORKS;
    bas_init_obj.bl_report_rd_sec = SEC_JUST_WORKS;

    err_code = ble_bas_init(&m_bas, &bas_init_obj);
    APP_ERROR_CHECK(err_code);
}


//static void service_error_handler(uint32_t nrf_error)
//{
//    /*
//        Function for handling Service errors.
//        A pointer to this function will be passed to each service which may need to inform the
//        application about an error.
//
//        nrf_error: Error code containing information about what went wrong.
//    */
//    APP_ERROR_HANDLER(nrf_error);
//}

static void conn_params_init(void)
{
    /*
        Function for initializing the Connection Parameters module.
    */
    ret_code_t err_code;
    ble_conn_params_init_t cp_init;

    memset(&cp_init, 0, sizeof(cp_init));

    cp_init.p_conn_params = NULL;
    cp_init.first_conn_params_update_delay = FIRST_CONN_PARAMS_UPDATE_DELAY;
    cp_init.next_conn_params_update_delay = NEXT_CONN_PARAMS_UPDATE_DELAY;
    cp_init.max_conn_params_update_count = MAX_CONN_PARAMS_UPDATE_COUNT;
    cp_init.start_on_notify_cccd_handle = BLE_GATT_HANDLE_INVALID;
    cp_init.disconnect_on_fail = false;
    cp_init.evt_handler = NULL;
    cp_init.error_handler = conn_params_error_handler;

    err_code = ble_conn_params_init(&cp_init);
    APP_ERROR_CHECK(err_code);
}

static void conn_params_error_handler(uint32_t nrf_error)
{
    /*
        Function for handling a Connection Parameters error.
        nrf_error: Error code containing information about what went wrong.
    */
    APP_ERROR_HANDLER(nrf_error);
}

static void peer_manager_init(void)
{
    /*
        Function for the Peer Manager initialization.
    */

    ret_code_t err_code;

    err_code = pm_init();
    APP_ERROR_CHECK(err_code);

    // Set security parameters:
    ble_gap_sec_params_t sec_param;
    memset(&sec_param, 0, sizeof(ble_gap_sec_params_t));
    sec_param.bond = SEC_PARAM_BOND;
    sec_param.mitm = SEC_PARAM_MITM;
    sec_param.lesc = SEC_PARAM_LESC;
    sec_param.keypress = SEC_PARAM_KEYPRESS;
    sec_param.io_caps = SEC_PARAM_IO_CAPABILITIES;
    sec_param.oob = SEC_PARAM_OOB;
    sec_param.min_key_size = SEC_PARAM_MIN_KEY_SIZE;
    sec_param.max_key_size = SEC_PARAM_MAX_KEY_SIZE;
    sec_param.kdist_own.enc = 1;
    sec_param.kdist_own.id = 1;
    sec_param.kdist_peer.enc = 1;
    sec_param.kdist_peer.id = 1;

    err_code = pm_sec_params_set(&sec_param);
    APP_ERROR_CHECK(err_code);

    err_code = pm_register(peer_manager_event_handler);
    APP_ERROR_CHECK(err_code);
}

static void peer_manager_event_handler(pm_evt_t const *p_evt)
{
    pm_handler_on_pm_evt(p_evt);
    pm_handler_disconnect_on_sec_failure(p_evt);
    pm_handler_flash_clean(p_evt);

    switch (p_evt->evt_id)
    {
        case PM_EVT_CONN_SEC_START:
        {
            flag_security_proc_started = true;
            flag_security_proc_failed = false;

#if DEBUG_BLE_ENCRYPTION
            NRF_LOG_DEBUG("<<< BLE: Security procedure started. >>>");
            NRF_LOG_FLUSH();
#endif
        }
        break;
        case PM_EVT_CONN_SEC_FAILED:
        {
            flag_security_proc_started = false;
            flag_security_proc_failed = true;

#if DEBUG_BLE_ENCRYPTION
            NRF_LOG_DEBUG("<<< BLE: Security procedure failed. >>>");
            NRF_LOG_FLUSH();
#endif
        }
        break;
        case PM_EVT_CONN_SEC_SUCCEEDED:
        {
#if DEBUG_BLE_ENCRYPTION
            NRF_LOG_DEBUG("<<< BLE: PM_EVT_CONN_SEC_SUCCEEDED >>>");
            NRF_LOG_FLUSH();
#endif
            flag_ble_connected = true;
            m_peer_id = p_evt->peer_id;
        }
        break;

        case PM_EVT_PEER_DELETE_SUCCEEDED:
        {
#if (BLUETOOTH_DEBUG_LOG > 2)
            NRF_LOG_DEBUG("<<< BLE: PM_EVT_PEER_DELETE_SUCCEEDED >>>");
            NRF_LOG_FLUSH();
#endif

            flag_peer_deleted = true;
        }
        break;

        case PM_EVT_PEERS_DELETE_SUCCEEDED:
        {
#if (BLUETOOTH_DEBUG_LOG > 2)
            NRF_LOG_DEBUG("<<< BLE: PM_EVT_PEERS_DELETE_SUCCEEDED >>>");
            NRF_LOG_FLUSH();
#endif

            flag_all_peers_deleted = true;
        }
        break;

        case PM_EVT_PEER_DATA_UPDATE_SUCCEEDED:
        {
            if (p_evt->params.peer_data_update_succeeded.flash_changed && (p_evt->params.peer_data_update_succeeded.data_id == PM_PEER_DATA_ID_BONDING))
            {
#if (BLUETOOTH_DEBUG_LOG > 2)
                NRF_LOG_DEBUG("<<< BLE: New Bond, adding peer to the whitelist. >>>");
#endif
                // Note: You should check on what kind of white list policy your application should use.

                /*
                    If a new pairing has been created, update the whitelist to include it.

                    The PM_PEER_ID_LIST_SKIP_NO_ID_ADDR argument specifies that peers that do not have a standard public
                    BLE address (i.e., only have an Identity Resolving Key) should not be included in the peer ID list.
                */
                whitelist_set(PM_PEER_ID_LIST_SKIP_NO_ID_ADDR);
            }
        }
        break;

        default:
        {
        }
        break;
    }
}

bool get_flag_security_proc_started(void)
{
    return flag_security_proc_started;
}

void clear_flag_security_proc_started(void)
{
    flag_security_proc_started = false;
}

bool get_flag_security_proc_failed(void)
{
    return flag_security_proc_failed;
}

void clear_flag_security_proc_failed(void)
{
    flag_security_proc_failed = false;
}

void ble_send_encryption_pin(char const *pin_number)
{
    ret_code_t err_code = sd_ble_gap_auth_key_reply(m_conn_handle,
                                                    BLE_GAP_AUTH_KEY_TYPE_PASSKEY,
                                                    (const uint8_t *)pin_number);
    APP_ERROR_CHECK(err_code);
}

void ble_goto_advertising_mode(void)
{

    ret_code_t err_code;

    // Start advertising.
    err_code = ble_advertising_start(&m_advertising, BLE_ADV_MODE_FAST);

    if (err_code == NRF_ERROR_CONN_COUNT)
    {
        NRF_LOG_INFO("BLE: Maximum connection count exceeded.");
        return;
    }

    APP_ERROR_CHECK(err_code);

    NRF_LOG_INFO("BLE: Advertising mode.");
}

/**@brief Function for disabling advertising and scanning.
 */
void ble_adv_stop(void)
{
    ret_code_t ret = sd_ble_gap_adv_stop(m_advertising.adv_handle);
    if ((ret != NRF_SUCCESS) && (ret != NRF_ERROR_INVALID_STATE) && (ret != BLE_ERROR_INVALID_ADV_HANDLE))
    {
        APP_ERROR_CHECK(ret);
    }
}

void ble_goto_white_list_advertising_mode(void)
{
    /*
        The PM_PEER_ID_LIST_SKIP_NO_ID_ADDR argument specifies that peers that do not have a standard public
        BLE address (i.e., only have an Identity Resolving Key) should not be included in the peer ID list.
    */
    whitelist_set(PM_PEER_ID_LIST_SKIP_NO_ID_ADDR);

    ble_goto_advertising_mode();
}

static void whitelist_set(pm_peer_id_list_skip_t skip)
{
    /*
        Function for setting filtered whitelist.
        Obtains, from the flash memory, a list of paired devices (peers) that have previously been
        connected and setting them as a whitelist for future connections.
        The devices on this whitelist are the only ones your device will allow to connect when it is
        in advertising mode.

        skip: Filter passed to pm_peer_id_list() function.
    */

    pm_peer_id_t peer_ids[BLE_GAP_WHITELIST_ADDR_MAX_COUNT];
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
    ret_code_t err_code = pm_peer_id_list(peer_ids, &peer_id_count, PM_PEER_ID_INVALID, skip);
    APP_ERROR_CHECK(err_code);

#if (BLUETOOTH_DEBUG_LOG > 1)
    NRF_LOG_INFO("BLE: Peers in whitelist: %d, MAX_PEERS_WLIST: %d", peer_id_count, BLE_GAP_WHITELIST_ADDR_MAX_COUNT);
#endif

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
    err_code = pm_whitelist_set(peer_ids, peer_id_count);
#if (BLUETOOTH_DEBUG_LOG > 1)
    NRF_LOG_INFO("BLE: pm_whitelist_set() returns %d", err_code);
#endif
    APP_ERROR_CHECK(err_code);
}

bool ble_is_advertising_mode(void)
{
    return flag_ble_is_adv_mode;
}

bool ble_is_idle(void)
{
    return flag_ble_is_idle;
}

void ble_disconnect(void)
{
    if (!ble_connected()) return;

    if (m_conn_handle != BLE_CONN_HANDLE_INVALID)
    {
        sd_ble_gap_disconnect(m_conn_handle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
        while (flag_ble_connected) ble_run(); // Wait until disconnecting procedure ends.
    }
}

ble_gap_addr_t gap_addr_get()
{
    ble_gap_addr_t gap_addr;
    sd_ble_gap_addr_get(&gap_addr);
    return gap_addr;
}

bool gap_addr_set(ble_gap_addr_t *gap_addr)
{
    uint32_t addr_set_return_code = sd_ble_gap_addr_set(gap_addr);
    return addr_set_return_code == 0;
}

void delete_peers(void)
{
    /*
        Clear bond information from persistent storage.
        First you need to disconnect BLE.
    */

    ret_code_t err_code;

#if (BLUETOOTH_DEBUG_LOG > 2)
    NRF_LOG_INFO("BLE: Deleting paired devices...");
#endif

    flag_all_peers_deleted = false;
    err_code = pm_peers_delete();
    APP_ERROR_CHECK(err_code);

    while (!flag_all_peers_deleted)
        ble_run(); // Wait until delet procedure ends.
#if (BLUETOOTH_DEBUG_LOG > 2)
    NRF_LOG_INFO("BLE: Done");
#endif
}

/**
 * @brief Function for deleting a bond by its peer id.
 *
 * @details This function is used to delete a bond associated with a specific peer id.
 * The function calls the Nordic SDK function pm_peer_delete() and passes the given peer id.
 * Any error returned by pm_peer_delete() is checked and handled by APP_ERROR_CHECK().
 *
 * @param[in]   peer_id  The id of the peer whose bond we want to delete.
 */
void delete_peer_by_id(pm_peer_id_t peer_id)
{
    if (peer_id == PM_PEER_ID_INVALID) return;

#if (BLUETOOTH_DEBUG_LOG > 2)
    NRF_LOG_INFO("BLE: Deleting paired device ID=%d from flash memory...", peer_id);
#endif

    flag_peer_deleted = false;
    ret_code_t ret = pm_peer_delete(peer_id);
    APP_ERROR_CHECK(ret);

    while (!flag_peer_deleted)
        ble_run(); // Wait until delet procedure ends.
#if (BLUETOOTH_DEBUG_LOG > 2)
    NRF_LOG_INFO("BLE: Done");
#endif
}

/**
 * @brief Function for getting the next peer id.
 *
 * @details This function wraps the pm_next_peer_id_get() function of the Nordic SDK.
 * It is used to iterate through all peer IDs stored in the flash memory. The input argument
 * defines the starting point for the search of the next peer ID.
 *
 * @param[in]   peer_id  The peer ID from where to get the next peer ID. To get the first peer ID, use PM_PEER_ID_INVALID.
 *
 * @return      The next peer id. If there is no more peer ID, PM_PEER_ID_INVALID will be returned.
 */
pm_peer_id_t get_next_peer_id(pm_peer_id_t peer_id)
{
    return pm_next_peer_id_get(peer_id);
}

void ble_run(void)
{
    /*
        Function for handling the idle state (main loop).
        If there is no pending log operation, then sleep until the next event occurs.
    */

    app_sched_execute();

    if (NRF_LOG_PROCESS() == false)
    {
        nrf_pwr_mgmt_run();
    }
}

void save_connected_device_name(uint8_t *name, uint16_t len)
{
    if (name)  // pass NULL to skip copy
    {
        memcpy(connected_device_name, name, len);
    }
    flag_connected_device_name_changed = true;

#if (BLUETOOTH_DEBUG_LOG > 0)
    NRF_LOG_DEBUG("BLE: New connected_device_name = %s, len = %i", connected_device_name, len);
#endif
}

uint8_t *get_connected_device_name_ptr(void)
{
    return connected_device_name;
}

pm_peer_id_t get_connected_peer_id(void)
{
    return m_peer_id;
}

void save_connected_device_address(ble_gap_addr_t gapAddr)
{
    memcpy(connected_device_address, gapAddr.addr, BLE_GAP_ADDR_LEN);
#if (BLUETOOTH_DEBUG_LOG > 0)
    NRF_LOG_DEBUG("BLE: peer addr saved = %02X %02X %02X %02X %02X %02X",
                  connected_device_address[0], connected_device_address[1],
                  connected_device_address[2], connected_device_address[3],
                  connected_device_address[4], connected_device_address[5]);
#endif
}

uint8_t *get_connected_device_address(void)
{
    return connected_device_address;
}

static void ble_event_handler(ble_evt_t const *ble_event, void *context)
{
    /*
        Function for handling BLE events.

        ble_event: Bluetooth stack event.
        context: Unused.
    */

    ret_code_t err_code;

    switch (ble_event->header.evt_id)
    {
        case BLE_GATTC_EVT_CHAR_VAL_BY_UUID_READ_RSP:
        {
            ble_gattc_evt_char_val_by_uuid_read_rsp_t *rd_rsp = (ble_gattc_evt_char_val_by_uuid_read_rsp_t *)&ble_event->evt.gattc_evt.params.char_val_by_uuid_read_rsp;

            if (rd_rsp->count)
            {
                ble_gattc_handle_value_t hdl_value = {0, NULL};

                if ( NRF_SUCCESS == sd_ble_gattc_evt_char_val_by_uuid_read_rsp_iter((ble_gattc_evt_t *)&ble_event->evt.gattc_evt,
                                                                                   &hdl_value) )
                {
                    save_connected_device_name(hdl_value.p_value, rd_rsp->value_len);

                    if(evenHandlerDeviceName != NULL)
                    {
                        evenHandlerDeviceName();
                    }
                }
            }
        }
        break;

#if DEBUG_BLE_ENCRYPTION
        case BLE_GAP_EVT_AUTH_STATUS:
        {
            /*
             * If the peer ignores the request, a BLE_GAP_EVT_AUTH_STATUS event occurs with the status
             * BLE_GAP_SEC_STATUS_TIMEOUT. Otherwise, the peer initiates security, in which case
             * things happen as if the peer had initiated security itself. See PM_EVT_CONN_SEC_START
             * for information about peer-initiated security.
             */
            if (ble_event->evt.gap_evt.params.auth_status.auth_status == BLE_GAP_SEC_STATUS_TIMEOUT)
            {
                NRF_LOG_DEBUG("<<< BLE: Security request fail. >>>");
                NRF_LOG_FLUSH();
            }
            else
            {
                NRF_LOG_DEBUG("<<< BLE: Security request accepted by the master and initiated. >>>");
                NRF_LOG_FLUSH();
            }
        }
        break;
#endif

        case BLE_GAP_EVT_AUTH_KEY_REQUEST:
        {
#if (BLUETOOTH_DEBUG_LOG > 0)
            NRF_LOG_INFO("<<< BLE_GAP_EVT_AUTH_KEY_REQUEST >>>");
            NRF_LOG_FLUSH();
#endif
        }
        break;

        case BLE_GAP_EVT_CONNECTED:
        {
#if (BLUETOOTH_DEBUG_LOG > 0)
            NRF_LOG_INFO("<<< BLE connected >>>");
#endif
            flag_ble_is_adv_mode = false;
            m_conn_handle = ble_event->evt.gap_evt.conn_handle;
            ble_gap_evt_connected_t connected_evt = ble_event->evt.gap_evt.params.connected;
            save_connected_device_address(connected_evt.peer_addr);
            err_code = nrf_ble_qwr_conn_handle_assign(&m_qwr, m_conn_handle);
            APP_ERROR_CHECK(err_code);
        }
        break;

        case BLE_GAP_EVT_DISCONNECTED:
        {
#if (BLUETOOTH_DEBUG_LOG > 0)
            NRF_LOG_INFO("<<< BLE disconnected >>>");
#endif

            flag_ble_connected = false;

            m_conn_handle = BLE_CONN_HANDLE_INVALID;
        }
        break;

        case BLE_GAP_EVT_PHY_UPDATE_REQUEST:
        {
#if (BLUETOOTH_DEBUG_LOG > 0)
            NRF_LOG_DEBUG("<<< BLE: PHY update request >>>");
#endif

            ble_gap_phys_t const phys = {
                .tx_phys = BLE_GAP_PHY_AUTO,
                .rx_phys = BLE_GAP_PHY_AUTO,
            };
            err_code = sd_ble_gap_phy_update(ble_event->evt.gap_evt.conn_handle, &phys);
            APP_ERROR_CHECK(err_code);
        }
        break;

        case BLE_GATTS_EVT_HVN_TX_COMPLETE:
        {
            //Here should be the call to the ble hid service
#if (BLUETOOTH_DEBUG_LOG > 4)
            NRF_LOG_DEBUG("<<< BLE: Report sent >>>");
#endif
        }
        break;

        case BLE_GATTC_EVT_TIMEOUT:
        {
// Disconnect on GATT Client timeout event.
#if (BLUETOOTH_DEBUG_LOG > 0)
            NRF_LOG_DEBUG("<<< BLE: GATT Client Timeout >>>");
#endif
            err_code = sd_ble_gap_disconnect(ble_event->evt.gattc_evt.conn_handle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
            APP_ERROR_CHECK(err_code);
        }
        break;

        case BLE_GATTS_EVT_TIMEOUT:
        {
// Disconnect on GATT Server timeout event.
#if (BLUETOOTH_DEBUG_LOG > 0)
            NRF_LOG_DEBUG("<<< BLE: GATT Server Timeout >>>");
#endif
            err_code = sd_ble_gap_disconnect(ble_event->evt.gatts_evt.conn_handle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
            APP_ERROR_CHECK(err_code);
        }
        break;
    }
}

void ble_get_device_name(EventHandlerDeviceName_t evenHandler)
{
    evenHandlerDeviceName = evenHandler;  // Set the handler to get the host BLE device name.

    // Ask the soft device to give us the device name.
    ble_gattc_handle_range_t hdl_range = {.start_handle = 1, .end_handle = 0xffff};
    ble_uuid_t bleUuid = {BLE_UUID_GAP_CHARACTERISTIC_DEVICE_NAME, BLE_UUID_TYPE_BLE};
    sd_ble_gattc_char_value_by_uuid_read(m_conn_handle, &bleUuid, &hdl_range);
}

bool ble_connected(void)
{
    return flag_ble_connected;
}

bool ble_innited(void)
{
    return flag_ble_innited;
}

bool ble_get_flag_connection_name_changed(void)
{
    return flag_connected_device_name_changed;
}

void ble_set_flag_connection_name_changed(bool flag)
{
    flag_connected_device_name_changed = flag;
}


void ble_battery_level_update(uint8_t battery_level)
{
    if (!ble_connected())
    {
        return;
    }

    ret_code_t err_code;

    err_code = ble_bas_battery_level_update(&m_bas, battery_level, m_conn_handle);
    if ((err_code != NRF_SUCCESS) && (err_code != NRF_ERROR_BUSY) &&
        (err_code != NRF_ERROR_RESOURCES) && (err_code != NRF_ERROR_FORBIDDEN) &&
        (err_code != NRF_ERROR_INVALID_STATE) && (err_code != BLE_ERROR_GATTS_SYS_ATTR_MISSING))
    {
        APP_ERROR_HANDLER(err_code);
    }
}

void set_device_name(const char *device_name)
{
    snprintf(defy_ble_name, sizeof(defy_ble_name), "%s - %i", device_name, current_channel + 1);
}

void set_current_channel(uint8_t channel)
{
    current_channel = channel;
}

void set_whitelist(bool whitelisting)
{
    active_whitelist_flag = whitelisting;
}
