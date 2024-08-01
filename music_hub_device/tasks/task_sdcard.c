#include "task_sdcard.h"

#include <string.h>
#include "board.h"
#include "unistd.h"
#include "fcntl.h"



#define FOLDER_NAME "/sdcard/.service/"
#define MAX_PATH_LEN 256


static u16_t build_path(char* path, const char* file_name, const u16_t max_path_len);
static esp_periph_set_handle_t init_sdcard(void);

// DBG
static const char write_buf[] = 
"The machines rose from the ashes of the nuclear fire. Their war to exterminate\n"
"mankind had raged for decades, but the final battle would not be fought in the future. It\n"
"would be fought here, in our present. Tonight\n\n";


void task_sdcard(void* task_args)
{
    esp_periph_set_handle_t periph = init_sdcard();
    s32_t status = 0, count = 0;
    while (1)
    {
        vTaskDelay(2000 / portTICK_PERIOD_MS);

        if (count > 5)
        {
            printf("WARNING: SDCARD Stop writing count = %d\n", count);
            continue;
        }
        status = sdcard_add_to_file("Terminator.txt", (u8_t*)&write_buf[0], strlen(write_buf));
        if (status <= 0)
            printf("ERROR: SDCARD Can't transmit data count = %d status = %d\n", count, status);
        else
            printf("SUCCESS: SDCARD write text\n");

        count += 1;
        
    }

    esp_periph_set_stop_all(periph);
    esp_periph_set_destroy(periph);
}


static esp_periph_set_handle_t init_sdcard(void)
{
    esp_periph_config_t periph_cfg = DEFAULT_ESP_PERIPH_SET_CONFIG();
    esp_periph_set_handle_t set = esp_periph_set_init(&periph_cfg);
    audio_board_sdcard_init(set, SD_MODE_1_LINE);
    return set;
}


s32_t sdcard_add_to_file(const char* file_name, u8_t* buf, u32_t buf_len)
{
    char path[MAX_PATH_LEN] = FOLDER_NAME;
    s32_t path_len = build_path(path, file_name, MAX_PATH_LEN);
    if (path_len <= 0)
    {
        printf("ERROR: SDCARD building path name\n");
        return -1;
    }

    const char* path_ptr = &path[0];
    s32_t file = open(path_ptr, O_WRONLY | O_CREAT | O_TRUNC, S_IRWXU);
    if (file == -1) {
        printf("Failed to open. File name: %s\n", path);
        return file;
    }

    s32_t wlen = write(file, buf, buf_len);
    fsync(file);
    if (wlen == -1) {
        printf("The error is happened in writing data\n");
        return -1;
    }
    close(file);
    return wlen;    
}


static u16_t build_path(char* path, const char* file_name, const u16_t max_path_len)
{
    u16_t path_len = strlen(file_name) + strlen(path);
    if ((path_len + 1) >= max_path_len)
        return -1;

    u16_t start_len = strlen(path);
    for (u16_t i = start_len; i < path_len; i++)
        path[i] = file_name[i - start_len];

    path[path_len] = 0;
    printf("PATH = %s\n", path);
    return strlen(path);
}
