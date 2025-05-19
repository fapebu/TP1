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
#include "timeHour.h"
#include "freertos/queue.h"

#define PIN_START GPIO_NUM_22  
#define PIN_RESET GPIO_NUM_21
#define PIN_PARCIAL GPIO_NUM_19
#define PIN_MODO GPIO_NUM_18
#define PIN_BUZZ GPIO_NUM_23
#define LED_ROJO  GPIO_NUM_32
#define LED_VERDE GPIO_NUM_26
#define BIT_START (1 << 0)
#define BIT_RESET (1 << 1)
#define BIT_PARCIAL (1 << 2)
#define BIT_STOP (1 << 3)
#define BIT_RUN (1 << 4)
#define BIT_MODE (1 << 5)

typedef struct {
    EventBits_t bit;
    gpio_num_t gpio;
    const char *task_name;
} KeyConfig;

TaskHandle_t taskHandle1;
EventGroupHandle_t eventGroup;

static EventGroupHandle_t eventGroupTime;
static EventGroupHandle_t eventGroupHour;
static EventGroupHandle_t eventGroupSetHour;
static EventGroupHandle_t eventGroupSetAlarm;
static EventGroupHandle_t eventGroupAlarm;
static time_task_t time_task_args;
static hour_task_t hour_task_args;
static hora_min_t configHour;
static hora_min_t alarmHour;
static QueueHandle_t laps;
static QueueHandle_t timeQueue;
static QueueHandle_t receiveHourQueue;
static QueueHandle_t sendHourQueue;
static QueueHandle_t alarmQueue;
SemaphoreHandle_t lcd_mutex;
SemaphoreHandle_t config_mutex;
SemaphoreHandle_t alarm_mutex;
static int configMode = 0;
static bool alarmActive = false;


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

void clearLCD() {
    pcd8544_set_pos(0,0);
    pcd8544_printf("                                                                                            ");
    pcd8544_sync_and_gc();
    vTaskDelay(pdMS_TO_TICKS(50));
}

void showHour(void * argumentos){ 
    int configLocal = 0;
   hora_min_t tiempo = {0,0};
    while (1) {
        /*pcd8544_clear_display();
        pcd8544_finalize_frame_buf(); */
         xQueueReceive(receiveHourQueue, &tiempo, portMAX_DELAY);
          int horas   = tiempo.horas;
          int minutos = tiempo.minutos; 
       
         xSemaphoreTake(config_mutex, portMAX_DELAY);
            configLocal = configMode;
        xSemaphoreGive(config_mutex);
        if(configLocal == 0) {
            if (xSemaphoreTake(lcd_mutex, portMAX_DELAY)) {
            pcd8544_set_pos(10,0);
            if(horas < 10) {
                pcd8544_printf("HORA 0");
             }
            else {
                pcd8544_printf("HORA ");
            }
                pcd8544_printf("%d", horas);
            if(minutos < 10) {
                pcd8544_printf(":0%d", minutos);
            }
            else {
                pcd8544_printf(":%d", minutos);
            }
         pcd8544_sync_and_gc();
         xSemaphoreGive(lcd_mutex);
         vTaskDelay(pdMS_TO_TICKS(50));
      }
    }
      }
}

void showTime(void * argumentos){ 
    
    uint32_t lapTimes[3] = {0.0f, 0.0f, 0.0f};
    static uint32_t segundos = 0;
    static int i = 0; 
    int configLocal = 0;
    while (1) {
        
        xQueueReceive(timeQueue, &segundos, portMAX_DELAY); //se blouque hasta que se encolan los datos cada 100ms.
        if (uxQueueMessagesWaiting(laps) > 0) {
                if(i >= 3) {
                    i = 0;
                }
                xQueueReceive(laps, &lapTimes[i], 0); //recibimos el dato instantaneamente si esta en la cola si no lo ignoramos y seguimos ejecutando
                i++;
        }
        xSemaphoreTake(config_mutex, portMAX_DELAY);
            configLocal = configMode;
        xSemaphoreGive(config_mutex);
        if(configLocal == 0) {
        if (xSemaphoreTake(lcd_mutex, portMAX_DELAY)) {
        pcd8544_set_pos(10,2);
        pcd8544_printf("CRON:%2.1f", segundos* 0.001);
        pcd8544_set_pos(10,3);
        pcd8544_printf("Lap1:%2.2f", lapTimes[0]* 0.001);
        pcd8544_set_pos(10,4);
        pcd8544_printf("Lap2:%2.2f", lapTimes[1]* 0.001);
        pcd8544_set_pos(10,5);
        pcd8544_printf("Lap3:%2.2f", lapTimes[2]* 0.001);
        pcd8544_sync_and_gc();
        xSemaphoreGive(lcd_mutex);
        vTaskDelay(pdMS_TO_TICKS(50));
        }
    }
    }
}

