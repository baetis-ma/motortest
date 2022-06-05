static xQueueHandle gpio_evt_queue = NULL;
static void gpio_isr_handler(void *arg)
{
    uint32_t gpio_num = (uint32_t) arg;
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

void gpio_init () {
    gpio_config_t io_conf;
    
    io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL<<GPIO_RELAY);
    io_conf.mode = GPIO_MODE_OUTPUT;    
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;    
    gpio_config(&io_conf);
    
    io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL<<GPIO_KX711_SCK);
    io_conf.mode = GPIO_MODE_OUTPUT;    
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;    
    gpio_config(&io_conf);
     
    io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_INPUT;   
    io_conf.pin_bit_mask = (1ULL<<GPIO_KX711_DOUT);
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);   


    io_conf.intr_type = GPIO_INTR_POSEDGE;
    io_conf.mode = GPIO_MODE_INPUT;   
    io_conf.pin_bit_mask = (1ULL << GPIO_RPM_IN);
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);
    
    gpio_install_isr_service(0);
    gpio_isr_handler_add(GPIO_RPM_IN, gpio_isr_handler, (void *) GPIO_RPM_IN);
    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));
}

static void gpio_task(void *arg)
{
    uint32_t io_num;
    for (;;) {
        if (xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY)) {
            timev = esp_timer_get_time();
            deltatime = timev - lasttime;
            if(deltatime>0.1)filter = (deltatime/10) + (9*filter/10);
            //printf( " %7.3f :  GPIO[%d] rising intr val=%d   pulse width = %7.6f   %7.6f\n", 
            //          (float)(timev/1000000.0), io_num, gpio_get_level(io_num), 
            //          (float)(deltatime/1000000.0), (float)(timev/1000000.0));
            lasttime = timev;
        }
    }
}

