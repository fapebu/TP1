#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "driver/gpio.h"
#include "freertos/queue.h"


typedef struct hour_task_t{
   
    QueueHandle_t minQueue;
    EventGroupHandle_t event;
    uint8_t reset;
    
}hour_task_t;

void HourTask(void *arg);