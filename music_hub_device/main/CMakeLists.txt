idf_component_register(
    SRCS 
        "main.c" 
        "crc.c"
        
        "../modules/esp_http_client/esp_http_client_user.c"
        "../modules/esp_http_client/lib/http_auth.c"
        "../modules/esp_http_client/lib/http_header.c"
        "../modules/esp_http_client/lib/http_utils.c" 

        "../tasks/task_http.c"
        "../tasks/task_test_com.c"
        "../tasks/task_sdcard.c"

        "../modules/client_serv_com/cserver_com_receiver.c"
        "../modules/client_serv_com/cserver_com_sender.c"

    INCLUDE_DIRS 
        "."

        "../modules/esp_http_client/include"
        "../modules/esp_http_client/lib/include"

        "../modules/client_serv_com/include"

        "../tasks/include"
        "common_inc"

    REQUIRES
        "esp_wifi"
        "nvs_flash"
        "tcp_transport"
        "nghttp"
        "esp_peripherals"
)


