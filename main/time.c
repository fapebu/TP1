#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "driver/gpio.h"
#include "time.h"

void TimeTask(void *arg){
    bool runing = false;
    uint32_t elapsedTime = 0;
    time_task_t args = arg;
    EventBits_t events;
    TickType_t last_counter;
while (1) {
     while (!runing)
     {
        events = xEventGroupWaitBits(args->event, args->start, pdTRUE, pdFALSE, portMAX_DELAY);
        if (events & args->reset) {
            elapsedTime = 0;
        }
        if (events & args->start) {
            runing = true;
        }
     }
        elapsedTime = 0;
        last_counter = xTaskGetTickCount();
        xEventGroupClearBits(args->event,args->stop|args->parcial);
        while (runing)
        {
         vTaskDelayUntil(&last_counter, pdMS_TO_TICKS(100));
         elapsedTime += 100;
         
         events = xEventGroupWaitBits(args->event, args->stop|args->parcial, pdTRUE, pdFALSE, 0);
            if (events & args->stop) {
                runing = false;
            }
            if (events & args->parcial) {
                printf("Parcial: %lu\n", elapsedTime);
            } 
        
        }
    
}
}
/*void baseTime (void * argumentos){
    TickType_t ultimo = xTaskGetTickCount();
    while (1) {
        xSemaphoreTake(flags_mutex, portMAX_DELAY);
          bool run = flags.running;
        xSemaphoreGive(flags_mutex);
        
        vTaskDelayUntil(&ultimo, pdMS_TO_TICKS(100));
        if (run) {
            xSemaphoreTake(cuenta_mutex, pdMS_TO_TICKS(50));
              cuenta += 100;
              
            xSemaphoreGive(cuenta_mutex);
        }
    }
    
}*/