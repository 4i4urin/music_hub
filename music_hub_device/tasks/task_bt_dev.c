#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "fcntl.h"
#include "unistd.h"
#include "esp_peripherals.h"
#include "esp_gap_bt_api.h"

#include "nvs_flash.h"
#include "audio_element.h"
#include "audio_pipeline.h"
#include "board.h"
#include "mp3_decoder.h"
#include "a2dp_stream.h"

#include "raw_stream.h"

#include "task_bt_dev.h"
#include "base_types.h"
#include "client_serv_prot.h"

#include "private.h"

#define BT_REMOTE_NAME  "JBL Charge 5"
#define DEVICE_NAME     "SHISHEL_DEV"

#define SEND_SIZE (1 * 1024)
#define MP3_OUTPUT_RB_SIZE (2 * 1024)



typedef u8_t esp_peer_bdname_t[ESP_BT_GAP_MAX_BDNAME_LEN + 1];
static esp_peer_bdname_t remote_bt_device_name;

static esp_bd_addr_t remote_bd_addr = {0};

static u8_t device_found = 0;
static u8_t device_connect = 0;

// static t_csp_track_pack queue_data;
extern volatile QueueHandle_t QueueHttpBtdev;
extern volatile QueueHandle_t QueueHttpBtStatus;

esp_periph_set_handle_t init_sdcard(void);
audio_element_handle_t init_mp3_decoder(void);
audio_element_handle_t init_bt_stream(void);
audio_element_handle_t init_raw_reader(void);
audio_pipeline_handle_t init_pipeline(
    audio_element_handle_t raw_reader,
    audio_element_handle_t bt_stream_writer, 
    audio_element_handle_t mp3_decoder);

u16_t send_to_bluetooth_pipline(
    audio_element_handle_t raw_reader, u8_t* buf, u16_t buf_len);
static t_queue_HttpBtData* btdev_read_track_queue(t_queue_HttpBtData* ptrack_data);
static void write_status_queue(u8_t connect_status);

// #define WAV

#ifdef WAV
audio_element_handle_t init_wav_decoder(void);
#endif


void task_bt_dev(void *task_param)
{
    esp_periph_config_t periph_cfg = DEFAULT_ESP_PERIPH_SET_CONFIG();
    esp_periph_set_handle_t set = esp_periph_set_init(&periph_cfg);

#ifdef WAV
    printf("Use WAV\n");
    audio_element_handle_t mp3_decoder = init_wav_decoder();
#else
    audio_element_handle_t mp3_decoder = init_mp3_decoder();
#endif

    audio_element_handle_t bt_stream_writer = init_bt_stream();
    audio_element_handle_t raw_reader = init_raw_reader();
    
    audio_pipeline_handle_t pipeline = init_pipeline(raw_reader, bt_stream_writer, mp3_decoder);
    
    printf("[3.7] Create bt peripheral\n");
    esp_periph_handle_t bt_periph = bt_create_periph();

    printf("[3.8] Start bt peripheral\n");
    esp_periph_start(set, bt_periph);

    printf("[ 5 ] Start audio_pipeline\n");
    audio_pipeline_run(pipeline);
    
    vTaskDelay(100 / portTICK_PERIOD_MS);

    t_queue_HttpBtData* pqueue_data = NULL;
    t_queue_HttpBtData queue_data = { 0 };
    u16_t pack_size = 0, written_bytes = 0;
    u16_t send_size = SEND_SIZE;
    while (1)
    {
        if (!device_connect)
        {
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            continue;
        }

        pqueue_data = btdev_read_track_queue(&queue_data);
        if (pqueue_data == NULL)
        {
            vTaskDelay(500 / portTICK_PERIOD_MS);
            continue;
        }
        pack_size = pqueue_data->data_len;
        while (pack_size)
        {
            send_size = (pack_size > send_size) ? SEND_SIZE : pack_size;

            written_bytes = send_to_bluetooth_pipline(
                raw_reader, 
                pqueue_data->ptr_data + pqueue_data->data_len - pack_size, 
                send_size);

            if (send_size != (u16_t)written_bytes)
                printf("WARNING: [BT_DEV] lose some bytes\n");
            
            pack_size -= (u16_t)written_bytes;
            vTaskDelay(25 / portTICK_PERIOD_MS);
        }
        printf("SEND TO BT full pack\n");
    }

}


static t_queue_HttpBtData* btdev_read_track_queue(t_queue_HttpBtData* ptrack_data)
{
    const u8_t queue_recive_timout = 1;
    printf("TRY read queue\n");
    portBASE_TYPE xStatus = xQueueReceive( QueueHttpBtdev, ptrack_data, queue_recive_timout );
    if ( xStatus == pdPASS )
    {
        printf("TRACK GET data = %x, len = %d\n", ptrack_data->ptr_data[0], ptrack_data->data_len);
        return ptrack_data;
    }
    else
        return NULL;
}

