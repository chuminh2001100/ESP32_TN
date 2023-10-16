#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_netif.h"
#include "esp_netif_ppp.h"
#include "mqtt_client.h"
#include "esp_modem.h"
#include "esp_modem_netif.h"
#include "esp_log.h"
#include "sim800.h"
#include "bg96.h"
#include "sim7672.h"
#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"
#include "esp_bit_defs.h"

// #define mqtt_broker "mqtt://demo.thingsboard.io:1883"
// #define mqtt_broker         "mqtt://mqtt.innoway.vn:1883"
#define mqtt_broker         "mqtt://test.mosquitto.org:1883"

static const char *TAG = "pppos_example";
static EventGroupHandle_t event_group = NULL;
#define CONNECT_BIT     BIT0
#define STOP_BIT        BIT1
#define GOT_DATA_BIT    BIT2
int count_login = 0;

char status_topic[] = "v1/devices/me/telemetry_CV";
char ota_topic[]    =  "v1/devices/me/ota_CV";
char ota_res_topic[] = "v1/devices/me/fota_CV";

// char status_topic[] = "messages/ca18c634-a035-48b5-96f4-b64748203f97/attributtes";
// char ota_res_topic[] = "messages/ca18c634-a035-48b5-96f4-b64748203f97/fota";
// char ota_topic[] = "messages/ca18c634-a035-48b5-96f4-b64748203f97/ota";

char tmp[100];
char url_ota[200] = {0};
char version_firm[50] = {0};
char res_ota[100] = {0};
extern const uint8_t server_cert_pem_start[] asm("_binary_ca_cert_pem_start");
extern const uint8_t server_cert_pem_end[] asm("_binary_ca_cert_pem_end");

#define SIM_POWER_TX_PIN            11
#define SIM_POWER_RX_PIN            12
#define PORT                        8964
#define HOST_IP_ADDR                "116.101.122.190"

char payload_login[] = "24240000140026092322284786418005181236605648542e312e303113dd014dc200d66c2323";
char payload_data[] = "2424ff003c00260923222847210826401055688618060a020400000000006c05000000000020030f9800011522000000000000000000000000000000000000000000000000000000240048e72323";

TaskHandle_t MQTT_TASK;// Handle of normal mqtt task, mqtt task can be suspene
esp_mqtt_client_handle_t mqtt_client;
TaskHandle_t HTTP_TASK;



esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id) {
    case HTTP_EVENT_ERROR:
        ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
        break;
    case HTTP_EVENT_ON_CONNECTED:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
        break;
    case HTTP_EVENT_HEADER_SENT:
        ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
        break;
    case HTTP_EVENT_ON_HEADER:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
        break;
    case HTTP_EVENT_ON_DATA:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
        break;
    case HTTP_EVENT_ON_FINISH:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
        break;
    case HTTP_EVENT_DISCONNECTED:
        ESP_LOGD(TAG, "HTTP_EVENT_DISCONNECTED");
        break;
    }
    return ESP_OK;
}

