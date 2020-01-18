
//#include <stdint.h>
// #include <stdbool.h>
// #include <stdlib.h>
#include <string.h>
#include "freertos/xtensa_api.h"
#include "freertos/FreeRTOSConfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "ui_task.h"
#include "ui_controller.h"

// Update display in response to message
static bool ui_send_msg(ui_msg_t *msg);
static void ui_work_dispatched(ui_msg_t *msg);

static xQueueHandle s_ui_task_queue = NULL;
static xTaskHandle s_ui_task_handle = NULL;

static void ui_task_handler(void *arg)
{
    ui_msg_t msg;
    for (;;)
    {
        if (xQueueReceive(s_ui_task_queue, &msg, (portTickType)portMAX_DELAY) == pdTRUE)
        {
            ESP_LOGD(UI_TASK_TAG, "%s, sig 0x%x, 0x%x", __func__, msg.sig, msg.event);
            switch (msg.sig)
            {
            case UI_SIG_WORK_DISPATCH:
                ui_work_dispatched(&msg);
                break;
            default:
                ESP_LOGW(UI_TASK_TAG, "%s, unhandled sig: %d", __func__, msg.sig);
                break;
            } // switch (msg.sig)

            if (msg.param)
            {
                free(msg.param);
            }
        }
    }
}

// Draw the screen every 50ms
static void ui_draw_handler(void *arg)
{
    for (;;)
    {
        ui_controller_scroll_text();
        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
}

static void ui_work_dispatched(ui_msg_t *msg)
{
    ui_controller_dispatch(msg);
}

void ui_task_start_up(void)
{
    s_ui_task_queue = xQueueCreate(10, sizeof(ui_msg_t));
    xTaskCreate(ui_task_handler, "UI_Model_Update", 2048, NULL, configMAX_PRIORITIES - 5, &s_ui_task_handle);
    xTaskCreate(ui_draw_handler, "UI_Draw_Screen", 2048, NULL, configMAX_PRIORITIES - 5, NULL);
    ui_controller_init();
}

void ui_task_shut_down(void)
{
    if (s_ui_task_handle)
    {
        vTaskDelete(s_ui_task_handle);
        s_ui_task_handle = NULL;
    }
    if (s_ui_task_queue)
    {
        vQueueDelete(s_ui_task_queue);
        s_ui_task_queue = NULL;
    }
}

bool ui_work_dispatch(ui_evt_t event, void *p_params, int param_len, ui_copy_cb_t p_copy_cback)
{
    ESP_LOGD(UI_TASK_TAG, "%s event 0x%x, param len %d", __func__, event, param_len);

    ui_msg_t msg;
    memset(&msg, 0, sizeof(ui_msg_t));

    msg.sig = UI_SIG_WORK_DISPATCH;
    msg.event = event;
    //msg.cb = p_cback;

    if (param_len == 0)
    {
        return ui_send_msg(&msg);
    }
    else if (p_params && param_len > 0)
    {
        if ((msg.param = malloc(param_len)) != NULL)
        {
            memcpy(msg.param, p_params, param_len);
            /* check if caller has provided a copy callback to do the deep copy */
            if (p_copy_cback)
            {
                p_copy_cback(&msg, msg.param, p_params);
            }
            return ui_send_msg(&msg);
        }
    }

    return false;
}

static bool ui_send_msg(ui_msg_t *msg)
{
    if (msg == NULL)
    {
        return false;
    }

    if (xQueueSend(s_ui_task_queue, msg, 10 / portTICK_RATE_MS) != pdTRUE)
    {
        ESP_LOGE(UI_TASK_TAG, "%s xQueue send failed", __func__);
        return false;
    }
    return true;
}

void ui_copyStrToTextParam(esp_ui_param_t *params, const uint8_t *str)
{
    ESP_LOGI(UI_TASK_TAG, "StrParam: %s", (char *)str);

    params->text_rsp.evt_text_length = strlen((char *)str);
    params->text_rsp.evt_text = (uint8_t *)malloc(params->text_rsp.evt_text_length + 1);
    memcpy(params->text_rsp.evt_text, str, params->text_rsp.evt_text_length);
    params->text_rsp.evt_text[params->text_rsp.evt_text_length] = 0;

    ESP_LOGI(UI_TASK_TAG, "UI Param: len %d, txt: %s", params->text_rsp.evt_text_length,
             params->text_rsp.evt_text);
}
