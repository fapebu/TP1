#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include <stdio.h>
#include "pcd8544.h"
#include "driver/spi_common.h"
#include "freertos/event_groups.h"
#include "teclado.h"
#include "time.h"
#include "freertos/queue.h"

#define PIN_START GPIO_NUM_22  
#define PIN_RESET GPIO_NUM_21
#define PIN_PARCIAL GPIO_NUM_19
#define LED_ROJO  GPIO_NUM_32
#define LED_VERDE GPIO_NUM_26
#define BIT_START (1 << 0)
#define BIT_RESET (1 << 1)
#define BIT_PARCIAL (1 << 2)
#define BIT_STOP (1 << 3)
#define BIT_RUN (1 << 4)


TaskHandle_t taskHandle1;
EventGroupHandle_t eventGroup;

static EventGroupHandle_t shared_event_group;
static time_task_t time_task_args;
static QueueHandle_t laps;
static QueueHandle_t timeQueue;


void configPin(gpio_num_t pin, gpio_mode_t modo) {
    gpio_set_direction(pin, modo);
    if(modo == GPIO_MODE_INPUT)
    gpio_pullup_en(pin);
}

void configLCD() {
    pcd8544_config_t config = {
        .spi_host = HSPI_HOST,
        .is_backlight_common_anode = true,
    };
    pcd8544_init(&config);
    pcd8544_set_contrast(26);
    pcd8544_clear_display();
    pcd8544_finalize_frame_buf();
}

void showTime(void * argumentos){ 
    TickType_t ultimo = xTaskGetTickCount();
    uint32_t lapTimes[3] = {0.0f, 0.0f, 0.0f};
    
    static uint32_t segundos = 0;
    static int i = 0; 
    while (1) {
        /*pcd8544_clear_display();
        pcd8544_finalize_frame_buf(); */
        xQueueReceive(timeQueue, &segundos, 50);
        if (uxQueueMessagesWaiting(laps) > 0) {
                if(i >= 3) {
                    i = 0;
                }
                xQueueReceive(laps, &lapTimes[i], 0);
                i++;
                
            }
        
        pcd8544_set_pos(10,1);
        pcd8544_printf("Tiempo: %2.2f", segundos* 0.001);
        pcd8544_set_pos(10,3);
        pcd8544_printf("Lap1: %2.2f", lapTimes[0]* 0.001);
        pcd8544_set_pos(10,4);
        pcd8544_printf("Lap2: %2.2f", lapTimes[1]* 0.001);
        pcd8544_set_pos(10,5);
        pcd8544_printf("Lap3: %2.2f", lapTimes[2]* 0.001);
        pcd8544_sync_and_gc();
        vTaskDelayUntil(&ultimo, pdMS_TO_TICKS(100));
    }
}

void readKey(void * argumentos){
       while (1) {
              
        EventBits_t bits = xEventGroupWaitBits(eventGroup,BIT_START|BIT_RESET|BIT_PARCIAL,pdTRUE,pdFALSE,portMAX_DELAY);
               static bool runLocal = false;
                if (BIT_START & bits) {
                    if(runLocal == false) {
                        runLocal = true;
                        xEventGroupSetBits(time_task_args.event,time_task_args.start);
                    }
                    else {
                        runLocal = false;
                        xEventGroupSetBits(time_task_args.event,time_task_args.stop);
                    } 
                    vTaskDelay(pdMS_TO_TICKS(50));
                }
        
                if (BIT_RESET & bits) {
                    printf("reset\n");
                    xEventGroupSetBits(time_task_args.event,time_task_args.reset);
                    vTaskDelay(pdMS_TO_TICKS(50));
                }
                if (BIT_PARCIAL & bits) {
                    printf("parcial\n");
                    xEventGroupSetBits(time_task_args.event,time_task_args.parcial);
                    vTaskDelay(pdMS_TO_TICKS(50));
                }
                vTaskDelay(pdMS_TO_TICKS(10));
            }
        }