void simple_ota_example_task(void *pvParameter)
{
    ESP_LOGI(TAG, "Starting OTA example");

    esp_http_client_config_t config = {
        .url = url_ota,
        .cert_pem = (char *)server_cert_pem_start,
        .event_handler = _http_event_handler,
        .keep_alive_enable = true,
    };
    config.skip_cert_common_name_check = true;
    esp_err_t ret = esp_https_ota(&config);
    if (ret == ESP_OK) {
        /*after wrote successfully new firmware in OTA parition, response to server version of firmware will be updated*/
        sprintf(res_ota,"{\"version\": \"%s\",\r\n\"status\":\"Success\"\n}",version_firm);
        esp_mqtt_client_publish(mqtt_client, ota_res_topic, res_ota, 0, 0, 1);
        vTaskDelay(100 / portTICK_PERIOD_MS);
        /*After esp_restart(), program will run with new firmware*/
        esp_restart();
    } 
    else 
    {   
        /*after wrote fail new firmware in OTA parition, response to server to announce fail update new firmware*/
        sprintf(res_ota,"{\"version\": \"%s\",\r\n\"status\":\"Fail\"\n}",version_firm);
        esp_mqtt_client_publish(mqtt_client, ota_res_topic, res_ota, 0, 0, 1);
        ESP_LOGE(TAG, "Firmware upgrade failed");
    }
    while (1) {
        /*print to check log, if see log below that Task OTA failed*/
        printf("Task in OTA\r\n");
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}


int HexDigit(char c)
{
  if (c >= '0' && c <= '9')
    return c - '0';

  if (c >= 'A' && c <= 'F')
    return c - 'A' + 10;

  if (c >= 'a' && c <= 'f')
    return c - 'a' + 10;

  return 0;
}

char HexByte(char *p)
{
  char value = 0;

  value += HexDigit(*p++);

  if (*p != '\0')
  {
    value = value * 16 +  HexDigit(*p);
  }
  return value;
}

void hextoString_byme(char *hex, char *result){
    memset(result,0,strlen(result));
	int i = 0;
	int hexLength = strlen(hex);
	if (hexLength % 2 != 0) {
        printf("Invalid hex string length\n");
        return;
    }
    
    for (i = 0; i < hexLength; i += 2){
    	*(result + i/2) = HexByte(&hex[i]);
	}	
}

int filter_comma(char *respond_data, int begin, int end, char *output, char exChar)
{
	memset(output, 0, strlen(output));
	int count_filter = 0, lim = 0, start = 0, finish = 0,i;
	for (i = 0; i < strlen(respond_data); i++)
	{
		if ( respond_data[i] == exChar)
		{
			count_filter ++;
			if (count_filter == begin)			start = i+1;
			if (count_filter == end)			finish = i;
		}

	}
	if(count_filter < end)
	{
	    finish = strlen(respond_data);
	}
	lim = finish - start;
	for (i = 0; i < lim; i++){
		output[i] = respond_data[start];
		start ++;
	}
	output[i] = 0;
	return 0;
}

/*Handle message OTA from server, get URL link to load file new firmware*/
bool process_ota(char *respond_data, char *url_data, char *version){
	char *a = NULL;
    char *b = NULL;
	memset(url_data,0,strlen(url_data));
    memset(version,0,strlen(version));
	// memcpy(url_data,"http://api.innoway.vn",strlen("http://api.innoway.vn"));
	char tmp2[200] = {0};
    char tmp3[100] = {0};
    b = strstr(respond_data,"\"version");
    if(b != NULL)
    {
        filter_comma(b,3,4,tmp3,'\"');
        strcat(version,tmp3);
        printf("\nVersion: \r\n%s",version);
    }
	a = strstr(respond_data,"\"url_api");
    if(a != NULL)
    {
        filter_comma(a,3,4,tmp2,'\"');
        printf("DATA: \r\n%s",tmp2);
        strcat(url_data,tmp2);
        printf("\nURL: \r\n%s",url_data);
        return true;
    }
    return false;
}


static void tcp_client_task(void *pvParameters)
{
    char rx_buffer[128];
    char buffer_login[100];
    char host_ip[] = HOST_IP_ADDR;
    int addr_family = 0;
    int ip_protocol = 0;

    while (1) {
        printf("Bat dau ket noi TCP\r\n");
        struct sockaddr_in dest_addr;
        dest_addr.sin_addr.s_addr = inet_addr(host_ip);
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port = htons(PORT);
        addr_family = AF_INET;
        ip_protocol = IPPROTO_IP;

        int sock =  socket(addr_family, SOCK_STREAM, ip_protocol);
        if (sock < 0) {
            ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
            break;
        }
        ESP_LOGI(TAG, "Socket created, connecting to %s:%d", host_ip, PORT);

        int err = connect(sock, (struct sockaddr *)&dest_addr, sizeof(struct sockaddr_in6));
        if (err != 0) {
            ESP_LOGE(TAG, "Socket unable to connect: errno %d", errno);
            break;
        }
        ESP_LOGI(TAG, "Successfully connected");

        while (1) {
            if(count_login == 0){
                hextoString_byme(payload_login,buffer_login);
                int err = send(sock, buffer_login, strlen(payload_login), 0);
                if (err < 0) {
                    ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
                    break;
                }
                vTaskDelay(10 / portTICK_PERIOD_MS);
                int len = recv(sock, rx_buffer, sizeof(rx_buffer) - 1, 0);
                // Error occurred during receiving
                if (len < 0) {
                    ESP_LOGE(TAG, "recv failed: errno %d", errno);
                    break;
                }
                // Data received
                else {
                    rx_buffer[len] = 0; // Null-terminate whatever we received and treat like a string
                    ESP_LOGI(TAG, "Received %d bytes from %s:", len, host_ip);
                    ESP_LOGI(TAG, "%s", rx_buffer);
                    count_login = 1;
                }
            }
            else{
                printf("Task send data position\r\n");
                hextoString_byme(payload_data,buffer_login);
                int err = send(sock, buffer_login, strlen(payload_login), 0);
                if (err < 0) {
                    ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
                    break;
                }
            }
            vTaskDelay(2000 / portTICK_PERIOD_MS);
        }

        if (sock != -1) {
            ESP_LOGE(TAG, "Shutting down socket and restarting...");
            shutdown(sock, 0);
            close(sock);
        }
    }
    vTaskDelete(NULL);
}

static void modem_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    switch (event_id) {
    case ESP_MODEM_EVENT_PPP_START:
        ESP_LOGI(TAG, "Modem PPP Started");
        break;
    case ESP_MODEM_EVENT_PPP_STOP:
        ESP_LOGI(TAG, "Modem PPP Stopped");
        xEventGroupSetBits(event_group, STOP_BIT);
        break;
    case ESP_MODEM_EVENT_UNKNOWN:
        ESP_LOGW(TAG, "Unknow line received: %s", (char *)event_data);
        break;
    default:
        break;
    }
}

