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
bool Suspended = true;
bool ledBlink = 0;
TaskHandle_t taskHandle1;
SemaphoreHandle_t mutexEstado;
SemaphoreHandle_t mutexCuenta;
SemaphoreHandle_t mutexReset;
SemaphoreHandle_t mutexSuspend;

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
        xSemaphoreTake(mutexSuspend, portMAX_DELAY);
          bool  LocalSuspended =  Suspended;
          
        xSemaphoreGive(mutexSuspend);

        vTaskDelayUntil(&ultimoTiempo,pdMS_TO_TICKS(100));
        
        if(!LocalSuspended){
            xSemaphoreTake(mutexCuenta, portMAX_DELAY);
                cuenta += 100;
            xSemaphoreGive(mutexCuenta);
        }
        
    }
    
}
void showTime(void * argumentos){ 
    TickType_t ultimoTiempo = xTaskGetTickCount();
    while (1) {
        xSemaphoreTake(mutexReset, portMAX_DELAY);
            bool resetLocal = reset;
        xSemaphoreGive(mutexReset);
        
        if(resetLocal){
            pcd8544_clear_display();
            pcd8544_finalize_frame_buf();
        }
        xSemaphoreTake(mutexCuenta, portMAX_DELAY);
            float segundos = cuenta * 0.001;
        xSemaphoreGive(mutexCuenta);

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
            xSemaphoreTake(mutexReset, portMAX_DELAY);
                reset = 1;
            xSemaphoreGive(mutexReset);
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
            bool estadoLocal = estado;
        xSemaphoreGive(mutexEstado);
        
        xSemaphoreTake(mutexSuspend, portMAX_DELAY);
            bool suspendedLocal = Suspended;
        xSemaphoreGive(mutexSuspend);
       
        if (estadoLocal && !suspendedLocal) {
           // vTaskSuspend(taskHandle1);
            xSemaphoreTake(mutexSuspend, portMAX_DELAY);
                Suspended = true;
            xSemaphoreGive(mutexSuspend);
        }
        else if (!estadoLocal && suspendedLocal) {
           // vTaskResume(taskHandle1);
            xSemaphoreTake(mutexSuspend, portMAX_DELAY);
                Suspended = false;
            xSemaphoreGive(mutexSuspend);
        }
        
        if (reset == 1 && Suspended) {
            xSemaphoreTake(mutexCuenta, portMAX_DELAY);
                cuenta = 0;
            xSemaphoreGive(mutexCuenta);
        }
            xSemaphoreTake(mutexReset, portMAX_DELAY);
                reset = 0;
            xSemaphoreGive(mutexReset);
        
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
    mutexCuenta = xSemaphoreCreateMutex();
    mutexReset = xSemaphoreCreateMutex();
    mutexSuspend = xSemaphoreCreateMutex();
    configLCD();
    configPin(PIN_START,GPIO_MODE_INPUT);
    configPin(PIN_RESET,GPIO_MODE_INPUT);
    configPin(LED_ROJO,GPIO_MODE_OUTPUT);
    configPin(LED_VERDE,GPIO_MODE_OUTPUT);

    xTaskCreate(readKey, "keys", 2048, NULL, tskIDLE_PRIORITY + 3, NULL);
    xTaskCreate(Blinking, "Led", 2048, NULL, tskIDLE_PRIORITY + 1, NULL);
    xTaskCreate(showTime, "show", 2048, NULL, tskIDLE_PRIORITY + 3, NULL);
    xTaskCreate(baseTime, "reloj", 2048, NULL, tskIDLE_PRIORITY + 4, &taskHandle1);
    xTaskCreate(processTask, "process", 2048, NULL, tskIDLE_PRIORITY + 3, NULL);
}
