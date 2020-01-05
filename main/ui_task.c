
#include <esp_log.h>

#include "ui_task.h"

#include <u8g2.h>
#include "u8g2_esp32_hal.h"

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

static void ui_work_dispatched(ui_msg_t *msg)
{
    if (msg->cb)
    {
        msg->cb(msg->event, msg->param);
    }
}

void ui_task_start_up(void)
{
    s_ui_task_queue = xQueueCreate(10, sizeof(ui_msg_t));
    xTaskCreate(ui_task_handler, "UI_T", 2048, NULL, configMAX_PRIORITIES - 5, &s_ui_task_handle);
    return;
}

void ui_task_shut_down(void)
{
    if (s_ui_task_handle) {
        vTaskDelete(s_ui_task_handle);
        s_ui_task_handle = NULL;
    }
    if (s_ui_task_queue) {
        vQueueDelete(s_ui_task_queue);
        s_ui_task_queue = NULL;
    }
}

bool ui_work_dispatch(ui_cb_t p_cback, uint16_t event, void *p_params, int param_len, ui_copy_cb_t p_copy_cback)
{
    ESP_LOGD(UI_TASK_TAG, "%s event 0x%x, param len %d", __func__, event, param_len);

    ui_msg_t msg;
    memset(&msg, 0, sizeof(ui_msg_t));

    msg.sig = UI_SIG_WORK_DISPATCH;
    msg.event = event;
    msg.cb = p_cback;

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

const char *TAG = UI_TASK_TAG;

void task_test_SSD1306i2c(void *ignore)
{
    u8g2_esp32_hal_t u8g2_esp32_hal = U8G2_ESP32_HAL_DEFAULT;
    u8g2_esp32_hal.sda = CONFIG_DISPLAY_I2C_SDA_PIN;
    u8g2_esp32_hal.scl = CONFIG_DISPLAY_I2C_SCL_PIN;
    u8g2_esp32_hal_init(u8g2_esp32_hal);

    u8g2_t u8g2; // a structure which will contain all the data for one display
    u8g2_Setup_ssd1306_i2c_128x64_noname_f(
        &u8g2,
        U8G2_R0,
        //u8x8_byte_sw_i2c,
        u8g2_esp32_i2c_byte_cb,
        u8g2_esp32_gpio_and_delay_cb); // init u8g2 structure
    u8x8_SetI2CAddress(&u8g2.u8x8, 0x78);

    ESP_LOGI(TAG, "u8g2_InitDisplay");
    u8g2_InitDisplay(&u8g2); // send init sequence to the display, display is in sleep mode after this,

    ESP_LOGI(TAG, "u8g2_SetPowerSave");
    u8g2_SetPowerSave(&u8g2, 0); // wake up display
    ESP_LOGI(TAG, "u8g2_ClearBuffer");
    u8g2_ClearBuffer(&u8g2);
    ESP_LOGI(TAG, "u8g2_DrawBox");
    u8g2_DrawBox(&u8g2, 0, 26, 80, 6);
    u8g2_DrawFrame(&u8g2, 0, 26, 100, 6);

    ESP_LOGI(TAG, "u8g2_SetFont");
    u8g2_SetFont(&u8g2, u8g2_font_ncenB14_tr);
    ESP_LOGI(TAG, "u8g2_DrawStr");
    u8g2_DrawStr(&u8g2, 2, 17, "Hi myself!");
    ESP_LOGI(TAG, "u8g2_SendBuffer");
    u8g2_SendBuffer(&u8g2);
    u8g2_SetContrast(&u8g2, 28);

    ESP_LOGI(TAG, "All done!");

    //    vTaskDelete(NULL);
}
