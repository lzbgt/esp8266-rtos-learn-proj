/* Hello World Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include "app_common.h"
#include <freertos/task.h>
#include <esp_event_loop.h>
#include <nvs_flash.h>
#include <lwip/err.h>
#include <lwip/sys.h>
#include "esp_spi_flash.h"
#include "app_wifi.h"
#include "app_httpsrv.h"

static const char * TAG="hello";

// wifi manager globals
EventGroupHandle_t xAppEventGroup;
const EventBits_t xBitsToWaitFor = APP_EBIT_WIFI_START_AP| APP_EBIT_WIFI_START_STA;
char *ap_ssid = "blu-esp1", *ap_passwd = "123456";
char *wifi_ssid = NULL, *wifi_passwd = NULL;
WIFIManagerConfig xWifiMgrCfg = {
    .ap_ssid = &ap_ssid,
    .ap_passwd = &ap_passwd,
    .sta_ssid = &wifi_ssid,
    .sta_passwd = &wifi_passwd,
    .pxEvtGroup = &xAppEventGroup,
    .pxBitsToWaitFor = &xBitsToWaitFor,
    .xApChange = 0,
};

// http server globals
httpd_handle_t httpsrv = NULL;

static esp_err_t system_event_handler(void *ctx, system_event_t *event)
{
    system_event_info_t *info = &event->event_info;
    static int s_retry_num = 0;
    switch (event->event_id) {
        case SYSTEM_EVENT_STA_DISCONNECTED:
            ESP_LOGE(TAG, "disconnected from AP");   
            if (s_retry_num < APP_WIFI_RETRY_MAX) {
                esp_wifi_connect();
                s_retry_num++;
                ESP_LOGI(TAG,"retry to connect to the AP");
            }else{
                ESP_LOGI(TAG, "swith to ap");
                xEventGroupSetBits(xAppEventGroup, APP_EBIT_WIFI_START_AP);
                s_retry_num = 0;
            }
            break;
        case SYSTEM_EVENT_STA_START:
            esp_wifi_connect();
            break;
        case SYSTEM_EVENT_STA_GOT_IP:
            ESP_LOGI(TAG, "got ip:%s", ip4addr_ntoa(&info->got_ip));
            if(!httpsrv) start_http_srv(&httpsrv, &xWifiMgrCfg);
            break;
        case SYSTEM_EVENT_AP_STAIPASSIGNED:
            ESP_LOGI(TAG, "got client:%s", ip4addr_ntoa(&info->ap_staipassigned));
            if(!httpsrv) start_http_srv(&httpsrv, &xWifiMgrCfg);
            break;
        default:
            break;
    }

    return ESP_OK;
}

static void vTaskWIFIManager(void *pvParameters){
    WIFIManagerConfig *pxCfg = (WIFIManagerConfig *)pvParameters;
    EventBits_t xEventGroupValue = 0;
    while(true){
        xEventGroupValue = xEventGroupWaitBits(*pxCfg->pxEvtGroup,
                    *pxCfg->pxBitsToWaitFor, pdTRUE, pdFALSE, portMAX_DELAY);//1000/portTICK_PERIOD_MS);
        //
        if((xEventGroupValue & APP_EBIT_WIFI_START_AP) != 0) {
            pxCfg->xApChange = 1;
            start_wifi_ap(*pxCfg->ap_ssid, *pxCfg->ap_passwd);
            pxCfg->xApChange = 0;
        }else if((xEventGroupValue & APP_EBIT_WIFI_START_STA) != 0) {
            pxCfg->xApChange = 2;
            ESP_LOGI(TAG, "WILL START STA with %s, %s", *pxCfg->sta_ssid, *pxCfg->sta_passwd);
            start_wifi_sta(*pxCfg->sta_ssid, *pxCfg->sta_passwd);
            pxCfg->xApChange = 0;
        }else{
            // // not needed
            // DELAYMS(500);
        }
        // ESP_LOGI(TAG, "FINISHED WIFIManager");
    }
}

void vApplicationStackOverflowHook( TaskHandle_t xTask,
                                    signed char *pcTaskName ){
    ESP_LOGE(TAG, "stack of :%s", pcTaskName);
}

void app_main()
{
    esp_err_t err = ESP_OK;

    tcpip_adapter_init();

    // system event loop
    ESP_ERROR_CHECK(esp_event_loop_init(system_event_handler, NULL));

    // create tasks
    xAppEventGroup = xEventGroupCreate();
    xTaskCreate(vTaskWIFIManager, "WIFI Mgr", 10240, &xWifiMgrCfg, 6, NULL); //configMAX_PRIORITIES

    // init wifi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // get previous stored wifi configuration
    wifi_config_t wifi_config;
    ESP_ERROR_CHECK(esp_wifi_get_config(ESP_IF_WIFI_STA, &wifi_config));

    if (strlen((const char*) wifi_config.sta.ssid)) {
        ESP_LOGI(TAG, "Found ssid %s",     (const char*) wifi_config.sta.ssid);
        ESP_LOGI(TAG, "Found password %s", (const char*) wifi_config.sta.password);
        *xWifiMgrCfg.sta_ssid = malloc(strlen((char*)wifi_config.sta.ssid)+1);
        strcpy(*xWifiMgrCfg.sta_ssid, (char*)wifi_config.sta.ssid);
        *xWifiMgrCfg.sta_passwd = malloc(strlen((char*)wifi_config.sta.password)+1);
        strcpy(*xWifiMgrCfg.sta_passwd, (char*)wifi_config.sta.password);
        xEventGroupSetBits(xAppEventGroup, APP_EBIT_WIFI_START_STA);
        start_wifi_sta(*xWifiMgrCfg.sta_ssid, *xWifiMgrCfg.sta_passwd);
    }else{
        xEventGroupSetBits(xAppEventGroup, APP_EBIT_WIFI_START_AP);
    }

    // prints
    printf("Hello world!\n");
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    printf("This is ESP8266 chip with %d CPU cores, WiFi, ", chip_info.cores);

    printf("silicon revision %d, ", chip_info.revision);

    printf("%dMB %s flash\n", spi_flash_get_chip_size() / (1024 * 1024),
            (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");

    // for (int i = 0;; i++) {
    //     if(i % 10 == 0){
    //         printf("I lived %d seconds...\n", i*100);
    //     }
    //     DELAYMS(10000);
    // }

    // vTaskStartScheduler();
    // esp_event_loop_delete(system_event_handler);
}
