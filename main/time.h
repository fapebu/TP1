#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "driver/gpio.h"
#include "freertos/queue.h"


typedef struct time_task_t{
    EventGroupHandle_t event;
    uint8_t start;
    uint8_t reset;
    uint8_t parcial;
    uint8_t stop;
    uint8_t runing;
    QueueHandle_t lapsQueue;
    QueueHandle_t timeQueue;  
}time_task_t;

void TimeTask(void *arg);