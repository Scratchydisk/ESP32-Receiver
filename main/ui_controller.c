#include "ui_controller.h"

#include <esp_log.h>
#include <u8g2.h>
#include "esp_ota_ops.h"
#include "u8g2_esp32_hal.h"

#define UI_CONTROLLER_TAG "UI Cont"

u8g2_t u8g2; // a structure which will contain all the data for one display

/** Drawing helpers **/

void drawStrCentered(u8g2_uint_t y, const char *str)
{
    uint16_t strWidth = u8g2_GetStrWidth(&u8g2, str);
    int16_t x = (u8g2_GetDisplayWidth(&u8g2) - strWidth) / 2;
    u8g2_DrawStr(&u8g2, x, y, str);
}

/** Controller functions **/

void ui_show_startup()
{
    u8g2_ClearBuffer(&u8g2);

    //u8g2_SetFont(&u8g2, u8g2_font_ncenB14_tr);
    u8g2_SetFont(&u8g2, u8g2_font_unifont_t_symbols);

    drawStrCentered(u8g2_GetMaxCharHeight(&u8g2), "Grotsoft");
    drawStrCentered(u8g2_GetMaxCharHeight(&u8g2) * 2, "Bluetooth");
    drawStrCentered(u8g2_GetMaxCharHeight(&u8g2) * 3, "Receiver");

    const esp_app_desc_t appDesc = *esp_ota_get_app_description();
    char appVer[40];
    sprintf(appVer, "SW %s", appDesc.version);

    drawStrCentered(u8g2_GetMaxCharHeight(&u8g2) * 4, appVer);

    u8g2_SendBuffer(&u8g2);
}

void ui_showConnected(esp_ui_param_t *param)
{
    u8g2_ClearBuffer(&u8g2);

    u8g2_SetFont(&u8g2, u8g2_font_unifont_t_symbols);

    drawStrCentered(u8g2_GetMaxCharHeight(&u8g2), "Connected");
  //  drawStrCentered(u8g2_GetMaxCharHeight(&u8g2) * 2, "to");
  //  drawStrCentered(u8g2_GetMaxCharHeight(&u8g2) * 3, (char *)param->text_rsp.evt_text);

    u8g2_SendBuffer(&u8g2);
}

void ui_showPaired(esp_ui_param_t *param)
{
    u8g2_ClearBuffer(&u8g2);

    //u8g2_SetFont(&u8g2, u8g2_font_ncenB14_tr);
    u8g2_SetFont(&u8g2, u8g2_font_unifont_t_symbols);

    drawStrCentered(u8g2_GetMaxCharHeight(&u8g2), "Paired");
    drawStrCentered(u8g2_GetMaxCharHeight(&u8g2) * 2, "with");
    drawStrCentered(u8g2_GetMaxCharHeight(&u8g2) * 3, (char *)param->text_rsp.evt_text);

    u8g2_SendBuffer(&u8g2);
}

/**
 * Decide what to do based upon the UI event passed.
 */
void ui_controller_dispatch(ui_msg_t *msg)
{
    esp_ui_param_t *param = (esp_ui_param_t *)(msg->param);

    switch (msg->event)
    {
    case UI_EVT_CONNECTED:
        ui_showConnected(param);
        break;
    case UI_EVT_PAIRED:
        break;
    case UI_EVT_TRK_ALBUM:
        break;
    case UI_EVT_TRK_ARTIST:
        break;
    case UI_EVT_TRK_PLAYINGTIME:
        break;
    case UI_EVT_TRK_TITLE:
        break;
    default:
        ui_show_startup();
        break;
    }
}

/**
 * Initialise the display
 */
void ui_controller_init()
{
    ESP_LOGI(UI_CONTROLLER_TAG, "UI Initialisation");

    // Initialise the wrapper around the U8g2 library
    u8g2_esp32_hal_t u8g2_esp32_hal = U8G2_ESP32_HAL_DEFAULT;
    u8g2_esp32_hal.sda = CONFIG_DISPLAY_I2C_SDA_PIN;
    u8g2_esp32_hal.scl = CONFIG_DISPLAY_I2C_SCL_PIN;
    u8g2_esp32_hal_init(u8g2_esp32_hal);

    // This is the 0.96" OLED display, I2C, with full frame buffer
    u8g2_Setup_ssd1306_i2c_128x64_noname_f(
        &u8g2,
        U8G2_R0,
        //u8x8_byte_sw_i2c,
        u8g2_esp32_i2c_byte_cb,
        u8g2_esp32_gpio_and_delay_cb); // init u8g2 structure

    u8x8_SetI2CAddress(&u8g2.u8x8, 0x78);
    u8g2_InitDisplay(&u8g2);     // send init sequence to the display, display is in sleep mode after this,
    u8g2_SetPowerSave(&u8g2, 0); // wake up display
    // Initialise half brightness
    u8g2_SetContrast(&u8g2, 127);

    ui_show_startup();
}

const char *TAG = UI_CONTROLLER_TAG;

void task_test_SSD1306i2c(void *ignore)
{
    /*
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
*/
    //    vTaskDelete(NULL);
}
