/* Hello World Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <esp_event_loop.h>
#include <esp_log.h>
#include <nvs_flash.h>

#include <lwip/err.h>
#include <lwip/sys.h>
#include "esp_spi_flash.h"
#include <esp_http_server.h>

const char * TAG="hello";
static uint32_t session_id = UINT32_MAX;
httpd_handle_t *httpsrv = NULL;
static int gAction = 0;
char *wifi_ssid = NULL, *wifi_password = NULL;
static const char *ap_ssid = "blu-esp1";
static const char *ap_pwd = "Hz123456";
static esp_err_t system_event_handler(void *ctx, system_event_t *event);
static esp_err_t  start_wifi_sta(char *ssid, char *password);
static esp_err_t start_wifi_ap(const char *ssid, const char *pass);
static esp_err_t start_http_srv();

esp_err_t  start_wifi_sta(char *ssid, char *password)
{
    esp_err_t err = ESP_OK;
    ESP_LOGI(TAG, "starting sta");
    wifi_mode_t mode = 0;
    err = esp_wifi_get_mode(&mode);
    if(err != ESP_OK || mode != WIFI_MODE_NULL){
        esp_wifi_disconnect();
        esp_wifi_stop();
    }
    wifi_config_t wifi_config;
    // IMPORTANT!!!! toooo many failed truth
    memset(&wifi_config, 0, sizeof(wifi_config));
    strcpy((char *)wifi_config.sta.ssid, ssid);
    strcpy((char *)wifi_config.sta.password, password);
    // wifi_config.sta.scan_method = WIFI_FAST_SCAN;
    // wifi_config.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_sta finished.");

    return err;
}

static esp_err_t start_wifi_ap(const char *ssid, const char *pass)
{
    esp_err_t err = ESP_OK;
    wifi_mode_t mode = 0;
    err = esp_wifi_get_mode(&mode);
    if(err != ESP_OK || mode != WIFI_MODE_NULL){
        esp_wifi_disconnect();
        esp_wifi_stop();
    }
    wifi_config_t wifi_config = {
        .ap = {
            .max_connection = 5,
        },
    };

    esp_wifi_disconnect();
    esp_wifi_stop();
    strncpy((char *) wifi_config.ap.ssid, ssid, sizeof(wifi_config.ap.ssid));
    wifi_config.ap.ssid_len = strlen(ssid);

    if (strlen(pass) == 0) {
        memset(wifi_config.ap.password, 0, sizeof(wifi_config.ap.password));
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    } else {
        strncpy((char *) wifi_config.ap.password, pass, sizeof(wifi_config.ap.password));
        wifi_config.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;
    }

    /* Start WiFi in AP mode with configuration built above */
    err = esp_wifi_set_mode(WIFI_MODE_AP);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set WiFi mode : %d", err);
        return err;
    }
    err = esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set WiFi config : %d", err);
        return err;
    }
    err = esp_wifi_start();
    tcpip_adapter_dhcps_stop(TCPIP_ADAPTER_IF_AP);
    err |= tcpip_adapter_dhcps_start(TCPIP_ADAPTER_IF_AP);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start WiFi : %d", err);
        return err;
    }

    return ESP_OK;
}

static esp_err_t http_wifi_handler(httpd_req_t *req)
{
    esp_err_t ret;
    char *ssid = NULL, *password = NULL;
    bool reconnect = false;

    if(gAction == 1) {
        ret = httpd_resp_send(req, req->uri, sizeof(req->uri));
        return ret;
    }

    for(int i = 0; i < req->num_params*2; i = i+2){
        ESP_LOGD(TAG, "param k: %s, v: %s", req->params[i], req->params[i+1]);
        if(memcmp(req->params[i], "ssid", 4) == 0){
           ssid =  req->params[i+1];
        }else if(memcmp(req->params[i], "password", 8) == 0){
            password = req->params[i+1];
        }
    }

    if(wifi_ssid != NULL) free(wifi_ssid);
    if(wifi_password != NULL) free(wifi_password);

    wifi_ssid = NULL, wifi_password = NULL;
    if(password != NULL && ssid != NULL) {
        wifi_ssid = malloc(strlen(ssid)+1);
        strcpy(wifi_ssid, ssid);
        wifi_password = malloc(strlen(password) + 1);
        strcpy(wifi_password, password);
        reconnect = true;
    }

    ret = httpd_resp_send(req, req->uri, sizeof(req->uri));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "HTTP send failed");
        ret = ESP_FAIL;
    }

    if(reconnect){
        gAction = 1; // reconnect
    }
    
    return ESP_OK;
}

esp_err_t start_http_srv(){
    esp_err_t err = ESP_OK;
    httpsrv = calloc(1, sizeof(httpd_handle_t));
    /* Configure the HTTP server */
    httpd_config_t server_config   = HTTPD_DEFAULT_CONFIG();
    server_config.server_port      = 80;
    server_config.stack_size       = 4096;
    server_config.task_priority    = tskIDLE_PRIORITY + 5;
    server_config.lru_purge_enable = true;
    server_config.max_open_sockets = 1;

    httpd_uri_t config_handler = {
        .uri      = "/wifi",
        .method   = HTTP_GET,
        .handler  = http_wifi_handler,
        .user_ctx = NULL
    };


    if((err =httpd_start(httpsrv, &server_config))!= ESP_OK){
        free(httpsrv);
        ESP_LOGE(TAG, "Failed to start http server: %d", err);
    }

    if ((err = httpd_register_uri_handler(*httpsrv, &config_handler)) != ESP_OK) {
        ESP_LOGE(TAG, "Uri handler register failed: %d", err);
        free(httpsrv);
        return ESP_FAIL;
    }

    return err;
}

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
            if(!httpsrv) start_http_srv();
            break;
        case SYSTEM_EVENT_AP_STAIPASSIGNED:
            ESP_LOGI(TAG, "got client:%s", ip4addr_ntoa(&info->ap_staipassigned));
            if(!httpsrv) start_http_srv();
            break;
        default:
            break;
    }

    return ESP_OK;
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

    for (int i = 0;; i++) {
        if(gAction == 1) {
            start_wifi_sta(wifi_ssid, wifi_password);
            gAction = 0;
        }
        if(i % 10 == 0){
            printf("I lived %d seconds...\n", i);
        }
        vTaskDelay(10000 / portTICK_PERIOD_MS);
    }

     esp_event_loop_delete(system_event_handler);
}
