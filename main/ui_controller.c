#include <string.h>
#include "ui_controller.h"
#include "app_events.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"

#include <esp_log.h>
#include <u8g2.h>
#include "esp_ota_ops.h"
#include "u8g2_esp32_hal.h"
#include "bt_app_core.h"

#define UI_CONTROLLER_TAG "UI Cont"

u8g2_t u8g2; // a structure which will contain all the data for one display

// mutex around the state structure
static portMUX_TYPE state_mutex = portMUX_INITIALIZER_UNLOCKED;

/*********
 * Model for screen data
 * *******/

#define MAX_STR_ATTRIBUTE_LENGTH 80

typedef struct ui_current_state
{
    char connectedTo[MAX_STR_ATTRIBUTE_LENGTH];
    char hostName[MAX_STR_ATTRIBUTE_LENGTH];
    char pairedWith[MAX_STR_ATTRIBUTE_LENGTH];
    char title[MAX_STR_ATTRIBUTE_LENGTH];
    char artist[MAX_STR_ATTRIBUTE_LENGTH];
    char album[MAX_STR_ATTRIBUTE_LENGTH];
    char pairingPINnum[7];
    TickType_t changedAt; // When last state change took place
    uint32_t trackDuration;
    uint32_t trackPosition;
} ui_current_state_t;

ui_current_state_t current_state;

void ui_controller_refresh();

/*********************
 * Screen drawing functions
 * *******************/

/** Scrolling Text **/

// Represents a single line of text that scrolls
// if it's longer than the display.
typedef struct ui_scrolling_text
{
    uint8_t *font;                      // font used to draw text
    char str[MAX_STR_ATTRIBUTE_LENGTH]; // text to draw
    u8g2_uint_t y;                      // bottom of the text line in pixels
    uint16_t strWidth;                  // Calculated width of string in pixels
    uint16_t x;                         // current x pos of start of text line
} ui_scrolling_text_t;

// Just one line for testing

ui_scrolling_text_t scrollLine;

// TODO: will probably need to add delay at start and end of scroll

void ui_controller_scroll_line(ui_scrolling_text_t line)
{
    //const uint8_t *origFont = u8g2.font;
    //    u8g2_SetFont(&u8g2, line.font);

    // Move left or reset x to 0?
    if (line.strWidth - line.x == u8g2_GetDisplayWidth(&u8g2))
    {
        line.x = 0;
    }
    else
    {
        line.x++;
    }

    u8g2_DrawStr(&u8g2, -line.x, line.y, line.str);
    //   u8g2_SetFont(&u8g2, origFont);
}

// Scrolls any scrolling text (just one line for testing)
void ui_controller_scroll_text()
{
    if (strnlen(scrollLine.str, MAX_STR_ATTRIBUTE_LENGTH) > 0)
    {
        ui_controller_scroll_line(scrollLine);
    }
}

//
void drawScrollingText(u8g2_uint_t y, const char *str)
{
    scrollLine.font = (uint8_t *)u8g2.font;
    strlcpy(str, scrollLine.str, MAX_STR_ATTRIBUTE_LENGTH);
    scrollLine.y = y;
    scrollLine.strWidth = u8g2_GetStrWidth(&u8g2, str);
    scrollLine.x = 0;
}

void stopScrollingText()
{
    scrollLine.str[0] = NULL;
}

/** Drawing helpers **/

void drawStrCentered(u8g2_uint_t y, const char *str)
{
    uint16_t strWidth = u8g2_GetStrWidth(&u8g2, str);
    int16_t x = (u8g2_GetDisplayWidth(&u8g2) - strWidth) / 2;
    u8g2_DrawStr(&u8g2, x, y, str);
}

// Draws a narrow progress bar at bottom of display, 4 pixels high
void ui_show_progress_bar(uint8_t percent)
{
    u8g2_uint_t dw = u8g2_GetDisplayWidth(&u8g2);
    u8g2_uint_t barWidth = (dw * percent) / 100;
    u8g2_DrawBox(&u8g2, 0, u8g2_GetDisplayHeight(&u8g2) - 4, barWidth, 4);
}

/** Controller functions **/

