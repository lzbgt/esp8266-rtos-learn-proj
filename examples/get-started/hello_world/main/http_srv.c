#include "common.h"

static const char *const TAG="app_httpsrv";

extern int gAction;
extern char *wifi_ssid, *wifi_password;

esp_err_t http_wifi_handler(httpd_req_t *req)
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

esp_err_t start_http_srv(httpd_handle_t *srv){
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
        .user_ctx = NULL
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