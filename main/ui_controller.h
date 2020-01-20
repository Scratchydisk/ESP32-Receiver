/*
    Contains the callbacks invoked from the 
    UI task.
*/

#ifndef __UI_CONTROLLER_H__
#define __UI_CONTROLLER_H__

#include "ui_events.h"

void ui_controller_init();

// Updates the model in response to events
void ui_controller_dispatch(ui_msg_t *msg);

// Writes the model to the display
void ui_controller_refresh();

#endif /* __UI_CONTROLLER_H__*/