void Blinking(void * argumentos) {
    static bool runLocal = false;
    static  bool ledBlink = false;
    static TickType_t lastEventTime = 0;  // Variable para guardar el último tiempo de evento
    const TickType_t timeout = pdMS_TO_TICKS(200);  // Tiempo en milisegundos sin evento para cambiar a ROJO

    while (1) {
    EventBits_t bits = xEventGroupWaitBits(shared_event_group,BIT_RUN,pdTRUE,pdFALSE,pdMS_TO_TICKS(50));
              if (BIT_RUN & bits) {
                runLocal = true;
                lastEventTime = xTaskGetTickCount();
              }
              if (xTaskGetTickCount() - lastEventTime >= timeout) {
                runLocal = false;  
            }
      if(runLocal){
            gpio_set_level(LED_ROJO, 0); //Apagamos el LED ROJO 
            ledBlink = !ledBlink;
            gpio_set_level(LED_VERDE, ledBlink);
            vTaskDelay(pdMS_TO_TICKS(500));
        }else {
            ledBlink = 0; //Apagamos el LED Verde
            gpio_set_level(LED_VERDE, ledBlink);

            gpio_set_level(LED_ROJO, 1);
            vTaskDelay(pdMS_TO_TICKS(100));  
        }
  
    }
}

void app_main() {

    static key_task_t key_args;

    configLCD();
    configPin(PIN_START,GPIO_MODE_INPUT);
    configPin(PIN_RESET,GPIO_MODE_INPUT);
    configPin(PIN_PARCIAL,GPIO_MODE_INPUT);
    configPin(LED_ROJO,GPIO_MODE_OUTPUT);
    configPin(LED_VERDE,GPIO_MODE_OUTPUT);

    eventGroup = xEventGroupCreate();
    xEventGroupClearBits(eventGroup, BIT_START | BIT_RESET | BIT_PARCIAL);
    if(eventGroup) {
       // key_args = malloc(sizeof(struct key_task_t));
        key_args.eventGroup = eventGroup;
        key_args.eventBits = BIT_START;
        key_args.gpio = PIN_START;
        xTaskCreate(tareaTeclado, "Tecla1", 2 * configMINIMAL_STACK_SIZE, (void *)&key_args, tskIDLE_PRIORITY + 3, NULL);
        
        key_args.eventGroup = eventGroup;
        key_args.eventBits = BIT_RESET;
        key_args.gpio = PIN_RESET;
        xTaskCreate(tareaTeclado, "Tecla2", 2 * configMINIMAL_STACK_SIZE, (void *)&key_args, tskIDLE_PRIORITY + 3, NULL);
        
        key_args.eventGroup = eventGroup;
        key_args.eventBits = BIT_PARCIAL;
        key_args.gpio = PIN_PARCIAL;
        xTaskCreate(tareaTeclado, "Tecla3", 2 * configMINIMAL_STACK_SIZE, (void *)&key_args, tskIDLE_PRIORITY + 3, NULL);
    }
    laps = xQueueCreate(3, sizeof(uint32_t));
    timeQueue = xQueueCreate(10, sizeof(uint32_t));

    shared_event_group = xEventGroupCreate();
    time_task_args.event = shared_event_group; // Pasar el handle compartido
    time_task_args.start = BIT_START;
    time_task_args.reset = BIT_RESET;
    time_task_args.parcial = BIT_PARCIAL;
    time_task_args.stop = BIT_STOP;
    time_task_args.runing = BIT_RUN;
    time_task_args.lapsQueue = laps;
    time_task_args.timeQueue = timeQueue;

    xTaskCreate(TimeTask, "TimeTask", 2 * configMINIMAL_STACK_SIZE, (void *)&time_task_args, tskIDLE_PRIORITY + 3, NULL);

    xTaskCreate(readKey, "keys", 2 * configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY + 3, NULL);
    xTaskCreate(Blinking, "Led", 2 * configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY + 1, NULL);
    xTaskCreate(showTime, "show", 2 * configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY + 4, NULL);
    //xTaskCreate(baseTime, "reloj",2 * configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY + 5, &taskHandle1);
   // xTaskCreate(processTask, "process", 2 * configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY + 4, NULL); 
}