void ui_show_about()
{
    //u8g2_SetFont(&u8g2, u8g2_font_ncenB14_tr);
    u8g2_SetFont(&u8g2, u8g2_font_unifont_t_symbols);

    drawStrCentered(u8g2_GetMaxCharHeight(&u8g2), "Grotsoft");
    drawStrCentered(u8g2_GetMaxCharHeight(&u8g2) * 2, "Bluetooth");
    drawStrCentered(u8g2_GetMaxCharHeight(&u8g2) * 3, "Receiver");

    const esp_app_desc_t appDesc = *esp_ota_get_app_description();
    char appVer[40];
    sprintf(appVer, "SW %s", appDesc.version);

    drawStrCentered(u8g2_GetMaxCharHeight(&u8g2) * 4, appVer);
}

void ui_show_discoverable()
{
    u8g2_SetFont(&u8g2, u8g2_font_unifont_t_symbols);

    drawStrCentered(u8g2_GetMaxCharHeight(&u8g2), "Discoverable as");
    //drawStrCentered(u8g2_GetMaxCharHeight(&u8g2) * 2, "as");
    //drawScrollingText(u8g2_GetMaxCharHeight(&u8g2) * 3, current_state.hostName);
    drawStrCentered(u8g2_GetMaxCharHeight(&u8g2) * 2, current_state.hostName);

    if (strnlen(current_state.pairingPINnum, 7) > (size_t)0)
    {
        drawStrCentered(u8g2_GetDisplayHeight(&u8g2) * 3, current_state.pairingPINnum);
    }

    // Show countdown to timeout of discovery mode
    TickType_t ellapsedTicks = (xTaskGetTickCount() - current_state.changedAt);
    TickType_t totalTicks = pdMS_TO_TICKS(CONFIG_DISCOVERY_MODE_DURATION * 1000);
    uint8_t percentLeft = (uint8_t)((totalTicks - ellapsedTicks) / (totalTicks / 100));
    ui_show_progress_bar(percentLeft);
}

void ui_show_connected()
{
    u8g2_SetFont(&u8g2, u8g2_font_unifont_t_symbols);

    drawStrCentered(u8g2_GetMaxCharHeight(&u8g2), "Connected to");
    drawStrCentered(u8g2_GetMaxCharHeight(&u8g2) * 2, current_state.connectedTo);
}

void ui_show_disconnected()
{
    u8g2_SetFont(&u8g2, u8g2_font_unifont_t_symbols);

    //drawStrCentered(u8g2_GetMaxCharHeight(&u8g2), "Disconnected");

    // Actually this should show the time
    drawStrCentered(u8g2_GetMaxCharHeight(&u8g2) * 1, "27/08/1936");

    // 16 pixel high font
    //u8g2_SetFont(&u8g2, u8g2_font_logisoso16_tn);  // Good
    //u8g2_SetFont(&u8g2, u8g2_font_inr27_mn);  // Pretty decent
    u8g2_SetFont(&u8g2, u8g2_font_osr29_tn); // Better than 26
    drawStrCentered(u8g2_GetDisplayHeight(&u8g2), "24:36");
}

void ui_show_track()
{
    u8g2_SetFont(&u8g2, u8g2_font_unifont_t_symbols);

    if (strcmp(current_state.title, "") == 0)
    {
        drawStrCentered(u8g2_GetMaxCharHeight(&u8g2), "Playing...");
    }
    else
    {
        drawStrCentered(u8g2_GetMaxCharHeight(&u8g2) * 1, current_state.artist);
        drawStrCentered(u8g2_GetMaxCharHeight(&u8g2) * 2, current_state.title);
        drawStrCentered(u8g2_GetMaxCharHeight(&u8g2) * 3, current_state.album);

        // Avoid divide by zero (no guarantee duration is set)
        if (current_state.trackDuration > (uint32_t)100)
        {
            // Times in ms, don't need this resolution, rather than mul numerator by 100, divide denominator by 100
            uint8_t progress = current_state.trackPosition / (current_state.trackDuration / (uint32_t)100);
            ui_show_progress_bar(progress);
        }
    }
}

void ui_show_pairing_failed()
{
    u8g2_SetFont(&u8g2, u8g2_font_unifont_t_symbols);
    drawStrCentered(u8g2_GetMaxCharHeight(&u8g2) * 2, "Pairing");
    drawStrCentered(u8g2_GetMaxCharHeight(&u8g2) * 3, "Failed");
}

