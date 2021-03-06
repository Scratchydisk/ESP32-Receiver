// Modified 2020 by Grotsoft
//
// Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_intr_alloc.h"

#include "esp_bt.h"
#include "bt_app_core.h"
#include "bt_app_av.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_gap_bt_api.h"
#include "esp_a2dp_api.h"
#include "esp_avrc_api.h"
#include "driver/i2s.h"
#include "driver/gpio.h"
#include "app_events.h"
#include "ui_task.h"
#include "ui_controller.h"

void setupI2S();
void setupBluetooth();

// For debouncing the discovery mode button
volatile TickType_t _lastDiscoveryClickAt;

// Interrupt handler for GPIO I/O
static void IRAM_ATTR gpio_isr_handler(void *arg)
{
    uint32_t gpio_num = (uint32_t)arg;
    // 100ms debounce
    if (gpio_num == CONFIG_DISCOVERY_MODE_ENABLE_GPIO && (xTaskGetTickCount() - _lastDiscoveryClickAt) > 100 / portTICK_PERIOD_MS)
    {
        _lastDiscoveryClickAt = xTaskGetTickCount();
        bt_app_work_interrupt_dispatch(bt_av_hdl_stack_evt, BT_APP_EVT_DISCOVEARBLE_ON, NULL, 0, NULL);
    }
}

void app_main()
{
    // Setup Discovery enable button as input / pull up
    // and enable rising edge interrupt
    // (Triggers when user releases button)
    gpio_config_t io_conf;
    //interrupt on rising edge
    io_conf.intr_type = GPIO_PIN_INTR_POSEDGE;
    //bit mask of the discovery button's GPIO pin
    io_conf.pin_bit_mask = 1ULL << CONFIG_DISCOVERY_MODE_ENABLE_GPIO;
    //set as input / pull up mode
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = true;
    io_conf.pull_down_en = false;
    gpio_config(&io_conf);

    /* Create UI task */
    ui_task_start_up();

    /* Initialize NVS — it is used to store PHY calibration data */
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    setupI2S();

    setupBluetooth();

    /* create application task */
    bt_app_task_start_up();

    /* Bluetooth device name, connection mode and profile set up */
    bt_app_work_dispatch(bt_av_hdl_stack_evt, BT_APP_EVT_STACK_UP, NULL, 0, NULL);

    // Hook ISR to discovery pin, Use defaults (0)
    gpio_install_isr_service(0);
    gpio_isr_handler_add(CONFIG_DISCOVERY_MODE_ENABLE_GPIO,
                         gpio_isr_handler,
                         (void *)CONFIG_DISCOVERY_MODE_ENABLE_GPIO);

    ESP_LOGI(BT_AV_TAG, "Finished app_main()");
}

// Initialises I2S based upon config settings.
// One of: Internal DAC, external I2S or external PCM
void setupI2S()
{
    i2s_config_t i2s_config = {
#ifdef CONFIG_A2DP_SINK_OUTPUT_INTERNAL_DAC
        .mode = I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_DAC_BUILT_IN,
#else
        .mode = I2S_MODE_MASTER | I2S_MODE_TX, // Only TX
#endif
        .sample_rate = 44100,
        .bits_per_sample = 16,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT, //2-channels
#ifdef CONFIG_A2DP_SINK_OUTPUT_EXTERNAL_PCM
        .communication_format = I2S_COMM_FORMAT_PCM_MSB,
#else
        .communication_format = I2S_COMM_FORMAT_I2S_MSB,
#endif
        .dma_buf_count = 6,
        .dma_buf_len = 60,
        .intr_alloc_flags = 0,     //Default interrupt priority
        .tx_desc_auto_clear = true //Auto clear tx descriptor on underflow
    };

    i2s_driver_install(0, &i2s_config, 0, NULL);
#ifdef CONFIG_A2DP_SINK_OUTPUT_INTERNAL_DAC
    i2s_set_dac_mode(I2S_DAC_CHANNEL_BOTH_EN);
    i2s_set_pin(0, NULL);
#else
    i2s_pin_config_t pin_config = {
        .bck_io_num = CONFIG_I2S_BCK_GPIO,
        .ws_io_num = CONFIG_I2S_LRCK_GPIO,
        .data_out_num = CONFIG_I2S_DATA_GPIO,
        .data_in_num = -1 //Not used
    };

    // TODO: Options for PCM

    i2s_set_pin(0, &pin_config);
#endif
}

void setupBluetooth()
{
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_BLE));

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    esp_err_t err = esp_bt_controller_init(&bt_cfg);
    if (err != ESP_OK)
    {
        ESP_LOGE(BT_AV_TAG, "%s initialize controller failed: %s\n", __func__, esp_err_to_name(err));
        return;
    }

    if ((err = esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT)) != ESP_OK)
    {
        ESP_LOGE(BT_AV_TAG, "%s enable controller failed: %s\n", __func__, esp_err_to_name(err));
        return;
    }

    if ((err = esp_bluedroid_init()) != ESP_OK)
    {
        ESP_LOGE(BT_AV_TAG, "%s initialize bluedroid failed: %s\n", __func__, esp_err_to_name(err));
        return;
    }

    if ((err = esp_bluedroid_enable()) != ESP_OK)
    {
        ESP_LOGE(BT_AV_TAG, "%s enable bluedroid failed: %s\n", __func__, esp_err_to_name(err));
        return;
    }

#if (CONFIG_BT_SSP_ENABLED == true)
    /* Set default parameters for Secure Simple Pairing */
    esp_bt_sp_param_t param_type = ESP_BT_SP_IOCAP_MODE;
    esp_bt_io_cap_t iocap = ESP_BT_IO_CAP_IO;
    esp_bt_gap_set_security_param(param_type, &iocap, sizeof(uint8_t));
#endif

    /*
     * Set default parameters for Legacy Pairing
     * Use fixed pin code
     */
    esp_bt_pin_type_t pin_type = ESP_BT_PIN_TYPE_FIXED;
    esp_bt_pin_code_t pin_code;
    pin_code[0] = '1';
    pin_code[1] = '2';
    pin_code[2] = '3';
    pin_code[3] = '4';
    esp_bt_gap_set_pin(pin_type, 4, pin_code);
}
