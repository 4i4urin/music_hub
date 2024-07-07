#include "task_test_com.h"
#include "freertos/FreeRTOS.h"

static esp_event_loop_handle_t _loop_handle;

void task_test_com(void* task_args)
{
    esp_event_loop_args_t loop_args = {
        .queue_size = 32,
        .task_name = "EVENT_COM",
        .task_priority = 1,
        .task_stack_size = 1 << 12,
        .task_core_id = 0
    };
    // esp_event_loop_create_default
    esp_err_t err = esp_event_loop_create(&loop_args, &_loop_handle);
    if (err == ESP_OK) {
        printf("Dedicated event loop created successfully\n");
    } else {
        printf("Failed to create event loop!\n");
    };

    while (1)
    {
        printf("Every 1 sec i print this message\n");
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}


void event_com(
    void* handler_arg, esp_event_base_t base, int32_t id, void* event_data
) {
    e_event_com_id e_id = (e_event_com_id)id;
    t_event_com_data* event_msg = (t_event_com_data*)event_data;
    switch (e_id)
    {
    case E_EVENT_COM_PRINT:
        printf("size of msg = %d\n",  event_msg->size);
        printf("msg: %s\n",  event_msg->str);
        break;
    
    case E_EVENT_COM_WAIT:
        printf("JUST wait msg\n");
        break;

    default:
        printf("Unknown event id\n");
        break;
    }
}


esp_event_loop_handle_t event_com_get_handle(void)
{
    return _loop_handle;
}

