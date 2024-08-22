#ifndef _TASK_HTTP_H
#define _TASK_HTTP_H


#define E_HTTP_STATUS_NULL ((u8_t)0xA0)
#define E_HTTP_STATUS_IDEL ((u8_t)0xA1)
#define E_HTTP_STATUS_WORK ((u8_t)0xA2)


void task_http(void *task_param);

#endif /* _TASK_HTTP_H */
