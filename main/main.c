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

typedef struct {
    EventBits_t bit;
    gpio_num_t gpio;
    const char *task_name;
} KeyConfig;

TaskHandle_t taskHandle1;
EventGroupHandle_t eventGroup;

static EventGroupHandle_t eventGroupTime;
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
   
    uint32_t lapTimes[3] = {0.0f, 0.0f, 0.0f};
    static uint32_t segundos = 0;
    static int i = 0; 
    while (1) {
        /*pcd8544_clear_display();
        pcd8544_finalize_frame_buf(); */
        xQueueReceive(timeQueue, &segundos, portMAX_DELAY); //se blouque hasta que se encolan los datos cada 100ms.
        if (uxQueueMessagesWaiting(laps) > 0) {
                if(i >= 3) {
                    i = 0;
                }
                xQueueReceive(laps, &lapTimes[i], 0); //recibimos el dato instantaneamente si esta en la cola si no lo ignoramos y seguimos ejecutando
                i++;
        }
        
        pcd8544_set_pos(10,1);
        pcd8544_printf("Time:%2.2f", segundos* 0.001);
        pcd8544_set_pos(10,3);
        pcd8544_printf("Lap1:%2.2f", lapTimes[0]* 0.001);
        pcd8544_set_pos(10,4);
        pcd8544_printf("Lap2:%2.2f", lapTimes[1]* 0.001);
        pcd8544_set_pos(10,5);
        pcd8544_printf("Lap3:%2.2f", lapTimes[2]* 0.001);
        pcd8544_sync_and_gc();
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
                }
        
                if (BIT_RESET & bits) {
                    printf("reset\n");
                    xEventGroupSetBits(time_task_args.event,time_task_args.reset);
                }
                if (BIT_PARCIAL & bits) {
                    printf("parcial\n");
                    xEventGroupSetBits(time_task_args.event,time_task_args.parcial);
                }
               
            }
        }


void Blinking(void * argumentos) {
    static bool runLocal = false;
    static  bool ledBlink = false;
    static TickType_t lastEventTime = 0;  
    const TickType_t timeout = pdMS_TO_TICKS(150); 

    while (1) {
    EventBits_t bits = xEventGroupWaitBits(eventGroupTime,BIT_RUN,pdTRUE,pdFALSE,pdMS_TO_TICKS(50));
              if (BIT_RUN & bits) {
                runLocal = true;
                lastEventTime = xTaskGetTickCount();
              }
              if (xTaskGetTickCount() - lastEventTime >= timeout) {
                runLocal = false;  
            }
      if(runLocal){
            gpio_set_level(LED_ROJO, 0); 
            ledBlink = !ledBlink;
            gpio_set_level(LED_VERDE, ledBlink);
            vTaskDelay(pdMS_TO_TICKS(450)); //aproximamos a 500ms el tiempo de encendido y apagado del led verde
        }else {
            ledBlink = 0;
            gpio_set_level(LED_VERDE, ledBlink);
            gpio_set_level(LED_ROJO, 1);
            vTaskDelay(pdMS_TO_TICKS(50));  
        }
  
    }
}
static void initKeyTasks(EventGroupHandle_t even) {
    KeyConfig keys[] = {
        { BIT_START,   PIN_START,   "Tecla1"},
        { BIT_RESET,   PIN_RESET,   "Tecla2"},
        { BIT_PARCIAL, PIN_PARCIAL, "Tecla3"}
    };
    for (int i = 0; i < sizeof(keys)/sizeof(keys[0]); ++i) {
        key_task_t args = {
            .eventGroup = even,
            .eventBits  = keys[i].bit,
            .gpio       = keys[i].gpio
        };
        xTaskCreate(tareaTeclado,keys[i].task_name,2 * configMINIMAL_STACK_SIZE,&args,tskIDLE_PRIORITY + 3,NULL);
    }
}
void app_main() {
    configLCD();
    configPin(PIN_START,GPIO_MODE_INPUT);
    configPin(PIN_RESET,GPIO_MODE_INPUT);
    configPin(PIN_PARCIAL,GPIO_MODE_INPUT);
    configPin(LED_ROJO,GPIO_MODE_OUTPUT);
    configPin(LED_VERDE,GPIO_MODE_OUTPUT);

    eventGroup = xEventGroupCreate();
    initKeyTasks(eventGroup); //creamos las tareas de teclado

    laps = xQueueCreate(3, sizeof(uint32_t));
    timeQueue = xQueueCreate(10, sizeof(uint32_t));

    eventGroupTime = xEventGroupCreate();
 
    time_task_args = (time_task_t){
        .event     = eventGroupTime,
        .start     = BIT_START,
        .reset     = BIT_RESET,
        .parcial   = BIT_PARCIAL,
        .stop      = BIT_STOP,
        .runing    = BIT_RUN,
        .lapsQueue = laps,
        .timeQueue = timeQueue
    };

    xTaskCreate(TimeTask, "TimeTask", 2 * configMINIMAL_STACK_SIZE, (void *)&time_task_args, tskIDLE_PRIORITY + 5, NULL);
    xTaskCreate(readKey, "keys", 2 * configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY + 3, NULL);
    xTaskCreate(Blinking, "Led", 2 * configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY + 1, NULL);
    xTaskCreate(showTime, "show", 2 * configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY + 4, NULL);
    
}
