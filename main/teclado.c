#include "teclado.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "driver/gpio.h"


void tareaTeclado(void *arg){
    key_task_t *key = (key_task_t *)arg;
    uint32_t eventBits = key->eventBits;
    gpio_num_t gpio = key->gpio;
    EventGroupHandle_t eventGroup = key->eventGroup;
    
    while(1){
        while(gpio_get_level(gpio) != 0){
            
            vTaskDelay(pdMS_TO_TICKS(pdMS_TO_TICKS(150)));
            
        }
        xEventGroupSetBits(eventGroup, eventBits);
        
        while(gpio_get_level(gpio) != 1){
            
            vTaskDelay(pdMS_TO_TICKS(pdMS_TO_TICKS(150)));
        }
    }
}