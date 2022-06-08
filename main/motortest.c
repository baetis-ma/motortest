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
int pwm = 1000, pwmfreq = 50;
long timev = 0, lasttime = 0, deltatime, filter = 0;
int onoff = 1;
float currentlp  = 0;
float batteryvlp = 0;
float weightlp   = 0;
float weightcal  = 0;
float weight, current, batteryv;

//requirements for wifi
#include "sys/tcpsetup.h"
#include "sys/wifi-init.h"
#include "lwip/sockets.h"

//requirements for gpio
#include "driver/gpio.h"
#define GPIO_RPM_IN         10 // read rpm of motor with inter
#define GPIO_KX711_DOUT      4 // kx711 dout
#define GPIO_RELAY          23 // relay control
#define GPIO_KX711_SCK       0 // kx711 sck
#include "perif/gpio_setup.h"

//requirements for i2c
#include "driver/i2c.h"
#define i2c_gpio_scl  19
#define i2c_gpio_sda  18
#define i2c_port 0
#define i2c_frequency 500000
#include "perif/i2c.h"

//requirements for pwm
#include "esp_attr.h"
#include "driver/mcpwm.h"
#include "soc/mcpwm_reg.h"
#include "soc/mcpwm_struct.h"
#define GPIO_MCPWM0A   22      
#include "perif/pwm.h"
#include "perif/hx711.h"

void trfData( ) {    //called from tcpserver when GET/trfData?{string}
  //printf("trfData received from base %s\n", espRxData);
  //read control signals from client
  sscanf(espRxData, "pwm=%d+freq=%d+onoff=%d", &pwm, &pwmfreq, &onoff);
  if(onoff==1)filter=0;
  mcpwm_set_frequency(MCPWM_UNIT_0, MCPWM_TIMER_0, pwmfreq);
  mcpwm_set_duty_in_us(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, pwm);
  gpio_set_level(GPIO_RELAY, onoff);

  //output measurement data to client
  sprintf(espTxData, "data,%0.2f,%0.2f,%0.4f,%0.4f,%0.2f,\n", 
     //(weight - weightcal)/0.0028, 4.405 * batteryv - current, current/0.1,
     (weightlp - weightcal)/0.0028, 4.405 * batteryvlp - currentlp, currentlp/0.1,
     (float)(filter/1000000.0),(float)(1000000.0/filter));
}

float ads1115_read (int channel) {
  int temp;
  uint8_t temp_str[2];
  temp_str[0] = 0x40 + 16 * channel; 
  temp_str[1] = 0x80;
  i2c_write_block(0x49, 0x01, temp_str, 2); vTaskDelay(1);
  i2c_read(0x49, 0x00, temp_str, 2); vTaskDelay(1);
  //i2c_read(0x49, 0x00, temp_str, 2); vTaskDelay(1);
  temp = ((int) (256*temp_str[0] + temp_str[1]));
  if (temp > 0x7fff) temp = temp - 0x10000;
  //voltage[0] = (float)(6.144/(1ULL<<15)) * temp;
  return ( (float)(6.144/(1ULL<<15)) * temp);
}

void app_main()
{
    nvs_flash_init();
    initialize_wifi();
    wait_for_ip();
    i2c_init();
    i2cdetect();
    gpio_init();
    gpio_set_level(GPIO_RELAY, onoff); //relay off
    gpio_set_level(GPIO_KX711_SCK, 0); //hc711 sck low
    vTaskDelay(100 / portTICK_PERIOD_MS);
    //hx711_init();
    pwm_init();
    mcpwm_set_frequency(MCPWM_UNIT_0, MCPWM_TIMER_0, pwmfreq);
    mcpwm_set_duty_in_us(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, pwm);

    xTaskCreate(gpio_task, "gpio_task", 2048, NULL, 10, NULL);
    xTaskCreate(tcp_server_task, "tcp_server", 8192, NULL, 4, NULL);

    int cnt = 0;
    while(1) {
        if((cnt++)%5 == 4){
           weight = hx711_read();
	   if (weight/weightlp < 2 || (weight - weightlp) < 5) 
                weightlp   = 0.7 * weightlp  + 0.3 * weight;
        }
	current = ads1115_read(0);
        currentlp  = 0.9 * currentlp  + 0.1 * current;
	batteryv = ads1115_read(1);
        batteryvlp = 0.9 * batteryvlp + 0.1 * batteryv;
        if(onoff == 1) weightcal = weightlp;
        if(cnt%5 == 0)printf("%6d raw -->  w=%7.4f c=%7.4f b=%7.4f  ", cnt, weight, current, batteryv); 
        if(cnt%5 == 0)printf("thrust = %5.2fg current = %7.4fA battery = %7.4fV power = %7.4fW\n",
            (weightlp - weightcal)/0.00224, currentlp/0.1, 
            4.405 * batteryvlp - currentlp, 
            (4.405 * batteryvlp - currentlp) * currentlp/0.1); 

        vTaskDelay(100/portTICK_RATE_MS);  
        //vTaskDelay(333/portTICK_RATE_MS);  //wait 333msec
    }
}