//--------------------------------------------------------------------------------------
// service stuf

u16_t send_to_bluetooth_pipline(
    audio_element_handle_t raw_reader,
    u8_t* buf, u16_t buf_len)
{
    s32_t res = raw_stream_write(raw_reader, (char*)buf, buf_len);
    if (res < 0)
        printf("Something whent wrong\n");

    return res;
}


void write_status_queue(u8_t connect_status)
{
    const u8_t queue_send_timout = 5;
    if ( xQueueSend( QueueHttpBtStatus, (void*)&connect_status, queue_send_timout ) == pdPASS )
        printf("HTTP QUEUE STATUS: send successe\n");
    else
        printf("HTTP QUEUE STATUS: send FAILD\n");
}

//--------------------------------------------------------------------------------------
// init

esp_periph_set_handle_t init_sdcard(void)
{
    printf("[ 1 ] Mount sdcard\n");
    esp_periph_config_t periph_cfg = DEFAULT_ESP_PERIPH_SET_CONFIG();
    esp_periph_set_handle_t set = esp_periph_set_init(&periph_cfg);

    audio_board_sdcard_init(set, SD_MODE_1_LINE);
    return set;
}


audio_element_handle_t init_mp3_decoder(void)
{
    audio_element_handle_t mp3_decoder;

    audio_board_handle_t board_handle = audio_board_init();
    audio_hal_ctrl_codec(board_handle->audio_hal, AUDIO_HAL_CODEC_MODE_DECODE, AUDIO_HAL_CTRL_START);
    
    printf("[3.2] Create mp3 decoder to decode mp3 file\n");
    mp3_decoder_cfg_t mp3_cfg = DEFAULT_MP3_DECODER_CONFIG();
    mp3_cfg.out_rb_size *= 2;

    mp3_decoder = mp3_decoder_init(&mp3_cfg);
    return mp3_decoder;
}


audio_element_handle_t init_raw_reader(void)
{
    raw_stream_cfg_t raw_cfg = {
        .type = AUDIO_STREAM_READER,
        .out_rb_size = SEND_SIZE * 2,
    };
    audio_element_handle_t raw_reader = raw_stream_init(&raw_cfg);

    return raw_reader;
}


#ifdef WAV
#include "wav_decoder.h"
audio_element_handle_t init_wav_decoder(void)
{
    audio_element_handle_t wav_decoder;

    printf("[3.2] Create wav decoder to decode wav file\n");
    wav_decoder_cfg_t wav_cfg = DEFAULT_WAV_DECODER_CONFIG();
    wav_decoder = wav_decoder_init(&wav_cfg);

    return wav_decoder;
}
#endif


static void bt_app_a2d_cb(esp_a2d_cb_event_t event, esp_a2d_cb_param_t *param);
static void bt_app_gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param);

audio_element_handle_t init_bt_stream(void)
{
    audio_element_handle_t bt_stream_writer;

    printf("[3.3] Create Bluetooth stream\n");

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();

    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_BLE));
    ESP_ERROR_CHECK(esp_bt_controller_init(&bt_cfg));
    ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT));
    ESP_ERROR_CHECK(esp_bluedroid_init());
    printf("KEKEKEK\n\n");
    ESP_ERROR_CHECK(esp_bluedroid_enable());

    esp_bt_dev_set_device_name(DEVICE_NAME);
    esp_bt_gap_register_callback(bt_app_gap_cb);
    
    memcpy(&remote_bt_device_name, BT_REMOTE_NAME, strlen(BT_REMOTE_NAME) + 1);
    
    a2dp_stream_config_t a2dp_config = 
    {
        .type = AUDIO_STREAM_WRITER,

        .user_callback = {
            .user_a2d_cb = bt_app_a2d_cb,
        },
    };
    bt_stream_writer = a2dp_stream_init(&a2dp_config);

    esp_bt_gap_start_discovery(ESP_BT_INQ_MODE_GENERAL_INQUIRY, 10, 0);
    esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
    printf("SOME SHEET\n\n");
    return bt_stream_writer;
}


