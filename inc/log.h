#pragma once
#if(defined(SIMPLE_HTTP_ESP_LOG_SUPPORT) && SIMPLE_HTTP_ESP_LOG_SUPPORT)
#include "esp_log.h"
#define SHTTP_LOGD(tag, format, ...) ESP_LOG_LEVEL_LOCAL(ESP_LOG_DEBUG, tag, format __VA_OPT__(,) __VA_ARGS__)
#define SHTTP_LOGI(tag, format, ...) ESP_LOG_LEVEL_LOCAL(ESP_LOG_INFO, tag, format __VA_OPT__(,) __VA_ARGS__)
#define SHTTP_LOGW(tag, format, ...) ESP_LOG_LEVEL_LOCAL(ESP_LOG_WARN, tag, format __VA_OPT__(,) __VA_ARGS__)
#define SHTTP_LOGE(tag, format, ...) ESP_LOG_LEVEL_LOCAL(ESP_LOG_ERROR, tag, format __VA_OPT__(,) __VA_ARGS__)
#else
#define SHTTP_LOGD(tag, format, ...)
#define SHTTP_LOGI(tag, format, ...)
#define SHTTP_LOGW(tag, format, ...)
#define SHTTP_LOGE(tag, format, ...)
#endif
