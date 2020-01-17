#include <string.h>
#include "ui_controller.h"

#include <esp_log.h>
#include <u8g2.h>
#include "esp_ota_ops.h"
#include "u8g2_esp32_hal.h"

#define UI_CONTROLLER_TAG "UI Cont"

u8g2_t u8g2; // a structure which will contain all the data for one display

#define MAX_LEN_TRACK_ATTRIBUTE 80

typedef enum
{
    RCVR_STATE_DISCONNECTED,
    RCVR_STATE_SUSPENDED,
    RCVR_STATE_RUNNING
} ui_rcvr_state_t;

ui_rcvr_state_t rcvr_state;

typedef struct ui_current_track
{
    char title[MAX_LEN_TRACK_ATTRIBUTE];
    char artist[MAX_LEN_TRACK_ATTRIBUTE];
    char album[MAX_LEN_TRACK_ATTRIBUTE];
    uint32_t playingTime;
    uint32_t currentPosition;
} ui_current_track_t;

ui_current_track_t current_track;

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

void ui_show_stackup(esp_ui_param_t *param)
{
    u8g2_ClearBuffer(&u8g2);

    u8g2_SetFont(&u8g2, u8g2_font_unifont_t_symbols);

    drawStrCentered(u8g2_GetMaxCharHeight(&u8g2), "Discoverable");
    drawStrCentered(u8g2_GetMaxCharHeight(&u8g2) * 2, "as");
    drawStrCentered(u8g2_GetMaxCharHeight(&u8g2) * 3, (const char *)param->text_rsp.evt_text);

    u8g2_SendBuffer(&u8g2);
}

void ui_show_connected(esp_ui_param_t *param)
{
    u8g2_ClearBuffer(&u8g2);

    u8g2_SetFont(&u8g2, u8g2_font_unifont_t_symbols);

    drawStrCentered(u8g2_GetMaxCharHeight(&u8g2) * 2, (char *)param->text_rsp.evt_text);
    //  drawStrCentered(u8g2_GetMaxCharHeight(&u8g2) * 2, "to");
    //  drawStrCentered(u8g2_GetMaxCharHeight(&u8g2) * 3, (char *)param->text_rsp.evt_text);

    u8g2_SendBuffer(&u8g2);
}

void ui_show_track()
{
    u8g2_ClearBuffer(&u8g2);

    u8g2_SetFont(&u8g2, u8g2_font_unifont_t_symbols);

    if (strcmp(current_track.title, "") == 0)
    {
        drawStrCentered(u8g2_GetMaxCharHeight(&u8g2), "Playing...");
    }
    else
    {
        drawStrCentered(u8g2_GetMaxCharHeight(&u8g2) * 1, current_track.artist);
        drawStrCentered(u8g2_GetMaxCharHeight(&u8g2) * 2, current_track.title);
        drawStrCentered(u8g2_GetMaxCharHeight(&u8g2) * 3, current_track.album);

        char percentStr[5] = "--";
        if (current_track.playingTime > 100)
        {
            // Times in ms, don't need this resolution, rather than mul numerator by 100, divide denominator by 100
            sprintf(percentStr, "%d%%", current_track.currentPosition / (current_track.playingTime / 100));
        }
        drawStrCentered(u8g2_GetMaxCharHeight(&u8g2) * 4, percentStr);
        // new fn to draw progress
    }
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
    uint8_t copyOffset;

    ESP_LOGI(UI_CONTROLLER_TAG, "Dispatch");
    esp_ui_param_t *param = (esp_ui_param_t *)(msg->param);

    switch (msg->event)
    {
    case UI_EVT_STACK_UP:
        ui_show_stackup(param);
        break;
    case UI_EVT_CONNECTED:
        ui_show_connected(param);
        break;
    case UI_EVT_DISCONNECTED:
        ui_show_connected(param);
        break;
    case UI_EVT_PAIRED:
        ui_showPaired(param);
        break;
    case UI_EVT_PLAY_POS_CHANGED:
        current_track.currentPosition = param->int_rsp.evt_value;
        ui_show_track();
        break;
    case UI_EVT_TRK_ALBUM:
        // Some album names have extraneous '/'s as their first char, remove this
        // copyOffset = (char)param->text_rsp.evt_text[0] == '/' ? 1 : 0;
        //strncpy(current_track.album, (char *)(param->text_rsp.evt_text + copyOffset), MAX_LEN_TRACK_ATTRIBUTE);
        strncpy(current_track.album, (char *)param->text_rsp.evt_text, MAX_LEN_TRACK_ATTRIBUTE);
        current_track.album[MAX_LEN_TRACK_ATTRIBUTE - 1] = '\0';
        ui_show_track();
        break;
    case UI_EVT_TRK_ARTIST:
        strncpy(current_track.artist, (char *)param->text_rsp.evt_text, MAX_LEN_TRACK_ATTRIBUTE);
        current_track.artist[MAX_LEN_TRACK_ATTRIBUTE - 1] = '\0';
        ui_show_track();
        break;
    case UI_EVT_TRK_PLAYINGTIME:
        current_track.playingTime = param->int_rsp.evt_value;
        ESP_LOGI(UI_CONTROLLER_TAG, "Playing time (param): %d", param->int_rsp.evt_value);
        ESP_LOGI(UI_CONTROLLER_TAG, "Playing time: %d", current_track.playingTime);
        ui_show_track();
        break;
    case UI_EVT_TRK_TITLE:
        strncpy(current_track.title, (char *)param->text_rsp.evt_text, MAX_LEN_TRACK_ATTRIBUTE);
        current_track.title[MAX_LEN_TRACK_ATTRIBUTE - 1] = '\0';
        ui_show_track();
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