static esp_err_t mqtt_event_handler(esp_mqtt_event_handle_t event)
{
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    int length = 0;
    char topic[150];
    switch (event->event_id) {
    case MQTT_EVENT_CONNECTED:
        vTaskResume(MQTT_TASK);
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        msg_id = esp_mqtt_client_subscribe(client, ota_topic, 0);
        ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);
        break;
    case MQTT_EVENT_DISCONNECTED:

        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        break;
    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
            printf("TOPIC=%.*s\r\n",event->topic_len,event->topic);
            printf("DATA=%.*s\r\n", event->data_len,event->data);
            length = event->data_len;
            printf("Length of content: %d\r\n",length);
            memcpy(tmp,event->data,length);
            tmp[event->data_len] = '\0';
            printf("Data in event: %.*s",strlen(tmp),tmp);
            memset(topic,0,strlen(topic));
            memcpy(topic, event->topic, event->topic_len);
            topic[strlen(topic) - 1] = '\0';
            printf("Topic me: %s\r\n",topic);
            if(strstr(topic,ota_topic)){
                printf("Receive topic requirement OTA\r\n");
                if(process_ota(tmp,url_ota,version_firm) == true)
                {
                    vTaskSuspend(MQTT_TASK);
                    xTaskCreate(&simple_ota_example_task, "ota_example_task", 8192, NULL, 3, HTTP_TASK);
                }
            }
            ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        break;
    default:
        ESP_LOGI(TAG, "MQTT other event id: %d", event->event_id);
        break;
    }
    return ESP_OK;
}

static void on_ppp_changed(void *arg, esp_event_base_t event_base,
                        int32_t event_id, void *event_data)
{
    ESP_LOGI(TAG, "PPP state changed event %d", event_id);
    if (event_id == NETIF_PPP_ERRORUSER) {
        /* User interrupted event from esp-netif */
        esp_netif_t *netif = *(esp_netif_t**)event_data;
        ESP_LOGI(TAG, "User interrupted event from netif:%p", netif);
    }
}


static void on_ip_event(void *arg, esp_event_base_t event_base,
                      int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "IP event! %d", event_id);
    if (event_id == IP_EVENT_PPP_GOT_IP) {
        esp_netif_dns_info_t dns_info;

        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        esp_netif_t *netif = event->esp_netif;

        ESP_LOGI(TAG, "Modem Connect to PPP Server");
        ESP_LOGI(TAG, "~~~~~~~~~~~~~~");
        ESP_LOGI(TAG, "IP          : " IPSTR, IP2STR(&event->ip_info.ip));
        ESP_LOGI(TAG, "Netmask     : " IPSTR, IP2STR(&event->ip_info.netmask));
        ESP_LOGI(TAG, "Gateway     : " IPSTR, IP2STR(&event->ip_info.gw));
        esp_netif_get_dns_info(netif, 0, &dns_info);
        ESP_LOGI(TAG, "Name Server1: " IPSTR, IP2STR(&dns_info.ip.u_addr.ip4));
        esp_netif_get_dns_info(netif, 1, &dns_info);
        ESP_LOGI(TAG, "Name Server2: " IPSTR, IP2STR(&dns_info.ip.u_addr.ip4));
        ESP_LOGI(TAG, "~~~~~~~~~~~~~~");
        xEventGroupSetBits(event_group, CONNECT_BIT);

        ESP_LOGI(TAG, "GOT ip event!!!");
    } else if (event_id == IP_EVENT_PPP_LOST_IP) {
        ESP_LOGI(TAG, "Modem Disconnect from PPP Server");
    } else if (event_id == IP_EVENT_GOT_IP6) {
        ESP_LOGI(TAG, "GOT IPv6 event!");

        ip_event_got_ip6_t *event = (ip_event_got_ip6_t *)event_data;
        ESP_LOGI(TAG, "Got IPv6 address " IPV6STR, IPV62STR(event->ip6_info.ip));
    }
}


void mqtt_task(void *pvParameter){
    char tmp[100];
    while(1){
        sprintf(tmp,"{\"led\": %d,\"status\": \"%d\"}", 2, 8);
        int msg_id = esp_mqtt_client_publish(mqtt_client, status_topic, tmp, 0, 0, 1);
        ESP_LOGI(TAG, "Sent publish successful, msg_id=%d", msg_id);
        vTaskDelay(1000/portTICK_PERIOD_MS);
    }
}

