#ifndef _TASK_HTTP_H
#define _TASK_HTTP_H

#include "base_types.h"

#define E_HTTP_STATUS_NULL ((u8_t)0xA0)
#define E_HTTP_STATUS_IDEL ((u8_t)0xA1)
#define E_HTTP_STATUS_WORK ((u8_t)0xA2)


void task_http(void *task_param);


void http_send_to_serv(u8_t* buf, u16_t buf_len, u8_t pack_type);
s8_t http_repeat_send(void);

u8_t http_get_device_id(void);
void http_set_device_id(u8_t device_id);

u8_t http_get_status(void);
void http_set_status(u8_t status);


void print_binary(u8_t* buf, u16_t len);

#endif /* _TASK_HTTP_H */
