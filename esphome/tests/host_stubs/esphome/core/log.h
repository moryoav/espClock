#pragma once
#include <cstdio>
#define ESP_LOGI(tag, ...) do { std::printf("%s: ", tag); std::printf(__VA_ARGS__); std::puts(""); } while (0)
#define ESP_LOGW(tag, ...) ESP_LOGI(tag, __VA_ARGS__)
#define ESP_LOGE(tag, ...) ESP_LOGI(tag, __VA_ARGS__)
