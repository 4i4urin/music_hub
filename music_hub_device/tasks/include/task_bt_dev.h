#ifndef _TASK_BT_DEV_H
#define _TASK_BT_DEV_H


#include "base_types.h"


void task_bt_dev(void *task_param);


#define QUEUE_HTTP_BT_SIZE  2


typedef struct _t_queue_HttpBtData
{
    u8_t * const ptr_data;
    u16_t data_len;
} t_queue_HttpBtData;

#endif /* _TASK_BT_DEV_H */