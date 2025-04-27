#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "driver/gpio.h"

typedef struct key_task_t{
    EventGroupHandle_t eventGroup;
    uint32_t eventBits;
    gpio_num_t gpio;
}key_task_t;

void tareaTeclado(void *arg);