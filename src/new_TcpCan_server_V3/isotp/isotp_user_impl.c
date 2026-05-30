#include "isotp_user.h"
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdint.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#endif

typedef void (*CanSendFunc)(uint32_t id, const uint8_t* data, uint8_t len);
static CanSendFunc g_can_send_callback = NULL;

void isotp_user_set_send_function(CanSendFunc func) {
    g_can_send_callback = func;
}
void isotp_user_debug(const char* message, ...) {
#ifdef ISO_TP_DEBUG
    printf("ISOTP DEBUG: %s\n", message);
#else
    (void)message;
#endif
}

// 2. 实现 CAN 发送函数
int isotp_user_send_can(const uint32_t arbitration_id, const uint8_t* data, const uint8_t size) {
if (g_can_send_callback != NULL) {
        g_can_send_callback(arbitration_id, data, size);
        return 0; // 成功
    }

  return -1; // 返回 0 表示成功 (假设 ISOTP_RET_OK 是 0)
}

// 3. 实现获取时间函数 (毫秒)
uint32_t isotp_user_get_ms(void) {

#ifdef _WIN32
    return (uint32_t)GetTickCount(); // 毫秒级，Windows 原生
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint32_t)(tv.tv_sec * 1000ULL + tv.tv_usec / 1000);
#endif
}
