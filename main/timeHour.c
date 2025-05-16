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
    static int hr  = 0;
    hora_min_t time = { .horas = hr, .minutos = min };
     hora_min_t alarm = { .horas = hr, .minutos = min };
    xQueueSend(args->sendQueue, &min, portMAX_DELAY);
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xDelay = pdMS_TO_TICKS(10); // 1 segundo
  
while (1) {
            
        vTaskDelayUntil(&xLastWakeTime, xDelay);
         
        segundos++;
       if (segundos >= 60) {
            segundos = 0;
                min++;
            if (min >= 60) {
                min = 0;
                hr = (hr + 1) % 24;
            }
            // Prepara y envía la hora completa
            if(xQueueReceive(args->receiveQueue, &time, 0)){

                  hr = time.horas ;
                  min = time.minutos;
                   printf("Hora");
                   printf("%d",  hr);
                   printf("%d\n",  min);
            }
            xQueueReceive(args->receiveAlarmQueue, &alarm, 0);
            if(hr == alarm.horas && min == alarm.minutos && alarm.horas != 0 && alarm.minutos != 0) {
                xEventGroupSetBits(args->eventAlarm, args->start);
                printf("alarma\n");
            }    
                
                time.horas   = hr;
                time.minutos = min;
               
            xQueueSend(args->sendQueue, &time, portMAX_DELAY);
        }
    }
        
} 

