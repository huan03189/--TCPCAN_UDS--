// isotp_user_impl.c
#include "isotp_user.h"
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include <stdarg.h>

#ifdef _WIN32
#include <windows.h>
#endif

static CanSendFunc g_can_send_callback = 0;

#ifdef _WIN32
static LARGE_INTEGER g_start_time;
static LARGE_INTEGER g_frequency;
static int g_started = 0;
#endif

// 设置 CAN 发送函数
void isotp_user_set_send_function(CanSendFunc func) {
    g_can_send_callback = func;
}

// 调试打印
void isotp_user_debug(const char* message, ...) {
    va_list args;
    va_start(args, message);
    vprintf(message, args);
    printf("\n");
    va_end(args);
}

// 发送 CAN 数据
int isotp_user_send_can(const uint32_t arbitration_id, const uint8_t* data, uint8_t size) {
    if (g_can_send_callback) {
        g_can_send_callback(arbitration_id, data, size);
        return 0;
    }
    return -1;
}

// 获取毫秒计时
uint32_t isotp_user_get_ms(void) {
#ifdef _WIN32
    if (g_started == 0) {
        QueryPerformanceFrequency(&g_frequency);
        QueryPerformanceCounter(&g_start_time);
        g_started = 1;
        return 0;
    }

    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);

    uint64_t elapsed_us = (now.QuadPart - g_start_time.QuadPart) * 1000000ULL / g_frequency.QuadPart;
    return (uint32_t)(elapsed_us / 1000ULL);
#else
    // Linux/Unix/macOS
    if (g_started == 0) {
        g_start_time = clock();
        g_started = 1;
        return 0;
    }
    clock_t now = clock();
    return (uint32_t)((now - g_start_time) * 1000 / CLOCKS_PER_SEC);
#endif
}
