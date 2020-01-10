/*
    Display related task for ESP32 Receiver.
*/

#ifndef __UI_TASH_H__
#define __UI_TASK_H__

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#include "ui_events.h"

#define UI_TASK_TAG                   "UI"

#define UI_SIG_WORK_DISPATCH          (0x02)

/**
 * @brief     handler for the dispatched work
 */
typedef void (* ui_cb_t) (uint16_t event, void *param);



/**
 * @brief     parameter deep-copy function to be customized
 */
typedef void (* ui_copy_cb_t) (ui_msg_t *msg, void *p_dest, void *p_src);

// TODO: Remove callback from ui dispatch (check if needed first...), UI controller will handle this based on event id

/**
 * @brief     work dispatcher for the UI task
 */
bool ui_work_dispatch(ui_evt_t event, void *p_params, int param_len, ui_copy_cb_t p_copy_cback);

void ui_task_start_up(void);

void ui_task_shut_down(void);

void ui_copyStrToTextParam(esp_ui_param_t *params, const uint8_t *str);

#endif /* __UI_TASK_H__ */
