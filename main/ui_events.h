#ifndef __UI_EVENTS_H__
#define __UI_EVENTS_H__

#include <stdint.h>


typedef enum
{
    UI_EVT_STACK_UP,
    UI_EVT_PAIRED,    // Device has paired to the receiver - contains request check value
    UI_EVT_CONNECTED, //
    UI_EVT_DISCONNECTED,
    UI_EVT_PLAY_POS_CHANGED,
    UI_EVT_TRK_TITLE,   // Received track title
    UI_EVT_TRK_ARTIST,  // Received track's artist
    UI_EVT_TRK_ALBUM,  // Received track's album
    UI_EVT_TRK_PLAYINGTIME  // Received track playing time
} ui_evt_t;

typedef union {
    /**
     * @brief UI_EVT_CONNECTED, UI_EVT_TRK_TITLE, UI_EVT_TRK_ARTIST, UI_EVT_TRK_ALBUM
     */
    struct ui_text_rsp_param
    {
        int evt_text_length; /*!< text character length */
        uint8_t *evt_text;   /*!< text of relevance */
    } text_rsp;              /*!< metadata attributes response */

    /**
     * @brief UI_EVT_PLAY_POS_CHANGED, UI_EVT_TRK_PLAYINGTIME
     */
    struct ui_int_rsp_param
    {
        uint32_t evt_value;    /*!< Numeric value of parameter */
    } int_rsp;
} esp_ui_param_t;

/* message to be sent */
typedef struct
{
    uint16_t sig;   /*!< signal to ui_task */
    ui_evt_t event; /*!< message event id */
    //ui_cb_t         cb;       /*!< context switch callback */
    esp_ui_param_t *param; /*!< parameter area needs to be last */
} ui_msg_t;

#endif