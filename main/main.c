#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include <stdio.h>
#include "pcd8544.h"
#include "driver/spi_common.h"

#define PIN_START GPIO_NUM_22  
#define PIN_RESET GPIO_NUM_21
#define LED_ROJO  GPIO_NUM_32
#define LED_VERDE GPIO_NUM_26

bool estado = 1;
bool reset = 0;
uint32_t cuenta = 0;
bool Suspended = false;
bool ledBlink = 0;
TaskHandle_t taskHandle1;
SemaphoreHandle_t mutexEstado;


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
    TickType_t ultimoTiempo = xTaskGetTickCount();
    
    while (1) {
        vTaskDelayUntil(&ultimoTiempo,pdMS_TO_TICKS(100));
        
        xSemaphoreTake(mutexEstado, portMAX_DELAY);
        if(!Suspended){
            cuenta += 100;
        }
        xSemaphoreGive(mutexEstado);
    }
    
}
void showTime(void * argumentos){ 
    TickType_t ultimoTiempo = xTaskGetTickCount();
    while (1) {
        xSemaphoreTake(mutexEstado, portMAX_DELAY);
            bool resetLocal = reset;
        xSemaphoreGive(mutexEstado);
        
        if(resetLocal){
            pcd8544_clear_display();
            pcd8544_finalize_frame_buf();
        }
        xSemaphoreTake(mutexEstado, portMAX_DELAY);
            float segundos = cuenta * 0.001;
        xSemaphoreGive(mutexEstado);
        pcd8544_set_pos(10,2);
        pcd8544_printf("seg: %2.2f%", segundos);
        pcd8544_sync_and_gc();
        vTaskDelayUntil(&ultimoTiempo,pdMS_TO_TICKS(100));
    }
}

void readKey(void * argumentos){
    static bool startUltimo = 0;
    static bool resetUltimo = 0;
    while (1) {
        
        bool startActual = gpio_get_level(PIN_START);
        bool resetActual = gpio_get_level(PIN_RESET);

        
        if (startUltimo == 1 && startActual == 0) {
            xSemaphoreTake(mutexEstado,portMAX_DELAY);
                estado = !estado; 
            xSemaphoreGive(mutexEstado);
            vTaskDelay(pdMS_TO_TICKS(100));  
        }
        
        if (resetUltimo == 1 && resetActual == 0) {
            xSemaphoreTake(mutexEstado, portMAX_DELAY);
                reset = 1;
            xSemaphoreGive(mutexEstado);
            vTaskDelay(pdMS_TO_TICKS(100));  
        }
        startUltimo = startActual;
        resetUltimo = resetActual;

        vTaskDelay(pdMS_TO_TICKS(100));  
    }
}

void processTask (void * argumentos){
    

    while (1) {
        xSemaphoreTake(mutexEstado, portMAX_DELAY);
        if (estado == 1 && !Suspended) {
           // vTaskSuspend(taskHandle1);
            Suspended = true;
        }
        else if (estado != 1 && Suspended) {
           // vTaskResume(taskHandle1);
            Suspended = false;
        }
        
        else if (reset == 1 && Suspended) {
            cuenta = 0;
            reset = 0;
        }
        else {
            reset = 0;
        }
        xSemaphoreGive(mutexEstado);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void Blinking(void * argumentos) {
    bool bttnStatus;
    while (1) {
        xSemaphoreTake(mutexEstado,portMAX_DELAY);
            bttnStatus =  estado;  
        xSemaphoreGive(mutexEstado);

        if(!bttnStatus){
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
    mutexEstado = xSemaphoreCreateMutex();
    
    configLCD();
    configPin(PIN_START,GPIO_MODE_INPUT);
    configPin(PIN_RESET,GPIO_MODE_INPUT);
    configPin(LED_ROJO,GPIO_MODE_OUTPUT);
    configPin(LED_VERDE,GPIO_MODE_OUTPUT);

    xTaskCreate(readKey, "keys", 2048, NULL, tskIDLE_PRIORITY+3, NULL);
    xTaskCreate(Blinking, "Led", 2048, NULL, tskIDLE_PRIORITY + 1, NULL);
    xTaskCreate(showTime, "show", 2048, NULL, tskIDLE_PRIORITY + 2, NULL);
    xTaskCreate(baseTime, "reloj", 2048, NULL, tskIDLE_PRIORITY + 4, &taskHandle1);
    xTaskCreate(processTask, "process", 2048, NULL, tskIDLE_PRIORITY+5, NULL);
}
