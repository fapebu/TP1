#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "driver/gpio.h"
#include "time.h"

void TimeTask(void *arg){
    bool runing = false;
    uint32_t elapsedTime = 0;
    time_task_t * args = (time_task_t *)arg;
    EventBits_t events;
    TickType_t last_counter;
while (1) {
     while (!runing)
     {
        events = xEventGroupWaitBits(args->event, args->start|args->reset, pdTRUE, pdFALSE, portMAX_DELAY);
        if (events & args->reset) {
            elapsedTime = 0;
            if(args->timeQueue != NULL) {
                xQueueSend(args->timeQueue, &elapsedTime, 30);
            }
        }
        if (events & args->start) {
            runing = true;
        }
     }
        //elapsedTime = 0;
        last_counter = xTaskGetTickCount();
        xEventGroupClearBits(args->event,args->stop|args->parcial);
        while (runing)
        {
         
         vTaskDelayUntil(&last_counter, pdMS_TO_TICKS(100));
         elapsedTime += 100;
         if(args->timeQueue != NULL) {
            xQueueSend(args->timeQueue, &elapsedTime, 30);
            }
         xEventGroupSetBits(args->event,args->runing);
         events = xEventGroupWaitBits(args->event, args->stop|args->parcial, pdTRUE, pdFALSE, 0);
            if (events & args->stop) {
                runing = false;
            }
            if (events & args->parcial) {
                printf("parcial\n");
                xQueueSend(args->lapsQueue, &elapsedTime, 30);
            } 
        
        }
    
}
}
