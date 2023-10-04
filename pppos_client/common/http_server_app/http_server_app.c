/* Simple HTTP Server Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>
#include <sys/param.h>
#include "esp_netif.h"
#include "esp_eth.h"
#include "driver/uart.h"
#include <esp_http_server.h>
#include "http_server_app.h"
/* A simple example that demonstrates how to create GET and POST
 * handlers for the web server.
 */

static const char *TAG = "HTTP_Server";
static httpd_handle_t server = NULL;
/* An HTTP GET handler */
static httpd_req_t *REQ;


#define BUF_SIZE (1024)
#define RD_BUF_SIZE (BUF_SIZE)
#define EX_UART_NUM UART_NUM_2

typedef struct{
    char buffer_rec[BUF_SIZE];//buffer chua du lieu tra ve
    bool flag_have_rec;//flag kiem tra co du lieu tra ve thong qua uart khong
} IR_command_t;
IR_command_t IR_command;

static QueueHandle_t uart0_queue; // queue de luu su kien uart
static void uart_event_task(void *pvParameters)
{
    uart_event_t event;
    uint8_t* dtmp = (uint8_t*) malloc(RD_BUF_SIZE);
    for(;;) {
        //Waiting for UART event.
        if(xQueueReceive(uart0_queue, (void * )&event, (portTickType)portMAX_DELAY)) {
            bzero(dtmp, RD_BUF_SIZE);
            switch(event.type) {
                case UART_DATA:
                    uart_read_bytes(EX_UART_NUM, dtmp, event.size, portMAX_DELAY);
                    IR_command.flag_have_rec = true;
                    memcpy(IR_command.buffer_rec,dtmp,strlen((char*)dtmp));
                    ESP_LOGI(TAG, "Data: %s", IR_command.buffer_rec);
                    break;
                default:
                    ESP_LOGI(TAG, "uart event type: %d", event.type);
                    break;
            }
        }
    }
    free(dtmp);
    dtmp = NULL;
    vTaskDelete(NULL);
}

void uart_start(){
    /* Configure parameters of an UART driver,
     * communication pins and install the driver */
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };
    //Install UART driver, and get the queue.
    uart_driver_install(EX_UART_NUM, BUF_SIZE * 2, 0, 20, &uart0_queue, 0);
    uart_param_config(EX_UART_NUM, &uart_config);
    //Set UART log level
    esp_log_level_set(TAG, ESP_LOG_INFO);
    //Set UART pins
    uart_set_pin(EX_UART_NUM, 17, 16, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    //Create a task to handler UART event
    xTaskCreate(uart_event_task, "uart_event_task", 2048, NULL, 10, NULL);
}
static esp_err_t hello_get_handler(httpd_req_t *req)
{
    extern const uint8_t index_html_start[] asm("_binary_index_html_start");
    extern const uint8_t index_html_end[] asm("_binary_index_html_end");
    httpd_resp_set_type(req, "text/html");
    const char* resp_str = (const char*) index_html_start;
    httpd_resp_send(req, resp_str, index_html_end - index_html_start);
    return ESP_OK;
}

static const httpd_uri_t hello = {
    .uri       = "/hello",
    .method    = HTTP_GET,
    .handler   = hello_get_handler,
    .user_ctx  = "Hello World!"
};

static esp_err_t samsung_air_handler(httpd_req_t *req)
{
    extern const uint8_t index2_html_start[] asm("_binary_index2_html_start");
    extern const uint8_t index2_html_end[] asm("_binary_index2_html_end");
    httpd_resp_set_type(req, "text/html");
    const char* resp_str = (const char*) index2_html_start;
    httpd_resp_send(req, resp_str, index2_html_end - index2_html_start);
    return ESP_OK;
}

static const httpd_uri_t samsung_air = {
    .uri       = "/air",
    .method    = HTTP_GET,
    .handler   = samsung_air_handler,
    /* Let's pass response string in user
     * context to demonstrate it's usage */
    .user_ctx  = "Hello World!"
};

