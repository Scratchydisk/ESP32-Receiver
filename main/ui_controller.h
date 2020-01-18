/*
    Contains the callbacks invoked from the 
    UI task.
*/

#ifndef __UI_CONTROLLER_H__
#define __UI_CONTROLLER_H__

#include "ui_events.h"

void ui_controller_init();

void ui_controller_dispatch(ui_msg_t *msg);

void ui_controller_scroll_text();

#endif /* __UI_CONTROLLER_H__*/