#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "driver/gpio.h"
#include "timeHour.h"

void HourTask(void *arg){
    hour_task_t * args = (hour_task_t *)arg;
    EventBits_t events;
    static int segundos = 0;
    static int min = 0;
    xQueueSend(args->minQueue, &min, portMAX_DELAY);
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xDelay = pdMS_TO_TICKS(10); // 1 segundo
  
while (1) {
            
        vTaskDelayUntil(&xLastWakeTime, xDelay);
         
        segundos++;
       
      
        events = xEventGroupWaitBits(args->event,args->reset, pdTRUE, pdFALSE, 0);
            if (events & args->reset) {
                 min = 0;
            }
        if (segundos >= 60) {
            //printf("%d\n",  min);
            segundos = 0;
            min++;

        xQueueSend(args->minQueue, &min, portMAX_DELAY);
        }
    }
        
} 

