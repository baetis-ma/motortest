#include "esp_timer.h"
#include "driver/gpio.h"
#include "rom/ets_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

//void hx711_init() {
//    gpio_set_level(GPIO_KX711_SCK, 0);   
//    vTaskDelay(100 / portTICK_PERIOD_MS);
//}

float hx711_read() {
   float hx711volt;
   uint8_t data[4] = {0,0,0,0};
   while(gpio_get_level(GPIO_KX711_DOUT) == 1){
      printf("waiting\n");
      vTaskDelay(50/portTICK_RATE_MS);
   }
   for(int i = 0; i < 25; i++) {
      gpio_set_level(GPIO_KX711_SCK, 1);     
      ets_delay_us(10);
      if(gpio_get_level(GPIO_KX711_DOUT) == 1) data[i/8] |= (1 << (7-(i%8)));
      gpio_set_level(GPIO_KX711_SCK, 0);     
      ets_delay_us(10);      
   }
   printf("0x%02x%02x%02x%02x   ", data[0], data[1], data[2], data[3]);
   signed int volt = 256*256*256*data[0]+256*256*data[1]+256*data[2]+data[3];
   printf ("0x%08x   ", volt);
   hx711volt = 20*((float)volt)/(1ULL<<31);
   //printf("%9.4f mv\n", hx711volt);
   return (hx711volt);

}
