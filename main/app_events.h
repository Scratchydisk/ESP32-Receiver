/******
 * Events handled by the App controller
 ******/

#ifndef __APP_EVENTS_H__
#define __APP_EVENTS_H__

#include <stdint.h>

/* handler for bluetooth stack enabled events */
void bt_av_hdl_stack_evt(uint16_t event, void *p_param);

/* event for handler bt_av_hdl_stack_up */
enum
{
    BT_APP_EVT_STACK_UP = 0,
    BT_APP_EVT_DISCOVEARBLE_ON,
    BT_APP_EVT_DISCOVEARBLE_OFF
} app_evt_t;

#endif