#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include <stdio.h>
#include "pcd8544.h"
#include "driver/spi_common.h"
#include "freertos/event_groups.h"
#include "teclado.h"

#define PIN_START GPIO_NUM_22  
#define PIN_RESET GPIO_NUM_21
#define PIN_PARCIAL GPIO_NUM_19
#define LED_ROJO  GPIO_NUM_32
#define LED_VERDE GPIO_NUM_26
#define BIT_START (1 << 0)
#define BIT_RESET (1 << 1)
#define BIT_PARCIAL (1 << 2)

uint32_t cuenta = 0;

bool ledBlink = 0;
TaskHandle_t taskHandle1;
SemaphoreHandle_t flags_mutex;
SemaphoreHandle_t cuenta_mutex;
EventGroupHandle_t eventGroup;
typedef struct {
    bool startPressed;  
    bool resetPressed;  
    bool running;       
} app_flags_t;

static app_flags_t flags = {
    .startPressed = true,
    .resetPressed = false,
    .running      = false
};

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
void baseTime (void * argumentos){
    TickType_t ultimo = xTaskGetTickCount();
    while (1) {
        xSemaphoreTake(flags_mutex, portMAX_DELAY);
          bool run = flags.running;
        xSemaphoreGive(flags_mutex);
        
        vTaskDelayUntil(&ultimo, pdMS_TO_TICKS(100));
        if (run) {
            xSemaphoreTake(cuenta_mutex, pdMS_TO_TICKS(50));
              cuenta += 100;
              
            xSemaphoreGive(cuenta_mutex);
        }
    }
    
}
void showTime(void * argumentos){ 
    TickType_t ultimo = xTaskGetTickCount();
    while (1) {
      
        bool resetLocal = false;
        xSemaphoreTake(flags_mutex, portMAX_DELAY);
        resetLocal = flags.resetPressed;
        xSemaphoreGive(flags_mutex);

        if (resetLocal) {
            pcd8544_clear_display();
            pcd8544_finalize_frame_buf();
        }
        uint32_t cuentaLocal;
        xSemaphoreTake(cuenta_mutex, portMAX_DELAY);
           cuentaLocal = cuenta;
        xSemaphoreGive(cuenta_mutex);

        float segundos = cuentaLocal * 0.001;
        pcd8544_set_pos(10,2);
        pcd8544_printf("seg: %2.2f", segundos);
        pcd8544_sync_and_gc();
        vTaskDelayUntil(&ultimo, pdMS_TO_TICKS(100));
    }
}

void readKey(void * argumentos){
       while (1) {
              
        EventBits_t bits = xEventGroupWaitBits(eventGroup,BIT_START|BIT_RESET,pdTRUE,pdFALSE,portMAX_DELAY);
               
                if (BIT_START & bits) {
                    printf("Start\n");
                    xSemaphoreTake(flags_mutex, portMAX_DELAY);
                      flags.startPressed = !flags.startPressed;
                    xSemaphoreGive(flags_mutex);
                    vTaskDelay(pdMS_TO_TICKS(50));
                }
        
                if (BIT_RESET & bits) {
                    printf("reset\n");
                    xSemaphoreTake(flags_mutex, portMAX_DELAY);
                      flags.resetPressed = true;
                    xSemaphoreGive(flags_mutex);
                    vTaskDelay(pdMS_TO_TICKS(50));
                }
        
                vTaskDelay(pdMS_TO_TICKS(10));
            }
        }
void processTask (void * argumentos){
    
    while (1) {
            
            bool startLocal, runLocal, resetLocal;
    
            xSemaphoreTake(flags_mutex, portMAX_DELAY);
                startLocal    = flags.startPressed;
                runLocal = flags.running;
                resetLocal = flags.resetPressed;
            xSemaphoreGive(flags_mutex);
    
            if (startLocal && runLocal) {
                    runLocal = false;
            }
            else if (!startLocal && !runLocal) { 
                        runLocal = true; 
            }
            if (resetLocal == 1 && !runLocal) {
                xSemaphoreTake(cuenta_mutex, portMAX_DELAY);
                 cuenta = 0;
                xSemaphoreGive(cuenta_mutex);
            }
            xSemaphoreTake(flags_mutex, portMAX_DELAY);
                 flags.startPressed = startLocal;
                 flags.running = runLocal;
                 flags.resetPressed = false;
            xSemaphoreGive(flags_mutex);
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }

void Blinking(void * argumentos) {
    bool startLocal;
    while (1) {
        xSemaphoreTake(flags_mutex,portMAX_DELAY);
        startLocal = flags.startPressed;  
        xSemaphoreGive(flags_mutex);

        if(!startLocal){
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
    flags_mutex = xSemaphoreCreateMutex();
    cuenta_mutex = xSemaphoreCreateMutex();
   // EventGroupHandle_t eventGroup; 
    //EventBits_t keys;
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
    xTaskCreate(readKey, "keys", 2 * configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY + 3, NULL);
    xTaskCreate(Blinking, "Led", 2 * configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY + 1, NULL);
    xTaskCreate(showTime, "show", 2 * configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY + 3, NULL);
    xTaskCreate(baseTime, "reloj",2 * configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY + 5, &taskHandle1);
    xTaskCreate(processTask, "process", 2 * configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY + 4, NULL); 
}
