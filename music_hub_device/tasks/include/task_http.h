#ifndef _TASK_HTTP_H
#define _TASK_HTTP_H

#include "base_types.h"

#define E_HTTP_ERROR_REPEAT_SEND ((s32_t)0x80000000)
#define E_HTTP_ERROR_UNEXPECTED ((s32_t)-1)
#define E_HTTP_ERROR_WAIT ((s32_t)-2)


void task_http(void *task_param);


void http_create_msg(u8_t* buf, u16_t buf_len, u8_t pack_type);

u8_t http_get_device_id(void);
void http_set_device_id(u8_t device_id);

u8_t http_get_status(void);
void http_set_status(u8_t status);


void print_binary(u8_t* buf, u16_t len);

#endif /* _TASK_HTTP_H */
