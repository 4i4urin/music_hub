#ifndef TASK_TEST_COM
#define TASK_TEST_COM

#include "esp_event.h"

void task_test_com(void* task_args);
void event_com(void* handler_arg, esp_event_base_t base, int32_t id, void* event_data);
esp_event_loop_handle_t event_com_get_handle(void);

#define EVENT_COM_BASE "EVENT_COM"
#define EVENT_COM_ID   1

#define EVENT_MSG_SIZE_LIM 100

typedef enum _e_event_com_id
{
    E_EVENT_COM_PRINT,
    E_EVENT_COM_WAIT
} e_event_com_id;

typedef struct _t_event_com_data
{
    char str[EVENT_MSG_SIZE_LIM];
    uint8_t size;
} t_event_com_data;

#endif /* TASK_TEST_COM */
