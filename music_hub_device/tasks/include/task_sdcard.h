#ifndef TASK_SDCARD_H
#define TASK_SDCARD_H

#include "base_types.h"

void task_sdcard(void* task_args);

s32_t sdcard_add_to_file(const char* file_name, u8_t* buf, u32_t buf_len);


#endif /* TASK_SDCARD_H */