// If there's a pairing pin then show it
void ui_show_pairing()
{
    u8g2_SetFont(&u8g2, u8g2_font_unifont_t_symbols);
    drawStrCentered(u8g2_GetMaxCharHeight(&u8g2) * 1, "Pairing");
    drawStrCentered(u8g2_GetMaxCharHeight(&u8g2) * 2, "PIN");

    if (strnlen(current_state.pairingPINnum, 7) > (size_t)0)
    {
        u8g2_SetFont(&u8g2, u8g2_font_osr21_tn);
        drawStrCentered(u8g2_GetDisplayHeight(&u8g2), current_state.pairingPINnum);
    }
}

void ui_show_paired()
{
    //u8g2_SetFont(&u8g2, u8g2_font_ncenB14_tr);
    u8g2_SetFont(&u8g2, u8g2_font_unifont_t_symbols);

    drawStrCentered(u8g2_GetMaxCharHeight(&u8g2) * 1, "Paired");
    drawStrCentered(u8g2_GetMaxCharHeight(&u8g2) * 2, "with");
    drawStrCentered(u8g2_GetMaxCharHeight(&u8g2) * 3, current_state.pairedWith);
}

// Draws screen, driven by current state
void ui_controller_refresh()
{
    u8g2_ClearBuffer(&u8g2);

    // Lock the state model
    portENTER_CRITICAL(&state_mutex);

    switch (rcvr_state)
    {
    case RCVR_STATE_CONNECTED:
        ui_show_connected();
        break;
    case RCVR_STATE_DISCONNECTED:
        ui_show_disconnected();
        break;
    case RCVR_STATE_PAIRED:
        ui_show_paired();
        break;
    case RCVR_STATE_PAIRED_FAIL:
        ui_show_pairing_failed();
        break;
    case RCVR_STATE_PARING:
        ui_show_pairing();
        break;
    case RCVR_STATE_PLAYING:
        ui_show_track();
        break;
    case RCVR_STATE_DISCOVERABLE:
        ui_show_discoverable();
        break;
    case RCVR_STATE_INITIALISING:
        ui_show_about();
        break;
    case RCVR_STATE_STOPPED:
        ui_show_connected();
        break;
    }

    // ui_controller_scroll_text();

    portEXIT_CRITICAL(&state_mutex);

    u8g2_SendBuffer(&u8g2);
}

// Turns off discovery mode when called from timer
void discovery_off_timer_cb(xTimerHandle timer)
{
    bt_app_work_dispatch(bt_av_hdl_stack_evt, BT_APP_EVT_DISCOVEARBLE_OFF, NULL, 0, NULL);
    xTimerDelete(timer, 0);
}

/************************************
 * Decide what to do based upon the UI event passed.
 */