void Alarm(void * argumentos){ 
  bool  setLocal = false;
   bool  setAlarm = false;
    

    while (1) {
        
    EventBits_t bits = xEventGroupWaitBits(eventGroupAlarm,BIT_START|BIT_RESET|BIT_PARCIAL,pdTRUE,pdFALSE,0);
        if (BIT_START & bits) {
            setLocal = true;
            xEventGroupSetBits(eventGroup,BIT_RUN);
           
        }
        if (BIT_RESET & bits) { //detenemos la alarma
            printf("detener alarma\n");
            setLocal = false;
            setAlarm = false;
             gpio_set_level(PIN_BUZZ, false);
            xEventGroupSetBits(eventGroupSetAlarm,BIT_STOP);
            xEventGroupSetBits(eventGroup,BIT_STOP);
        }
        
        if (BIT_PARCIAL & bits) { //posponemos 10 min 
            setLocal = false;
            setAlarm = false;
            gpio_set_level(PIN_BUZZ, false);
           xEventGroupSetBits(eventGroupSetAlarm,BIT_RUN);
           xEventGroupSetBits(eventGroup,BIT_STOP);
        }

        if(setLocal){
            setAlarm = !setAlarm;
            gpio_set_level(PIN_BUZZ, setAlarm); 
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
        vTaskDelay(pdMS_TO_TICKS(10));
        }
}

void setHour(void * argumentos){  //configuracion de la hora
   
    bool configLocal = false;
    static int minutos = 0;
    static int horas = 0;
  while (1) {
        
    EventBits_t bits = xEventGroupWaitBits(eventGroupSetHour,BIT_START|BIT_RESET|BIT_PARCIAL|BIT_MODE,pdTRUE,pdFALSE,portMAX_DELAY);
        if (BIT_START & bits) {
            if(minutos > 59) {
                minutos = 0;
            }else{
                minutos++;
            }
        }
        if (BIT_RESET & bits) {
            if(horas > 23) {
                horas = 0;
            }else{
                horas++;
            }
        }
        if(BIT_PARCIAL & bits) {
            configHour.horas = horas;
            configHour.minutos = minutos;
           
            xQueueSend(sendHourQueue, &configHour, portMAX_DELAY);
            pcd8544_set_pos(12,4);
            pcd8544_printf("ACTUALIZADO");
            pcd8544_sync_and_gc();
            vTaskDelay(pdMS_TO_TICKS(50));
        }
        if(BIT_MODE & bits) {
            pcd8544_set_pos(7,0);
        pcd8544_printf("CONFIG HORA");
        pcd8544_set_pos(22,3);
        if(horas < 10) {
            pcd8544_printf("0");
        }
        else {
            pcd8544_printf(" ");
        }
        pcd8544_printf("%d", horas);
        if(minutos < 10) {
           pcd8544_printf(":0%d", minutos);
        }
        else {
            pcd8544_printf(":%d", minutos);
        }
        pcd8544_sync_and_gc();
       vTaskDelay(pdMS_TO_TICKS(50)); 
        
        }
       
         
        xSemaphoreTake(config_mutex, portMAX_DELAY);
            configLocal = configMode;
        xSemaphoreGive(config_mutex);
        if(configLocal == 1) {
        if (xSemaphoreTake(lcd_mutex, portMAX_DELAY)) {
        
        pcd8544_set_pos(7,0);
        pcd8544_printf("CONFIG HORA");
        pcd8544_set_pos(22,3);
        if(horas < 10) {
            pcd8544_printf("0");
        }
        else {
            pcd8544_printf(" ");
        }
        pcd8544_printf("%d", horas);
        if(minutos < 10) {
           pcd8544_printf(":0%d", minutos);
        }
        else {
            pcd8544_printf(":%d", minutos);
        }
        
        pcd8544_sync_and_gc();
        xSemaphoreGive(lcd_mutex);
        vTaskDelay(pdMS_TO_TICKS(50));
        }
        }
         vTaskDelay(pdMS_TO_TICKS(10));
      }
    }

void setAlarm(void * argumentos){ //seteo de la alarma
  int configLocal = false;
    static int minutos = 0;
    static int horas = 0;
    bool actualizar = false;
    while (1) {
        EventBits_t bits = xEventGroupWaitBits(eventGroupSetAlarm,BIT_START|BIT_RESET|BIT_PARCIAL|BIT_MODE|BIT_RUN|BIT_STOP,pdTRUE,pdFALSE,portMAX_DELAY);
        if (BIT_START & bits) {
           if(minutos > 59) {
                minutos = 0;
            }else{
                minutos++;
            }
        }
        if (BIT_RESET & bits) {
            if(horas > 23) {
                horas = 0;
            }else{
                horas++;
            }
        }
        if (BIT_RUN & bits) { //posponemos por 10 min
            printf("RUN\n");
            minutos = minutos + 10;
            if(minutos >= 60) {
                minutos = minutos - 60;
                horas++ ;
                
            }
            actualizar = true; 
        }
        if (BIT_STOP & bits) { //apagamos la alarma
            printf("STOP ALRMA\n");
            minutos = 0;
            horas = 0;
            actualizar = true; 
        }
        
        if (BIT_PARCIAL & bits || actualizar) {
            
            alarmHour.horas = horas;
            alarmHour.minutos = minutos;
           
            xQueueSend(alarmQueue, &alarmHour, portMAX_DELAY);
            if(!actualizar){
            pcd8544_set_pos(12,4);
            pcd8544_printf("GUARDADO");
            pcd8544_sync_and_gc();
            vTaskDelay(pdMS_TO_TICKS(50));
            }
            actualizar = false;
        }
         if (BIT_MODE & bits) {
           pcd8544_set_pos(7,0);
                pcd8544_printf("CONFIG ALARM");
                pcd8544_set_pos(22,3);
                if(horas < 10) {
                    pcd8544_printf("0");
                }
            else {
                pcd8544_printf(" ");
            }
            pcd8544_printf("%d", horas);
            if(minutos < 10) {
                pcd8544_printf(":0%d", minutos);
            }
            else {
                pcd8544_printf(":%d", minutos);
            }
            pcd8544_sync_and_gc();
            vTaskDelay(pdMS_TO_TICKS(50));
        }
            
        
        xSemaphoreTake(config_mutex, portMAX_DELAY);
            configLocal = configMode;
        xSemaphoreGive(config_mutex);
        if(configLocal == 2) {
            if (xSemaphoreTake(lcd_mutex, portMAX_DELAY)) {
                pcd8544_set_pos(7,0);
                pcd8544_printf("CONFIG ALARM");
                pcd8544_set_pos(22,3);
                if(horas < 10) {
                    pcd8544_printf("0");
                }
            else {
                pcd8544_printf(" ");
            }
            pcd8544_printf("%d", horas);
            if(minutos < 10) {
                pcd8544_printf(":0%d", minutos);
            }
            else {
                pcd8544_printf(":%d", minutos);
            }
        
      
            pcd8544_sync_and_gc();
            xSemaphoreGive(lcd_mutex);
            vTaskDelay(pdMS_TO_TICKS(50));
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
      }
      }

void readKey(void * argumentos){  //eventos de las teclas
    int LocalconfigMode = 0;
    static bool AlarmLocal = false;
    while (1) {
              
        EventBits_t bits = xEventGroupWaitBits(eventGroup,BIT_START|BIT_RESET|BIT_MODE|BIT_PARCIAL|BIT_RUN|BIT_STOP,pdTRUE,pdFALSE,portMAX_DELAY);
               static bool runLocal = false;
               if (BIT_START & bits) {
                   

                   if(AlarmLocal) {
                            printf("start\n");
                        }
                   else if(LocalconfigMode == 0){
                        if(runLocal == false) {
                            runLocal = true;
                            xEventGroupSetBits(time_task_args.event,time_task_args.start);
                        }
                        else {
                            runLocal = false;
                            xEventGroupSetBits(time_task_args.event,time_task_args.stop);
                        } 
                    }else if(LocalconfigMode == 1) {
                            xEventGroupSetBits(eventGroupSetHour,BIT_START);
                    }
                    else if(LocalconfigMode == 2) {
                            xEventGroupSetBits(eventGroupSetAlarm,BIT_START);
                    } 
                   
                }

                if (BIT_RESET & bits) {
                    if(AlarmLocal) {
                            xEventGroupSetBits(eventGroupAlarm,BIT_RESET);
                             printf("detener alarma\n");
                        }
                    else if(LocalconfigMode == 0){
                    xEventGroupSetBits(time_task_args.event,time_task_args.reset);
                    }
                    else if(LocalconfigMode == 1) {
                            xEventGroupSetBits(eventGroupSetHour,BIT_RESET);
                        }
                    else if(LocalconfigMode == 2) {
                            xEventGroupSetBits(eventGroupSetAlarm,BIT_RESET);
                        }
                    
                }

                if (BIT_PARCIAL & bits) {
                    if(AlarmLocal) {
                            xEventGroupSetBits(eventGroupAlarm,BIT_PARCIAL);
                             
                    }
                    else if(LocalconfigMode == 0){
                        xEventGroupSetBits(time_task_args.event,time_task_args.parcial);
                    }
                    else if(LocalconfigMode == 1) {
                            xEventGroupSetBits(eventGroupSetHour,BIT_PARCIAL);
                        }
                    else if(LocalconfigMode == 2) {
                            xEventGroupSetBits(eventGroupSetAlarm,BIT_PARCIAL);
                        }
                  
                }
                if (BIT_RUN & bits) {
                    AlarmLocal = true;
                }
                if (BIT_STOP & bits) {
                    AlarmLocal = false;
                }

                if (BIT_MODE & bits) {
                    
                    if(LocalconfigMode<2) {
                        LocalconfigMode++;
                    }else {
                        LocalconfigMode = 0;
                    }
                    
                    switch (LocalconfigMode)
                    {
                    case 1:
                       clearLCD();
                       xEventGroupSetBits(eventGroupSetHour, BIT_MODE);
                        break;
                    
                    case 2:
                       clearLCD();
                       xEventGroupSetBits(eventGroupSetAlarm, BIT_MODE);
                        break;
                    default:
                       clearLCD();
                       break;
                    }

                    xSemaphoreTake(config_mutex, portMAX_DELAY);
                    configMode = LocalconfigMode;
                    xSemaphoreGive(config_mutex);
                     
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
        { BIT_PARCIAL, PIN_PARCIAL, "Tecla3"},
        { BIT_MODE,    PIN_MODO,    "Tecla4"}
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
    lcd_mutex = xSemaphoreCreateMutex();
    config_mutex = xSemaphoreCreateMutex();
    alarm_mutex = xSemaphoreCreateMutex();
    configLCD();
    configPin(PIN_START,GPIO_MODE_INPUT);
    configPin(PIN_RESET,GPIO_MODE_INPUT);
    configPin(PIN_PARCIAL,GPIO_MODE_INPUT);
    configPin(PIN_MODO,GPIO_MODE_INPUT);
    configPin(LED_ROJO,GPIO_MODE_OUTPUT);
    configPin(LED_VERDE,GPIO_MODE_OUTPUT);
    configPin(PIN_BUZZ,GPIO_MODE_OUTPUT);

    eventGroup = xEventGroupCreate();
    initKeyTasks(eventGroup); //creamos las tareas de teclado

    laps = xQueueCreate(3, sizeof(uint32_t));
    timeQueue = xQueueCreate(10, sizeof(uint32_t));
    sendHourQueue = xQueueCreate(10, sizeof(hora_min_t));
    receiveHourQueue = xQueueCreate(10, sizeof(hora_min_t));
    alarmQueue = xQueueCreate(10, sizeof(hora_min_t));
    eventGroupTime = xEventGroupCreate();
    eventGroupHour = xEventGroupCreate();
    eventGroupSetHour = xEventGroupCreate();
    eventGroupSetAlarm = xEventGroupCreate();
    eventGroupAlarm = xEventGroupCreate(); 

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

     hour_task_args = (hour_task_t){
        .sendQueue = receiveHourQueue,
        .receiveQueue = sendHourQueue,
        .receiveAlarmQueue = alarmQueue,
        .eventAlarm = eventGroupAlarm,
        .eventTime = eventGroupHour,
        .start     = BIT_START,
        .reset     = BIT_RESET,
        .posponer   = BIT_PARCIAL,
    };

    xTaskCreate(HourTask, "HourTask", 2 * configMINIMAL_STACK_SIZE, (void *)&hour_task_args, tskIDLE_PRIORITY + 5, NULL);
    xTaskCreate(TimeTask, "TimeTask", 2 * configMINIMAL_STACK_SIZE, (void *)&time_task_args, tskIDLE_PRIORITY + 5, NULL);
    xTaskCreate(readKey, "keys", 2 * configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY + 4, NULL);
    xTaskCreate(Blinking, "Led", 2 * configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY + 1, NULL);
    xTaskCreate(showTime, "show", 2 * configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY + 3, NULL);
    xTaskCreate(showHour, "show2", 2 * configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY + 3, NULL);
    xTaskCreate(setHour, "setHour", 2 * configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY + 3, NULL);
    xTaskCreate(setAlarm, "setAlarm", 2 * configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY + 3, NULL);
    xTaskCreate(Alarm, "Alarm", 2 * configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY + 4, NULL);
}
