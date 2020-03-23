/* Hello World Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include "common.h"
#include <freertos/task.h>
#include <esp_event_loop.h>
#include <nvs_flash.h>
#include <lwip/err.h>
#include <lwip/sys.h>
#include "esp_spi_flash.h"
#include "app_wifi.h"
#include "app_httpsrv.h"

static const char * TAG="hello";
static uint32_t session_id = UINT32_MAX;
httpd_handle_t httpsrv = NULL;
int gAction = 0;
char *wifi_ssid = NULL, *wifi_password = NULL;
static const char *ap_ssid = "blu-esp1";
static const char *ap_pwd = "Hz123456";

static esp_err_t system_event_handler(void *ctx, system_event_t *event);

static esp_err_t system_event_handler(void *ctx, system_event_t *event)
{
    /* For accessing reason codes in case of disconnection */
    system_event_info_t *info = &event->event_info;
    static int s_retry_num = 0;
    static bool isInAp = false;

    switch (event->event_id) {
        case SYSTEM_EVENT_STA_DISCONNECTED:
            if(gAction == 1){
                // 
            }else{
                if (s_retry_num < 4) {
                isInAp = false;
                esp_wifi_connect();
                s_retry_num++;
                ESP_LOGE(TAG,"connect to the AP fail, retry to connect to the AP");
                }else if(!isInAp){
                    ESP_LOGW(TAG, "swith to ap");
                    isInAp = true;
                    start_wifi_ap(ap_ssid, ap_pwd);
                    s_retry_num = 0;
                }
            }

            ESP_LOGI(TAG, "disconnected from AP");   
            break;
        case SYSTEM_EVENT_STA_START:
            esp_wifi_connect();
            break;
        case SYSTEM_EVENT_STA_GOT_IP:
            ESP_LOGI(TAG, "got ip:%s", ip4addr_ntoa(&info->got_ip));
            if(!httpsrv) start_http_srv(&httpsrv);
            break;
        case SYSTEM_EVENT_AP_STAIPASSIGNED:
            ESP_LOGI(TAG, "got client:%s", ip4addr_ntoa(&info->ap_staipassigned));
            if(!httpsrv) start_http_srv(&httpsrv);
            break;
        default:
            break;
    }

    return ESP_OK;
}


void check_status( void *pvParameters ) {
    for (int i = 0;; i++) {
        if(gAction == 1) {
            start_wifi_sta(wifi_ssid, wifi_password);
            gAction = 0;
        }
        if(i % 10 == 0){
            printf("I lived %d seconds...\n", i*100);
        }
        vTaskDelay(10000 / portTICK_PERIOD_MS);
    }
}

void app_main()
{
    esp_err_t err = ESP_OK;
    printf("Hello world!\n");

    ESP_ERROR_CHECK(nvs_flash_init());
    tcpip_adapter_init();

    ESP_ERROR_CHECK(esp_event_loop_init(system_event_handler, NULL));

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    /* Get WiFi Station configuration */
    wifi_config_t wifi_config;
    ESP_ERROR_CHECK(esp_wifi_get_config(ESP_IF_WIFI_STA, &wifi_config));

    if (strlen((const char*) wifi_config.sta.ssid)) {
        ESP_LOGI(TAG, "Found ssid %s",     (const char*) wifi_config.sta.ssid);
        ESP_LOGI(TAG, "Found password %s", (const char*) wifi_config.sta.password);
        err = start_wifi_sta((char *)wifi_config.sta.ssid, (char *)wifi_config.sta.password);
    }else{
        err = start_wifi_ap(ap_ssid, ap_pwd);
    }
    /* Print chip information */
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    printf("This is ESP8266 chip with %d CPU cores, WiFi, ", chip_info.cores);

    printf("silicon revision %d, ", chip_info.revision);

    printf("%dMB %s flash\n", spi_flash_get_chip_size() / (1024 * 1024),
            (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");

    xTaskCreate(check_status, "check stat", 1000, NULL, configTIMER_TASK_PRIORITY, NULL);

    while(1){
        DELAYMS(10000);
    };
    //esp_event_loop_delete(system_event_handler);
}
