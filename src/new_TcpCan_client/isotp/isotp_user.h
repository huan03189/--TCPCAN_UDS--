#ifndef __ISOTP_USER_H__
#define __ISOTP_USER_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*CanSendFunc)(uint32_t id, const uint8_t* data, uint8_t len);

/* user implemented, set send callback */
void isotp_user_set_send_function(CanSendFunc func);

/* user implemented, print debug message */
void isotp_user_debug(const char* message, ...);

/* user implemented, send can message. should return ISOTP_RET_OK when success.
*/
int  isotp_user_send_can(const uint32_t arbitration_id,
                        const uint8_t* data, const uint8_t size);

/* user implemented, get millisecond */
uint32_t isotp_user_get_ms(void);

#ifdef __cplusplus
}
#endif

#endif // __ISOTP_USER_H__
