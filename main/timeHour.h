#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "driver/gpio.h"
#include "freertos/queue.h"


typedef struct hour_task_t{
   
    QueueHandle_t sendQueue;
    QueueHandle_t receiveQueue;
    QueueHandle_t receiveAlarmQueue;
    EventGroupHandle_t eventTime;
    EventGroupHandle_t eventAlarm;
    uint8_t start;
    uint8_t reset;
    uint8_t posponer;
    
}hour_task_t;

typedef struct {
    int horas;
    int minutos;
} hora_min_t;

void HourTask(void *arg);