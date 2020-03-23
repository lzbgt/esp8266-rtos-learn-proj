#ifndef __APP_COMMON_H__
#define __APP_COMMON_H__
#include <FreeRTOS.h>
#include <esp_log.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <string.h>
#include <esp_http_server.h>

#define DELAYMS(ms) do {\
vTaskDelay((ms)/portTICK_PERIOD_MS); \
}while(0);

#endif