void ui_controller_dispatch(ui_msg_t *msg)
{
    TimerHandle_t timer;
    uint8_t copyOffset;

    ESP_LOGI(UI_CONTROLLER_TAG, "Dispatch");

    esp_ui_param_t *param = (esp_ui_param_t *)(msg->param);

    // if (param->text_rsp != NULL)
    // {
    //     ESP_LOGI(UI_CONTROLLER_TAG, "Param Txt Len: %d", param->text_rsp.evt_text_length);
    //     if (param->text_rsp.evt_text_length > 0)
    //     {
    //         ESP_LOGI(UI_CONTROLLER_TAG, "Param Txt: %s", param->text_rsp.evt_text);
    //     }
    // }

    // Lock the state model
    portENTER_CRITICAL(&state_mutex);

    current_state.changedAt = xTaskGetTickCount();

    switch (msg->event)
    {
    case UI_EVT_NON_DISCOVERABLE:
        rcvr_state = RCVR_STATE_DISCONNECTED;
        if (param != NULL)
        {
            strlcpy(current_state.hostName, (char *)param->text_rsp.evt_text, MAX_STR_ATTRIBUTE_LENGTH);
        }
        break;
    case UI_EVT_DISCOVERABLE:
        rcvr_state = RCVR_STATE_DISCOVERABLE;
        strlcpy(current_state.pairingPINnum, (char *)param->text_rsp.evt_text, MAX_STR_ATTRIBUTE_LENGTH);
        // Start timer to stop discoverable after the period
        // defined in the config file.
        timer = xTimerCreate(
            "timer",
            // Config duration is in seconds
            pdMS_TO_TICKS(CONFIG_DISCOVERY_MODE_DURATION * 1000),
            pdFALSE,                 // single shot
            (void *)1,               // timer ID
            discovery_off_timer_cb); // callback
        xTimerStart(timer, 0);       // block time = 0
        break;
    case UI_EVT_CONNECTED:
        rcvr_state = RCVR_STATE_CONNECTED;
        strlcpy(current_state.connectedTo, (char *)param->text_rsp.evt_text, MAX_STR_ATTRIBUTE_LENGTH);
        break;
    case UI_EVT_DISCONNECTED:
        rcvr_state = RCVR_STATE_DISCONNECTED;
        current_state.connectedTo[0] = NULL;
        break;
    case UI_EVT_PAIRED_FAIL:
        rcvr_state = RCVR_STATE_PAIRED_FAIL;
        current_state.pairingPINnum[0] = NULL;
        break;
    case UI_EVT_PAIRED_OK:
        rcvr_state = RCVR_STATE_PAIRED;
        // if (param->text_rsp.evt_text_length > 0)
        // {
        //     strlcpy(current_state.pairedWith, (char *)param->text_rsp.evt_text, MAX_STR_ATTRIBUTE_LENGTH);
        // }
        // else
        // {
        //     strlcpy(current_state.pairedWith, "No Name", MAX_STR_ATTRIBUTE_LENGTH);
        // }
        current_state.pairingPINnum[0] = NULL;
        break;
    case UI_EVT_PAIRING_AUTH:
        rcvr_state = RCVR_STATE_PARING;
        // Check value is 6 digits
        strlcpy(current_state.pairingPINnum, (char *)param->text_rsp.evt_text, 7);
        break;
    case UI_EVT_TRK_STARTED:
        rcvr_state = RCVR_STATE_PLAYING;
        break;
    case UI_EVT_TRK_STOPPED:
        rcvr_state = RCVR_STATE_STOPPED;
        break;
    case UI_EVT_TRK_POS_CHANGED:
        current_state.trackPosition = param->int_rsp.evt_value;
        break;
    case UI_EVT_TRK_ALBUM:
        // Some album names have extraneous '/'s as their first char, remove this
        strlcpy(current_state.album, (char *)param->text_rsp.evt_text, MAX_STR_ATTRIBUTE_LENGTH);
        break;
    case UI_EVT_TRK_ARTIST:
        strlcpy(current_state.artist, (char *)param->text_rsp.evt_text, MAX_STR_ATTRIBUTE_LENGTH);
        break;
    case UI_EVT_TRK_PLAYINGTIME:
        current_state.trackDuration = param->int_rsp.evt_value;
        break;
    case UI_EVT_TRK_TITLE:
        strlcpy(current_state.title, (char *)param->text_rsp.evt_text, MAX_STR_ATTRIBUTE_LENGTH);
        break;
    default:
        ui_show_about();
        break;
    }

    portEXIT_CRITICAL(&state_mutex);

    // if(msg->event == UI_EVT_DISCOVERABLE)
    // {
    //     vTaskStartScheduler();
    // }
}

/**
 * Initialise the display
 */
void ui_controller_init()
{
    ESP_LOGI(UI_CONTROLLER_TAG, "UI Initialisation");

    // Initialise the wrapper around the U8g2 library
    u8g2_esp32_hal_t u8g2_esp32_hal = U8G2_ESP32_HAL_DEFAULT;
    u8g2_esp32_hal.sda = CONFIG_DISPLAY_I2C_SDA_GPIO;
    u8g2_esp32_hal.scl = CONFIG_DISPLAY_I2C_SCL_GPIO;
    u8g2_esp32_hal_init(u8g2_esp32_hal);

    // This is the 0.96" OLED display, I2C, with full frame buffer
    u8g2_Setup_ssd1306_i2c_128x64_noname_f(
        &u8g2,
        U8G2_R0,
        u8g2_esp32_i2c_byte_cb,
        u8g2_esp32_gpio_and_delay_cb); // init u8g2 structure

    u8x8_SetI2CAddress(&u8g2.u8x8, CONFIG_DISPLAY_I2C_ADDRESS);
    u8g2_InitDisplay(&u8g2);     // send init sequence to the display, display is in sleep mode after this,
    u8g2_SetPowerSave(&u8g2, 0); // wake up display
    // Initialise lowish brightness
    u8g2_SetContrast(&u8g2, 64);
}