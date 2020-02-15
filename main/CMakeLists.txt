set(COMPONENT_SRCS "app_events.c"
                   "bt_app_av.c"
                   "bt_app_core.c"
                   "u8g2_esp32_hal.c"
                   "ui_controller.c"
                   "ui_task.c"
                   "main.c")
set(COMPONENT_ADD_INCLUDEDIRS ".")

register_component()
