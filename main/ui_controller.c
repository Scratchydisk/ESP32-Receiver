#include <string.h>
#include "ui_controller.h"

#include <esp_log.h>
#include <u8g2.h>
#include "esp_ota_ops.h"
#include "u8g2_esp32_hal.h"

#define UI_CONTROLLER_TAG "UI Cont"

u8g2_t u8g2; // a structure which will contain all the data for one display

// mutex around the state structure
static portMUX_TYPE state_mutex = portMUX_INITIALIZER_UNLOCKED;

/*********
 * Model for screen data
 * *******/

#define MAX_LEN_TRACK_ATTRIBUTE 80

typedef enum
{
    RCVR_STATE_INITIALISING,
    RCVR_STATE_DISCOVERABLE,
    RCVR_STATE_PAIRED,
    RCVR_STATE_CONNECTED,
    RCVR_STATE_DISCONNECTED,
    RCVR_STATE_STOPPED,
    RCVR_STATE_PLAYING
} ui_rcvr_state_t;

volatile ui_rcvr_state_t rcvr_state = RCVR_STATE_INITIALISING;

typedef struct ui_current_state
{
    char connectedTo[MAX_LEN_TRACK_ATTRIBUTE];
    char hostName[MAX_LEN_TRACK_ATTRIBUTE];
    char pairedWith[MAX_LEN_TRACK_ATTRIBUTE];
    char title[MAX_LEN_TRACK_ATTRIBUTE];
    char artist[MAX_LEN_TRACK_ATTRIBUTE];
    char album[MAX_LEN_TRACK_ATTRIBUTE];
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
    uint8_t *font;     // font used to draw text
    const char *str;   // text to draw
    u8g2_uint_t y;     // bottom of the text line in pixels
    uint16_t strWidth; // Calculated width of string in pixels
    uint16_t x;        // current x pos of start of text line
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
    if (strlen(scrollLine.str) > 0)
    {
        ui_controller_scroll_line(scrollLine);
    }
}

//
void drawScrollingText(u8g2_uint_t y, const char *str)
{
    scrollLine.font = (uint8_t *)u8g2.font;
    scrollLine.str = str;
    scrollLine.y = y;
    scrollLine.strWidth = u8g2_GetStrWidth(&u8g2, str);
    scrollLine.x = 0;
}

void stopScrollingText()
{
    scrollLine.str = NULL;
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

    drawStrCentered(u8g2_GetMaxCharHeight(&u8g2), "Discoverable");
    drawStrCentered(u8g2_GetMaxCharHeight(&u8g2) * 2, "as");
    //drawScrollingText(u8g2_GetMaxCharHeight(&u8g2) * 3, current_state.hostName);
    drawStrCentered(u8g2_GetMaxCharHeight(&u8g2) * 3, current_state.hostName);
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

    drawStrCentered(u8g2_GetMaxCharHeight(&u8g2), "Disconnected");

    // Actually this should show the time
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
        if (current_state.trackDuration > 100)
        {
            // Times in ms, don't need this resolution, rather than mul numerator by 100, divide denominator by 100
            uint8_t progress = current_state.trackPosition / (current_state.trackDuration / 100);
            ui_show_progress_bar(progress);
        }
    }
}

void ui_show_paired()
{
    //u8g2_SetFont(&u8g2, u8g2_font_ncenB14_tr);
    u8g2_SetFont(&u8g2, u8g2_font_unifont_t_symbols);

    drawStrCentered(u8g2_GetMaxCharHeight(&u8g2), "Paired");
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

/************************************
 * Decide what to do based upon the UI event passed.
 */
void ui_controller_dispatch(ui_msg_t *msg)
{
    uint8_t copyOffset;

    ESP_LOGI(UI_CONTROLLER_TAG, "Dispatch");
    esp_ui_param_t *param = (esp_ui_param_t *)(msg->param);

    // Lock the state model
    portENTER_CRITICAL(&state_mutex);

    switch (msg->event)
    {
    case UI_EVT_STACK_UP:
        rcvr_state = RCVR_STATE_DISCOVERABLE;
        strncpy(current_state.hostName, (char *)param->text_rsp.evt_text, MAX_LEN_TRACK_ATTRIBUTE);
        current_state.hostName[MAX_LEN_TRACK_ATTRIBUTE - 1] = '\0';
        break;
    case UI_EVT_CONNECTED:
        rcvr_state = RCVR_STATE_CONNECTED;
        strncpy(current_state.connectedTo, (char *)param->text_rsp.evt_text, MAX_LEN_TRACK_ATTRIBUTE);
        current_state.connectedTo[MAX_LEN_TRACK_ATTRIBUTE - 1] = '\0';
        break;
    case UI_EVT_DISCONNECTED:
        rcvr_state = RCVR_STATE_DISCONNECTED;
        current_state.connectedTo[0] = '\0';
        break;
    case UI_EVT_PAIRED:
        rcvr_state = RCVR_STATE_PAIRED;
        strncpy(current_state.pairedWith, (char *)param->text_rsp.evt_text, MAX_LEN_TRACK_ATTRIBUTE);
        current_state.pairedWith[MAX_LEN_TRACK_ATTRIBUTE - 1] = '\0';
        break;
    case UI_EVENT_TRACK_STARTED:
        rcvr_state = RCVR_STATE_PLAYING;
        break;
    case UI_EVENT_TRACK_STOPPED:
        rcvr_state = RCVR_STATE_STOPPED;
        break;
    case UI_EVT_PLAY_POS_CHANGED:
        current_state.trackPosition = param->int_rsp.evt_value;
        break;
    case UI_EVT_TRK_ALBUM:
        // Some album names have extraneous '/'s as their first char, remove this
        strncpy(current_state.album, (char *)param->text_rsp.evt_text, MAX_LEN_TRACK_ATTRIBUTE);
        current_state.album[MAX_LEN_TRACK_ATTRIBUTE - 1] = '\0';
        break;
    case UI_EVT_TRK_ARTIST:
        strncpy(current_state.artist, (char *)param->text_rsp.evt_text, MAX_LEN_TRACK_ATTRIBUTE);
        current_state.artist[MAX_LEN_TRACK_ATTRIBUTE - 1] = '\0';
        break;
    case UI_EVT_TRK_PLAYINGTIME:
        current_state.trackDuration = param->int_rsp.evt_value;
        break;
    case UI_EVT_TRK_TITLE:
        strncpy(current_state.title, (char *)param->text_rsp.evt_text, MAX_LEN_TRACK_ATTRIBUTE);
        current_state.title[MAX_LEN_TRACK_ATTRIBUTE - 1] = '\0';
        break;
    default:
        ui_show_about();
        break;
    }

    portEXIT_CRITICAL(&state_mutex);
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
        u8g2_esp32_i2c_byte_cb,
        u8g2_esp32_gpio_and_delay_cb); // init u8g2 structure

    u8x8_SetI2CAddress(&u8g2.u8x8, CONFIG_DISPLAY_I2C_ADDRESS);
    u8g2_InitDisplay(&u8g2);     // send init sequence to the display, display is in sleep mode after this,
    u8g2_SetPowerSave(&u8g2, 0); // wake up display
    // Initialise half brightness
    u8g2_SetContrast(&u8g2, 127);
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
