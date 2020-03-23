#include "app_wifi.h"

static const char * const TAG = "app_wifi";
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

esp_err_t start_wifi_ap(const char *ssid, const char *pass)
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

