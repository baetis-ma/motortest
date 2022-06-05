#include <string.h>
#include <stdio.h>
#include <sys/param.h>
#include <sys/time.h>
#include <unistd.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "driver/spi_common.h"
#include <esp_timer.h>
#define CPU_FREQ    240

//sys global
char espTxData[500], espRxData[500];
int  voltt=0, pwm = 1000, pwmfreq = 50;
long timev = 0, lasttime = 0, deltatime, filter = 0;

//requirements for wifi
#include "sys/tcpsetup.c"
#include "sys/wifi-init.h"
#include "lwip/sockets.h"

//requirements for gpio
#include "driver/gpio.h"
#define GPIO_RPM_IN         10 // read rpm of motor with inter
#define GPIO_KX711_DOUT      4 // kx711 dout
#define GPIO_RELAY          23 // relay control
#define GPIO_KX711_SCK       0 // kx711 sck
#include "perif/gpio_setup.c"

//requirements for i2c
#include "driver/i2c.h"
#define i2c_gpio_scl  19
#define i2c_gpio_sda  18
#define i2c_port 0
#define i2c_frequency 100000
#include "perif/i2c.c"

//requirements for pwm
#include "esp_attr.h"
#include "driver/mcpwm.h"
#include "soc/mcpwm_reg.h"
#include "soc/mcpwm_struct.h"
#define GPIO_MCPWM0A   22      
#include "perif/pwm.h"
#include "perif/hx711.c"

void trfData();

float voltage[4];
uint8_t temp_str[4];
int onoff = 1;

float ads1115_read (int channel) {
  int temp;
  temp_str[0] = 0x40 + 16 * channel; 
  temp_str[1] = 0x80;
  i2c_write_block(0x49, 0x01, temp_str, 2); vTaskDelay(2);
  i2c_read(0x49, 0x00, temp_str, 2); vTaskDelay(2);
  i2c_read(0x49, 0x00, temp_str, 2); vTaskDelay(2);
  temp = ((int) (256*temp_str[0] + temp_str[1]));
  if (temp > 0x7fff) temp = temp - 0x10000;
  voltage[0] = (float)(6.144/(1ULL<<15)) * temp;
  return ( (float)(6.144/(1ULL<<15)) * temp);
}

void app_main()
{
    float currentlp  = 0;
    float batteryvlp = 0;
    float weightlp   = 0;
    float weightcal  = 0;
    nvs_flash_init();
    initialize_wifi();
    wait_for_ip();
    i2c_init();
    i2cdetect();
    gpio_init();
    gpio_set_level(GPIO_RELAY, 1); //relay off
    hx711_init();

    pwm_init();
    mcpwm_set_frequency(MCPWM_UNIT_0, MCPWM_TIMER_0, pwmfreq);
    mcpwm_set_duty_in_us(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, pwm);

    xTaskCreate(gpio_task, "gpio_task", 2048, NULL, 10, NULL);
    xTaskCreate(tcp_server_task, "tcp_server", 8192, NULL, 4, NULL);

    //uint64_t starttime = esp_timer_get_time();
 
    while(1) {
        weightlp   = 0.7 * weightlp  + 0.3 * hx711_read();
        //currentlp  = 0.9 * currentlp  + 0.1 * ads1115_read(1);
        //batteryvlp = 0.9 * batteryvlp + 0.1 * ads1115_read(0);
        //if(onoff == 1) weightcal = weightlp;
        weightcal = 1.4064;
       
        printf("thrust = %7.2fg current = %7.4fA battery = %7.4fV power = %7.4fW\n",
            (weightlp - weightcal)/0.0028, currentlp/0.1, 
            4.405 * batteryvlp, 4.405 * batteryvlp * currentlp/0.1); 

        vTaskDelay(20);
    }
}

int temp;
void trfData( ) {    //called from tcpserver when GET/trfData?{string}
  printf("trfData received from base %s\n", espRxData);
  sscanf(espRxData, "vout=%x+pwm=%d+freq=%d+onoff=%d", &voltt, &pwm, &pwmfreq, &onoff);
  if(onoff==1)filter=0;
  mcpwm_set_frequency(MCPWM_UNIT_0, MCPWM_TIMER_0, pwmfreq);
  mcpwm_set_duty_in_us(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, pwm);
  gpio_set_level(GPIO_RELAY, onoff);

  temp_str[0] = 0x40; temp_str[1] = 0x80;
  i2c_write_block(0x49, 0x01, temp_str, 2); vTaskDelay(2);
  i2c_read(0x49, 0x00, temp_str, 2); vTaskDelay(2);
  i2c_read(0x49, 0x00, temp_str, 2); vTaskDelay(2);
  temp = ((int) (256*temp_str[0] + temp_str[1]));
  if (temp > 0x7fff) temp = temp - 0x10000;
  voltage[0] = (float)(6.144/(1ULL<<15)) * temp;

  temp_str[0] = 0x50; temp_str[1] = 0x80;
  i2c_write_block(0x49, 0x01, temp_str, 2); vTaskDelay(2);
  i2c_read(0x49, 0x00, temp_str, 2); vTaskDelay(2);
  i2c_read(0x49, 0x00, temp_str, 2); vTaskDelay(2);
  temp = ((int) (256*temp_str[0] + temp_str[1]));
  if (temp > 0x7fff) temp = temp - 0x10000;
  voltage[1] = (float)(6.144/(1ULL<<15)) * temp;

  temp_str[0] = 0x60; temp_str[1] = 0x80;
  i2c_write_block(0x49, 0x01, temp_str, 2); vTaskDelay(2);
  i2c_read(0x49, 0x00, temp_str, 2); vTaskDelay(2);
  i2c_read(0x49, 0x00, temp_str, 2); vTaskDelay(2);
  temp = ((int) (256*temp_str[0] + temp_str[1]));
  if (temp > 0x7fff) temp = temp - 0x10000;
  voltage[2] = (float)(6.144/(1ULL<<15)) * temp;

  temp_str[0] = 0x70; temp_str[1] = 0x80;
  i2c_write_block(0x49, 0x01, temp_str, 2); vTaskDelay(2);
  i2c_read(0x49, 0x00, temp_str, 2); vTaskDelay(2);
  i2c_read(0x49, 0x00, temp_str, 2); vTaskDelay(2);
  temp = ((int) (256*temp_str[0] + temp_str[1]));
  if (temp > 0x7fff) temp = temp - 0x10000;
  voltage[3] = (float)(6.144/(1ULL<<15)) * temp;

  sprintf(espTxData, "data,%0.4f,%0.4f,%0.4f,%0.4f,%0.6f,%0.2f,\n", 
       voltage[0], voltage[1], voltage[2], voltage[3],
           (float)(filter/1000000.0),(float)(1000000.0/filter));
  printf("voltage = %7.4f %7.4f %7.4f %7.4f  rps= %7.6f  pwm= %5.2f throttle= %5d\n", 
       voltage[0], voltage[1], voltage[2], voltage[3],
           (float)(filter/1000000.0),(float)(1000000.0/filter),pwm);
}
