#include "app_wifi.h"

static const char * const TAG = "app_wifi";
static wifi_config_t wifi_config;
esp_err_t  start_wifi_sta(char *ssid, char *password)
{
    esp_err_t err = ESP_OK;
    wifi_mode_t mode = 0;
    err = esp_wifi_get_mode(&mode);
    ESP_LOGI(TAG, "starting sta. current mode: %d", mode);
    if(err != ESP_OK || (mode != WIFI_MODE_NULL)){
        // IMPORTANT!!!! toooo many GROUND truth
        esp_wifi_disconnect();
        esp_wifi_stop();
    }
    memset(&wifi_config, 0, sizeof(wifi_config));
    strcpy((char *)wifi_config.sta.ssid, ssid);
    strcpy((char *)wifi_config.sta.password, password);
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "start_wifi_sta COMPLETED");

    return err;
}

esp_err_t start_wifi_ap(const char *ssid, const char *pass)
{
    esp_err_t err = ESP_OK;
    wifi_mode_t mode = 0;
    ESP_LOGI(TAG, "starting ap. current mode: %d", mode);
    err = esp_wifi_get_mode(&mode);
    if(err != ESP_OK || (mode != WIFI_MODE_NULL)){
        esp_wifi_disconnect();
        esp_wifi_stop();
    }
    ESP_LOGI(TAG, "current mode: %d", mode);

    memset(&wifi_config, 0, sizeof(wifi_config));
    wifi_config.ap.max_connection = 3;
    
    strncpy((char *) wifi_config.ap.ssid, ssid, sizeof(wifi_config.ap.ssid));
    wifi_config.ap.ssid_len = strlen(ssid);

    if (strlen(pass) == 0) {
        memset(wifi_config.ap.password, 0, sizeof(wifi_config.ap.password));
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    } else {
        strncpy((char *) wifi_config.ap.password, pass, sizeof(wifi_config.ap.password));
        wifi_config.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;
    }

    ESP_LOGI(TAG, "starting ap with ssid: %s, password: %s",wifi_config.ap.ssid, wifi_config.ap.password );

    /* Start WiFi in AP mode with configuration built above */
    err = esp_wifi_set_mode(WIFI_MODE_AP);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set WiFi mode : %s", esp_err_to_name(err));
        return err;
    }
    err = esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set WiFi config : %s", esp_err_to_name(err));
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

