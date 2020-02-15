#include "app_events.h"
#include "ui_events.h"
#include "esp_bt.h"
#include "esp_gap_bt_api.h"
#include "esp_log.h"
#include "bt_app_av.h"
#include "bt_app_core.h"
#include "ui_task.h"
#include "esp_bt_device.h"

volatile ui_rcvr_state_t rcvr_state = RCVR_STATE_INITIALISING;

void bt_app_gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param)
{
    switch (event)
    {
    case ESP_BT_GAP_AUTH_CMPL_EVT:
    {
        if (param->auth_cmpl.stat == ESP_BT_STATUS_SUCCESS)
        {
            ESP_LOGI(BT_AV_TAG, "authentication success: %s", param->auth_cmpl.device_name);

            esp_ui_param_t params;
            ui_copyStrToTextParam(&params, (const uint8_t *)param->auth_cmpl.device_name);
            ui_work_dispatch(UI_EVT_PAIRED, &params, sizeof(esp_ui_param_t), NULL);
            esp_log_buffer_hex(BT_AV_TAG, param->auth_cmpl.bda, ESP_BD_ADDR_LEN);
        }
        else
        {
            ESP_LOGE(BT_AV_TAG, "authentication failed, status:%d", param->auth_cmpl.stat);
        }
        break;
    }

#if (CONFIG_BT_SSP_ENABLED == true)
    case ESP_BT_GAP_CFM_REQ_EVT:
        ESP_LOGI(BT_AV_TAG, "ESP_BT_GAP_CFM_REQ_EVT Please compare the numeric value: %d", param->cfm_req.num_val);
        esp_bt_gap_ssp_confirm_reply(param->cfm_req.bda, true);
        break;
    case ESP_BT_GAP_KEY_NOTIF_EVT:
        ESP_LOGI(BT_AV_TAG, "ESP_BT_GAP_KEY_NOTIF_EVT passkey:%d", param->key_notif.passkey);
        break;
    case ESP_BT_GAP_KEY_REQ_EVT:
        ESP_LOGI(BT_AV_TAG, "ESP_BT_GAP_KEY_REQ_EVT Please enter passkey!");
        break;
#endif

    default:
    {
        ESP_LOGI(BT_AV_TAG, "event: %d", event);
        break;
    }
    }
    return;
}

void bt_av_hdl_stack_evt(uint16_t event, void *p_param)
{
    char *dev_name = CONFIG_DEVICE_NAME;

    ESP_LOGI(BT_AV_TAG, "%s evt %d", __func__, event);
    switch (event)
    {
    case BT_APP_EVT_STACK_UP:
        /* set up device name */
        esp_bt_dev_set_device_name(dev_name);

        esp_ui_param_t params;
        ui_copyStrToTextParam(&params, (const uint8_t *)dev_name);
        ui_work_dispatch(UI_EVT_NON_DISCOVERABLE, &params, sizeof(esp_ui_param_t), NULL);

        esp_bt_gap_register_callback(bt_app_gap_cb);

        /* initialize AVRCP controller */
        esp_avrc_ct_init();
        esp_avrc_ct_register_callback(bt_app_rc_ct_cb);
        /* initialize AVRCP target */
        assert(esp_avrc_tg_init() == ESP_OK);
        esp_avrc_tg_register_callback(bt_app_rc_tg_cb);

        esp_avrc_rn_evt_cap_mask_t evt_set = {0};
        esp_avrc_rn_evt_bit_mask_operation(ESP_AVRC_BIT_MASK_OP_SET, &evt_set, ESP_AVRC_RN_VOLUME_CHANGE);
        assert(esp_avrc_tg_set_rn_evt_cap(&evt_set) == ESP_OK);

        /* initialize A2DP sink */
        esp_a2d_register_callback(&bt_app_a2d_cb);
        esp_a2d_sink_register_data_callback(bt_app_a2d_data_cb);
        esp_a2d_sink_init();

        /* set connectable mode, wait to be connected */
        esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_NON_DISCOVERABLE);
        ESP_LOGI(BT_AV_TAG, "End stack up");
        break;
    case BT_APP_EVT_DISCOVEARBLE_ON:
        ESP_LOGI(BT_AV_TAG, "Start discoverable on");
        // If currently in discoverable mode just ignore this
        if (rcvr_state != RCVR_STATE_DISCOVERABLE)
        {
            ESP_LOGI(BT_AV_TAG, "Enabling discoverable mode");
            /* set discoverable mode, wait to be paired */
            esp_bt_gap_set_scan_mode(ESP_BT_NON_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
            // Send discoverable UI event (no param sent)
            //-This should be a state change, UI driven from that
            ui_work_dispatch(UI_EVT_DISCOVERABLE, NULL, 0, NULL);
        }
        ESP_LOGI(BT_AV_TAG, "End discoverable on");
        break;
    case BT_APP_EVT_DISCOVEARBLE_OFF:
        /* set connectable mode, wait to be connected */
        esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_NON_DISCOVERABLE);
        ui_work_dispatch(UI_EVT_NON_DISCOVERABLE, NULL, 0, NULL);
        ESP_LOGI(BT_AV_TAG, "End discoverable off");
        break;
    default:
        ESP_LOGE(BT_AV_TAG, "%s unhandled evt %d", __func__, event);
        break;
    }
}