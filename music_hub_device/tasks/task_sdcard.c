#include "task_sdcard.h"

#include <string.h>
#include "board.h"
#include "unistd.h"
#include "fcntl.h"

#include "client_serv_prot.h"

#define FOLDER_NAME "/sdcard/.service/"
#define MAX_PATH_LEN 256
#define MAX_FILE_NAME_LEN 100


static u16_t build_path(char* path, const char* file_name, const u16_t max_path_len);
static esp_periph_set_handle_t init_sdcard(void);
static t_csp_track_pack* sdcard_read_queue(t_csp_track_pack* track_data);
static u8_t get_file_name(t_csp_track_pack* ptrack_data, char* file_name, u8_t max_file_name_len);


static t_csp_track_pack queue_data;
extern volatile QueueHandle_t QueueHttpSD;

static char file_name[MAX_FILE_NAME_LEN] = { 0 };

// DBG
// static const char write_buf[5][80] = {
//     "The machines rose from the ashes of the nuclear fire.\n",
//     "Their war to exterminate mankind had raged for decades,\n",
//     "but the final battle would not be fought in the future.\n",
//     "It would be fought here, in our present.\n",
//     "Tonight\n\n"
// };


void task_sdcard(void* task_args)
{
    esp_periph_set_handle_t periph = init_sdcard();
    s32_t status = 0, count = 0;

    t_csp_track_pack* pqueue_data = NULL;
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    // DBG
    while (1)
    {
        vTaskDelay(450 / portTICK_PERIOD_MS);

        if (count > 4)
            count = 0;

        pqueue_data = sdcard_read_queue(&queue_data);
        if (pqueue_data == NULL)
            continue; // ????? DO NOT KNOW WHAT TO DO
        get_file_name(pqueue_data, file_name, MAX_FILE_NAME_LEN);

        status = sdcard_add_to_file(file_name, (u8_t*)pqueue_data->track, sizeof(pqueue_data->track));
        // status = sdcard_add_to_file("Terminator.txt", (u8_t*)&write_buf[count], strlen(write_buf[count]));
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


static u8_t hexu16_to_string(u16_t hex_num, char* str, u8_t max_str_len);

static u8_t get_file_name(t_csp_track_pack* ptrack_data, char* file_name, u8_t max_file_name_len)
{
    if (ptrack_data == NULL || file_name == NULL)
        return -1;

    u16_t track_id = ptrack_data->track_id;
    char file_ext[5] = { 0 };
    if ( ptrack_data->track_format == ECSP_TRACK_FORMAT_SBC)
        strcpy(file_ext, ".sbc");
    else
        strcpy(file_ext, ".mp3");
    
    u8_t name_len = hexu16_to_string(track_id, file_name, max_file_name_len);
    for (u8_t i = 0; i < strlen(file_ext); i++)
        file_name[i + name_len] = file_ext[i];

    return strlen(file_name);
}


static u8_t hexu16_to_string(u16_t hex_num, char* str, u8_t max_str_len)
{
    u8_t sim = 0, i;
    for (i = 0; hex_num && i < max_str_len; i++)
    {
        sim = (u8_t)((hex_num & (u16_t)0xF000) >> 12);
        if (sim < 10)
            str[i] = sim + '0';
        else 
            str[i] = sim - 10 + 'A';

        hex_num <<= 4;
    }
    return i;
}


static t_csp_track_pack* sdcard_read_queue(t_csp_track_pack* ptrack_data)
{
    const u8_t queue_recive_timout = 10;
    portBASE_TYPE xStatus = xQueueReceive( QueueHttpSD, ptrack_data, queue_recive_timout );
    if ( xStatus == pdPASS )
    {
        return ptrack_data;
    } else
    {
        return NULL;
    }
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
    // O_APPEND  O_WRONLY
    s32_t file = open(path_ptr, O_RDWR | O_APPEND | O_CREAT);
    if (file == -1) {
        printf("Failed to open. File name: %s\n", path);
        return file;
    }
    // vTaskDelay(10 / portTICK_PERIOD_MS);
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
    // DBG
    printf("PATH = %s\n", path);
    return strlen(path);
}
