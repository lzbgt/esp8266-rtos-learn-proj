#include "app_common.h"

static const char *const TAG="app_httpsrv";
static char pcMsgWifiInProcess[] = "WIFI config is in processing, command ignored";

esp_err_t http_wifi_handler(httpd_req_t *req)
{
    esp_err_t ret;
    char *ssid = NULL, *password = NULL;
    WIFIManagerConfig *xWifiCfg = (WIFIManagerConfig *)req->user_ctx;

    // wifi config is not finished, don't accept command
    if(xWifiCfg->xApChange) {
        ret = httpd_resp_send(req, pcMsgWifiInProcess, sizeof(pcMsgWifiInProcess));
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

    if(password != NULL && ssid != NULL) {
        if(*xWifiCfg->sta_ssid != NULL) {
            free(*xWifiCfg->sta_ssid);
            free(*xWifiCfg->sta_passwd);
        }
        *xWifiCfg->sta_ssid = malloc(strlen(ssid)+1);
        strcpy(*xWifiCfg->sta_ssid, ssid);
        *xWifiCfg->sta_passwd = malloc(strlen(password) + 1);
        strcpy(*xWifiCfg->sta_passwd, password);
        ESP_LOGI(TAG, "got ssid: %s password: %s from httpsrv", *xWifiCfg->sta_ssid, *xWifiCfg->sta_passwd);
        ESP_LOGI(TAG, "sent STA bootstrapping");
        xEventGroupSetBits(*xWifiCfg->pxEvtGroup, APP_EBIT_WIFI_START_STA);
    }

    ret = httpd_resp_send(req, req->uri, sizeof(req->uri));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "HTTP send failed");
    }
    ESP_LOGI(TAG, "FINISHED STA bootstrapping");
    
    return ret;
}

esp_err_t start_http_srv(httpd_handle_t *srv, WIFIManagerConfig *xWifiCfg){
    esp_err_t err = ESP_OK;
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
        .user_ctx = xWifiCfg
    };

    if((err =httpd_start(srv, &server_config))!= ESP_OK){
        free(srv);
        ESP_LOGE(TAG, "Failed to start http server: %d", err);
    }

    if ((err = httpd_register_uri_handler(*srv, &config_handler)) != ESP_OK) {
        ESP_LOGE(TAG, "Uri handler register failed: %d", err);
        free(srv);
        return ESP_FAIL;
    }

    return err;
}