static esp_err_t fan_air_handler(httpd_req_t *req)
{
    extern const uint8_t index2_html_start[] asm("_binary_index3_html_start");
    extern const uint8_t index2_html_end[] asm("_binary_index3_html_end");
    httpd_resp_set_type(req, "text/html");
    const char* resp_str = (const char*) index2_html_start;
    httpd_resp_send(req, resp_str, index2_html_end - index2_html_start);
    return ESP_OK;
}

static const httpd_uri_t pan = {
    .uri       = "/pan",
    .method    = HTTP_GET,
    .handler   = fan_air_handler,
    /* Let's pass response string in user
     * context to demonstrate it's usage */
    .user_ctx  = "Hello World!"
};

static esp_err_t samsung_handler(httpd_req_t *req)
{
    extern const uint8_t index1_html_start[] asm("_binary_index1_html_start");
    extern const uint8_t index1_html_end[] asm("_binary_index1_html_end");
    httpd_resp_set_type(req, "text/html");
    const char* resp_str = (const char*) index1_html_start;
    httpd_resp_send(req, resp_str, index1_html_end - index1_html_start);
    return ESP_OK;
}
static const httpd_uri_t samsung = {
    .uri       = "/samsung",
    .method    = HTTP_GET,
    .handler   = samsung_handler,
    /* Let's pass response string in user
     * context to demonstrate it's usage */
    .user_ctx  = "Hello World!"
};
static esp_err_t echo_post_handler(httpd_req_t *req)
{
    char buf[100];
    int data_len = req->content_len;

    /* Read the data for the request */
    httpd_req_recv(req, buf,data_len);
    /* Log data received */
    printf("Data:%s",buf);

    ESP_LOGI(TAG, "Data recv: %.*s", data_len, buf);
    if(strstr(buf,"SS_ON")){
        uart_write_bytes(EX_UART_NUM, "SS_ON_OK", strlen("SS_ON_OK"));
    }
    else if(strstr(buf,"SS_UP_VOL")){
        uart_write_bytes(EX_UART_NUM, "SS_UP_VOL_OK", strlen("SS_UP_VOL_OK"));
    }
    else if(strstr(buf,"SS_DOWN_VOL")){
        uart_write_bytes(EX_UART_NUM, "SS_DOWN_VOL_OK", strlen("SS_DOWN_VOL_OK"));
    }
    else if(strstr(buf,"SS_DOWN_CH")){
        uart_write_bytes(EX_UART_NUM, "SS_DOWN_CH_OK", strlen("SS_DOWN_CH_OK"));
    }
    else if(strstr(buf,"SS_UP_CH")){
        uart_write_bytes(EX_UART_NUM, "SS_UP_CH_OK", strlen("SS_UP_CH_OK"));
    }
    else if(strstr(buf,"SS_UP_Cir")){
        uart_write_bytes(EX_UART_NUM, "SS_UP_Cir_OK", strlen("SS_UP_Cir_OK"));
    }
    else if(strstr(buf,"SS_Before_Cir")){
        uart_write_bytes(EX_UART_NUM, "SS_Before_Cir_OK", strlen("SS_Before_Cir_OK"));
    }
    else if(strstr(buf,"SS_OK_Cir")){
        uart_write_bytes(EX_UART_NUM, "SS_OK_Cir_OK", strlen("SS_OK_Cir_OK"));
    }
    else if(strstr(buf,"SS_Next_Cir")){
        uart_write_bytes(EX_UART_NUM, "SS_Next_Cir_OK", strlen("SS_Next_Cir_OK"));
    }
    else if(strstr(buf,"SS_More_Cir")){
        uart_write_bytes(EX_UART_NUM, "SS_More_Cir_OK", strlen("SS_More_Cir_OK"));
    }
    // End response
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

static const httpd_uri_t echo = {
    .uri       = "/sshandler",
    .method    = HTTP_POST,
    .handler   = echo_post_handler,
    .user_ctx  = NULL
};

static esp_err_t echo_handler(httpd_req_t *req)
{
    char buf[100];
    int data_len = req->content_len;

    /* Read the data for the request */
    httpd_req_recv(req, buf,data_len);
    /* Log data received */
    printf("Data:%s",buf);

    ESP_LOGI(TAG, "Data recv: %.*s", data_len, buf);
    if(strstr(buf,"AIR_ON")){
        uart_write_bytes(EX_UART_NUM, "AIR_ON_AR", strlen("AIR_ON_AR"));
    }
    else if(strstr(buf,"AIR_UP_VOL")){
        uart_write_bytes(EX_UART_NUM, "AIR_UP_TEMP_AR", strlen("AIR_UP_TEMP_AR"));
    }
    else if(strstr(buf,"AIR_DOWN_VOL")){
        uart_write_bytes(EX_UART_NUM, "AIR_DOWN_TEMP_AR", strlen("AIR_DOWN_TEMP_AR"));
    }
    // End response
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

static const httpd_uri_t echo_air = {
    .uri       = "/airhandler",
    .method    = HTTP_POST,
    .handler   = echo_handler,
    .user_ctx  = NULL
};


static esp_err_t pan_handler(httpd_req_t *req)
{
    char buf[100];
    int data_len = req->content_len;

    /* Read the data for the request */
    httpd_req_recv(req, buf,data_len);
    /* Log data received */
    printf("Data:%s",buf);

    ESP_LOGI(TAG, "Data recv: %.*s", data_len, buf);
    if(strstr(buf,"AIR_ON")){
        uart_write_bytes(EX_UART_NUM, "AIR_ON_AR", strlen("AIR_ON_AR"));
    }
    else if(strstr(buf,"AIR_UP_VOL")){
        uart_write_bytes(EX_UART_NUM, "AIR_UP_TEMP_AR", strlen("AIR_UP_TEMP_AR"));
    }
    else if(strstr(buf,"AIR_DOWN_VOL")){
        uart_write_bytes(EX_UART_NUM, "AIR_DOWN_TEMP_AR", strlen("AIR_DOWN_TEMP_AR"));
    }
    // End response
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

static const httpd_uri_t echo_pan = {
    .uri       = "/panss",
    .method    = HTTP_POST,
    .handler   = pan_handler,
    .user_ctx  = NULL
};

/* This handler allows the custom error handling functionality to be
 * tested from client side. For that, when a PUT request 0 is sent to
 * URI /ctrl, the /hello and /echo URIs are unregistered and following
 * custom error handler http_404_error_handler() is registered.
 * Afterwards, when /hello or /echo is requested, this custom error
 * handler is invoked which, after sending an error message to client,
 * either closes the underlying socket (when requested URI is /echo)
 * or keeps it open (when requested URI is /hello). This allows the
 * client to infer if the custom error handler is functioning as expected
 * by observing the socket state.
 */
esp_err_t http_404_error_handler(httpd_req_t *req, httpd_err_code_t err)
{
    if (strcmp("/hello", req->uri) == 0) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "/hello URI is not available");
        /* Return ESP_OK to keep underlying socket open */
        return ESP_OK;
    } else if (strcmp("/echo", req->uri) == 0) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "/echo URI is not available");
        /* Return ESP_FAIL to close underlying socket */
        return ESP_FAIL;
    }
    /* For any other URI send 404 and close socket */
    httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Some 404 error message");
    return ESP_FAIL;
}


void start_webserver(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;
    uart_start();
    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        // Set URI handlers
        ESP_LOGI(TAG, "Registering URI handlers");
        httpd_register_uri_handler(server, &hello);
        httpd_register_uri_handler(server, &samsung);
        httpd_register_uri_handler(server, &samsung_air);
        httpd_register_uri_handler(server, &pan);
        httpd_register_uri_handler(server, &echo_pan);
        httpd_register_uri_handler(server, &echo);
        httpd_register_uri_handler(server, &echo_air);
    }
    else{
        ESP_LOGI(TAG, "Error starting server!");
    }
}

void stop_webserver(void)
{
    // Stop the httpd server
    httpd_stop(server);
}