audio_pipeline_handle_t init_pipeline(
    audio_element_handle_t raw_reader, audio_element_handle_t bt_stream_writer, 
    audio_element_handle_t mp3_decoder)
{
    audio_pipeline_handle_t pipeline;

    printf("[3.0] Create audio pipeline for playback\n");
    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    pipeline = audio_pipeline_init(&pipeline_cfg);

    mem_assert(pipeline);

    printf("[3.4] Register all elements to audio pipeline\n");
    
    audio_pipeline_register(pipeline, raw_reader, "RAW");
    audio_pipeline_register(pipeline, mp3_decoder, "mp3");
    audio_pipeline_register(pipeline, bt_stream_writer, "bt");

    printf("[3.5] Link it together raw_reader-->mp3_decoder-->"
        "bt_stream-->[bt sink]\n");

    const char *link_tag[3] = {"RAW", "mp3", "bt"};
    audio_pipeline_link(pipeline, &link_tag[0], 3);
    
    return pipeline;
}

//--------------------------------------------------------------------------------------
// bluetooth callbacks

static void bt_app_a2d_cb(esp_a2d_cb_event_t event, esp_a2d_cb_param_t *param)
{
    esp_a2d_cb_param_t *a2d = NULL;

    switch (event) {
        case ESP_A2D_MEDIA_CTRL_ACK_EVT:
            a2d = (esp_a2d_cb_param_t *)(param);
            if (a2d->media_ctrl_stat.cmd == ESP_A2D_MEDIA_CTRL_CHECK_SRC_RDY &&
                a2d->media_ctrl_stat.status == ESP_A2D_MEDIA_CTRL_ACK_SUCCESS)
            {
                printf("A2DP device connected\n");
                device_connect = 1;
                write_status_queue(device_connect);
            }
            break;
        default:
            break;
    }
}


static void filter_inquiry_scan_result(esp_bt_gap_cb_param_t *param);

static void bt_app_gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param)
{
    switch (event) {
        case ESP_BT_GAP_DISC_RES_EVT:
            filter_inquiry_scan_result(param);
            break;

        case ESP_BT_GAP_DISC_STATE_CHANGED_EVT:
            if (param->disc_st_chg.state != ESP_BT_GAP_DISCOVERY_STOPPED)
                break;
            
            if (device_found) {
                printf("Device discovery stopped.\n");
                printf("a2dp connecting to peer: %s\n", remote_bt_device_name);
                device_found = 0;
                esp_a2d_source_connect(remote_bd_addr);
            } else {
                // not discovered, continue to discover
                printf("Device discovery failed, continue to discover...\n");
                esp_bt_gap_start_discovery(ESP_BT_INQ_MODE_GENERAL_INQUIRY, 10, 0);
            }
            break;

        default:
            break;
    }
    return;
}


static bool get_name_from_eir(u8_t *eir, u8_t *bdname);

static void filter_inquiry_scan_result(esp_bt_gap_cb_param_t *param)
{
    u8_t *eir = NULL;
    esp_peer_bdname_t peer_bdname;
    esp_bt_gap_dev_prop_t *p;

    for (s32_t i = 0; i < param->disc_res.num_prop; i++) {
        p = param->disc_res.prop + i;
        switch (p->type) {
            case ESP_BT_GAP_DEV_PROP_EIR:
                eir = (u8_t*)(p->val);
                break;
            default:
                break;
        }
    }

    /* search for device named "peer_bdname" in its extended inquiry response */
    if (eir == NULL)
        return;

    get_name_from_eir(eir, (u8_t*)&peer_bdname);
    if (strcmp((char *)peer_bdname, (char *)remote_bt_device_name) != 0) {
        return;
    }

    device_found = 1;
    printf("Found a target device, name %s\n", (u8_t*)peer_bdname);
    memcpy(&remote_bd_addr, param->disc_res.bda, ESP_BD_ADDR_LEN);
    printf("Cancel device discovery ...\n");
    esp_bt_gap_cancel_discovery();
}


static bool get_name_from_eir(u8_t *eir, u8_t*bdname)
{
    u8_t *rmt_bdname = NULL;
    u8_t rmt_bdname_len = 0;

    if (!eir) {
        return false;
    }

    rmt_bdname = esp_bt_gap_resolve_eir_data(eir, ESP_BT_EIR_TYPE_CMPL_LOCAL_NAME, &rmt_bdname_len);
    if (!rmt_bdname) {
        rmt_bdname = esp_bt_gap_resolve_eir_data(eir, ESP_BT_EIR_TYPE_SHORT_LOCAL_NAME, &rmt_bdname_len);
    }

    if (rmt_bdname) {
        if (rmt_bdname_len > ESP_BT_GAP_MAX_BDNAME_LEN) {
            rmt_bdname_len = ESP_BT_GAP_MAX_BDNAME_LEN;
        }

        if (bdname) {
            memcpy(bdname, rmt_bdname, rmt_bdname_len);
            bdname[rmt_bdname_len] = '\0';
        }
        return true;
    }

    return false;
}


