//this lower level source file contains following functions:
//  1) initialize_wifi - sets up connection to locl wifi network
//  2) event_handler   - sets up wifi connection event handler
//  3) wait_for_ip     - waits for wifi connection
//  4) tcp_server_task - sets up loop to listen for and handle http requests from client
//                       current configured for three types of payload headers
//                       -- GET /index.html or GET / returns embedded index.html file
//                       -- GET /(not index.html or blank) returns 404 page
//                       -- Get /GetData runs function in top level source to return something like data
#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>
#include "sdkconfig.h"

//local wifi credentials and port number to listen on
#define EXAMPLE_WIFI_SSID "troutstream"
#define EXAMPLE_WIFI_PASS "password"
#define PORT 80

//need to forward define funtion in top level source
char rx_packet[1000], espTxData[500], espRxData[500];
void trfData( );

/* FreeRTOS event group to signal when we are connected & ready to make a request */
static EventGroupHandle_t wifi_event_group;
const int IPV4_GOTIP_BIT = BIT0;
static const char *TAG = "webpage ";
static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    switch (event->event_id) {
    case SYSTEM_EVENT_STA_START:
        esp_wifi_connect();
        ESP_LOGI(TAG, "SYSTEM_EVENT_STA_START");
        break;
    case SYSTEM_EVENT_STA_CONNECTED:
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        xEventGroupSetBits(wifi_event_group, IPV4_GOTIP_BIT);
        ESP_LOGI(TAG, "SYSTEM_EVENT_STA_GOT_IP");
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        /* This is a workaround as ESP32 WiFi libs don't currently auto-reassociate. */
        esp_wifi_connect();
        xEventGroupClearBits(wifi_event_group, IPV4_GOTIP_BIT);
        break;
    default:
        break;
    }
    return ESP_OK;
}

void initialize_wifi(void)
{
    tcpip_adapter_init();
    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK( esp_event_loop_init(event_handler, NULL) );
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = EXAMPLE_WIFI_SSID,
            .password = EXAMPLE_WIFI_PASS,
        },
    };
    ESP_LOGI(TAG, "Setting WiFi configuration SSID %s...", wifi_config.sta.ssid);
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK( esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
    ESP_ERROR_CHECK( esp_wifi_start() );
}

void wait_for_ip()
{
    uint32_t bits = IPV4_GOTIP_BIT;
    //ESP_LOGI(TAG, "Waiting for AP connection...");
    xEventGroupWaitBits(wifi_event_group, bits, false, true, portMAX_DELAY);
    ESP_LOGI(TAG, "Connected to AP");
}

void tcp_server_task(void *pvParameters)
{
    int    addr_family;
    int    ip_protocol;
    char   addr_str[20];
    struct sockaddr_in destAddr;
    struct sockaddr_in sourceAddr;
    int    listen_sock;

    destAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    destAddr.sin_family = AF_INET;
    destAddr.sin_port = htons(PORT);
    addr_family = AF_INET;
    ip_protocol = IPPROTO_IP;
    inet_ntoa_r(destAddr.sin_addr, addr_str, sizeof(addr_str) - 1);

    listen_sock = socket(addr_family, SOCK_STREAM, ip_protocol);
    bind(listen_sock, (struct sockaddr *)&destAddr, sizeof(destAddr));
    listen(listen_sock, 1);

    unsigned int addrLen = sizeof(sourceAddr);
    int    sock, len, temp;
    char   http_type[10], url_name[16], temp_str[200];
    while (1) {
        //wait for packet
        while ((sock = accept(listen_sock, (struct sockaddr *)&sourceAddr, &addrLen)) < 0) ;
        ESP_LOGI(TAG, "Socket accepted");
        len = recv(sock, rx_packet, sizeof(rx_packet) - 1, 0);
        rx_packet[len] = 0; 
        printf("     RCV PACKET\n%s\n", rx_packet);

        temp = strstr (rx_packet, "HTTP/") - rx_packet;
        rx_packet[temp] = '\0';
        sscanf(rx_packet, "%s /%s", http_type, url_name);

        //printf("type=%s name=%s\n", http_type, url_name);
        if(strstr(url_name, "?")) {
           temp = (strchr(url_name, '?') - url_name);
           url_name[temp] = '\0';
           strcpy (espRxData, url_name + temp + 1 );
           if (strcmp("trfData", url_name)==0) {
              trfData(); 
              send(sock, espTxData, 80, 0);
              //send(sock, espTxData, sizeof(espTxData), 0);
           }
        } 

        if (strcmp("index.html", url_name) ==0 || strcmp("HTTP/1.1",url_name) ==0 ){
           extern const char index_start[] asm("_binary_index_html_start");
           extern const char index_end[] asm("_binary_index_html_end");
           int pkt_buf_size = 1500;
           int pkt_end = pkt_buf_size;
           int ro_len =  strlen(index_start) - strlen(index_end);
           for( int pkt_ptr = 0; pkt_ptr < ro_len; pkt_ptr = pkt_ptr + pkt_buf_size){
               if ((ro_len - pkt_ptr) < pkt_buf_size) pkt_end = ro_len - pkt_ptr;
                   //ESP_LOGI(TAG, "pkt_ptr %d pkt_end %d", pkt_ptr,pkt_end );
                   send(sock, index_start + pkt_ptr, pkt_end, 0);
           }
        }
        
        strcpy(url_name, " ");
        vTaskDelay(10/ portTICK_RATE_MS); //waits for buffer to purge
        shutdown(sock, 0);
        close(sock);
        }
    vTaskDelete(NULL);
}
