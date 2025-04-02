#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include <stdio.h>
#include "pcd8544.h"
#include "driver/spi_common.h"

#define PIN_START GPIO_NUM_22  
#define PIN_RESET GPIO_NUM_21
#define LED_ROJO  GPIO_NUM_32
#define LED_VERDE GPIO_NUM_26

bool estado = 0;
bool reset = 0;
float cuenta = 0;
bool startUltimo = 0;
bool resetUltimo = 0;
TaskHandle_t taskHandle1;

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
void reloj (void * argumentos){
    while (1) {
        if(reset){
            cuenta = 0;
            reset = 0;
        }
        vTaskDelay( pdMS_TO_TICKS(100) );
        cuenta += 100;
    }
    
}
void showTime(void * argumentos){
    while (1) {
        float segundos = cuenta*0.001;
        pcd8544_set_pos(10,2);
        pcd8544_printf("seg: %2.2f%", segundos);
        pcd8544_sync_and_gc();
        vTaskDelay(pdMS_TO_TICKS(50));  // Esperar 50 milisegundos
    }
}

void leerPin(void * argumentos){
    while (1) {
        
        bool startActual = gpio_get_level(PIN_START);
        bool resetActual = gpio_get_level(PIN_RESET);

        
        if (startUltimo == 1 && startActual == 0) {
            estado = !estado;  
            vTaskDelay(pdMS_TO_TICKS(50));  
        }
        
        if (resetUltimo == 1 && resetActual == 0) {
            reset = 1;
            vTaskDelay(pdMS_TO_TICKS(50));  
        }
        startUltimo = startActual;
        resetUltimo = resetActual;

        vTaskDelay(pdMS_TO_TICKS(10));  
    }
}

void processKey (void * argumentos){ 
    while (1) {
        if (estado == 1){
        
            if (eTaskGetState(taskHandle1) != eSuspended) { 
                vTaskSuspend(taskHandle1);// Pausa la cuenta.
            } 
        }
        else{
            if (eTaskGetState(taskHandle1) ==  eSuspended) { 
                vTaskResume(taskHandle1); // Reanuda la cuenta.
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(500)); 
    }
}

void Blinking(void * argumentos) {
   
    gpio_set_level(LED_VERDE, 0);

    vTaskDelay(pdMS_TO_TICKS(500));
    while (1) {
        if(!estado){
        gpio_set_level(LED_ROJO, 0); //Apagamos el LED ROJO 
        
        gpio_set_level(LED_VERDE, 1);
        vTaskDelay(pdMS_TO_TICKS(500));
    
        gpio_set_level(LED_VERDE, 0);
        vTaskDelay(pdMS_TO_TICKS(500));
    } else {
        gpio_set_level(LED_ROJO, 1);
        vTaskDelay(pdMS_TO_TICKS(100));  
    }
}
}

void app_main() {
    configLCD();
    configPin(PIN_START,GPIO_MODE_INPUT);
    configPin(PIN_RESET,GPIO_MODE_INPUT);
    configPin(LED_ROJO,GPIO_MODE_OUTPUT);
    configPin(LED_VERDE,GPIO_MODE_OUTPUT);

    xTaskCreate(leerPin, "keyboard", 2048, NULL, tskIDLE_PRIORITY+2, NULL);
    xTaskCreate(Blinking, "Led", 2048, NULL, tskIDLE_PRIORITY + 1, NULL);
    xTaskCreate(showTime, "show", 2048, NULL, tskIDLE_PRIORITY + 3, NULL);
    xTaskCreate(reloj, "reloj", 2048, NULL, tskIDLE_PRIORITY + 4, &taskHandle1);
    xTaskCreate(processKey, "process", 2048, NULL, tskIDLE_PRIORITY+5, NULL);
}