void app_main(void)
{
#if CONFIG_LWIP_PPP_PAP_SUPPORT
    esp_netif_auth_type_t auth_type = NETIF_PPP_AUTHTYPE_PAP;
#elif CONFIG_LWIP_PPP_CHAP_SUPPORT
    esp_netif_auth_type_t auth_type = NETIF_PPP_AUTHTYPE_CHAP;
#elif !defined(CONFIG_EXAMPLE_MODEM_PPP_AUTH_NONE)
#error "Unsupported AUTH Negotiation"
#endif
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, &on_ip_event, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(NETIF_PPP_STATUS, ESP_EVENT_ANY_ID, &on_ppp_changed, NULL));

    event_group = xEventGroupCreate();

    // Init netif object
    esp_netif_config_t cfg = ESP_NETIF_DEFAULT_PPP();
    esp_netif_t *esp_netif = esp_netif_new(&cfg);
    assert(esp_netif);

    /* create dte object */
    esp_modem_dte_config_t config = ESP_MODEM_DTE_DEFAULT_CONFIG();
    /* setup UART specific configuration based on kconfig options */
    config.tx_io_num = 17;
    config.rx_io_num = 16;
    config.event_task_stack_size = CONFIG_EXAMPLE_MODEM_UART_EVENT_TASK_STACK_SIZE;
    modem_dte_t *dte = esp_modem_dte_init(&config);
    /* Register event handler */
    ESP_ERROR_CHECK(esp_modem_set_event_handler(dte, modem_event_handler, ESP_EVENT_ANY_ID, NULL));
    /* create dce object */
    modem_dce_t *dce = sim7600_init(dte);
    assert(dce != NULL);
    ESP_ERROR_CHECK(dce->set_flow_ctrl(dce, MODEM_FLOW_CONTROL_NONE));
    ESP_ERROR_CHECK(dce->store_profile(dce));
    /* Print Module ID, Operator, IMEI, IMSI */
    ESP_LOGI(TAG, "Module: %s", dce->name);
    ESP_LOGI(TAG, "Operator: %s", dce->oper);
    ESP_LOGI(TAG, "IMEI: %s", dce->imei);
    ESP_LOGI(TAG, "IMSI: %s", dce->imsi);
    /* Get signal quality */
    uint32_t rssi = 0, ber = 0;
    ESP_ERROR_CHECK(dce->get_signal_quality(dce, &rssi, &ber));
    ESP_LOGI(TAG, "rssi: %d, ber: %d", rssi, ber);
    /* Get battery voltage */
    uint32_t voltage = 0, bcs = 0, bcl = 0;
    ESP_ERROR_CHECK(dce->get_battery_status(dce, &bcs, &bcl, &voltage));
    ESP_LOGI(TAG, "Battery voltage: %d mV", voltage);
    /* setup PPPoS network parameters */
#if !defined(CONFIG_EXAMPLE_MODEM_PPP_AUTH_NONE) && (defined(CONFIG_LWIP_PPP_PAP_SUPPORT) || defined(CONFIG_LWIP_PPP_CHAP_SUPPORT))
    esp_netif_ppp_set_auth(esp_netif, auth_type, CONFIG_EXAMPLE_MODEM_PPP_AUTH_USERNAME, CONFIG_EXAMPLE_MODEM_PPP_AUTH_PASSWORD);
#endif
    void *modem_netif_adapter = esp_modem_netif_setup(dte);
    esp_modem_netif_set_default_handlers(modem_netif_adapter, esp_netif);
    /* attach the modem to the network interface */
    esp_netif_attach(esp_netif, modem_netif_adapter);
    /* Wait for IP address */
    xEventGroupWaitBits(event_group, CONNECT_BIT, pdTRUE, pdTRUE, portMAX_DELAY);
    /* Config MQTT */
    esp_mqtt_client_config_t mqtt_config = {
        .uri = mqtt_broker,
        .password = NULL,
        .username = NULL,
        .event_handle = mqtt_event_handler,
    };
    mqtt_client = esp_mqtt_client_init(&mqtt_config);
    esp_mqtt_client_start(mqtt_client);
    xTaskCreate(&mqtt_task, "mqtt_test_task", 2048, NULL, 3, &MQTT_TASK);
    vTaskSuspend(MQTT_TASK);
    xTaskCreate(tcp_client_task, "tcp_client", 4096, NULL, 5, NULL);
    while(1)
    {
        sprintf(tmp,"{\"Temperature\": %d}", 28);
        printf("API TEST\r\n");
        int msg_id = esp_mqtt_client_publish(mqtt_client, status_topic, tmp, 0, 0, 1);
        ESP_LOGI(TAG, "Sent publish successful, msg_id=%d", msg_id);
        vTaskDelay(200);
    }
}
