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


uint32_t cuenta = 0;

bool ledBlink = 0;
TaskHandle_t taskHandle1;
SemaphoreHandle_t flags_mutex;
SemaphoreHandle_t cuenta_mutex;

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
        vTaskDelayUntil(&ultimo, pdMS_TO_TICKS(100));
        xSemaphoreTake(flags_mutex, portMAX_DELAY);
          bool run = flags.running;
        xSemaphoreGive(flags_mutex);

        if (run) {
            xSemaphoreTake(cuenta_mutex, portMAX_DELAY);
              cuenta += 100;
              printf("Segundos: %.2f\n", cuenta * 0.001);
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
    static bool startUltimo = 0;
    static bool resetUltimo = 0;
        while (1) {
            bool startActual = gpio_get_level(PIN_START);
            bool resetActual = gpio_get_level(PIN_RESET);

            if (startUltimo == 1 && startActual == 0) {
                xSemaphoreTake(flags_mutex, portMAX_DELAY);
                  flags.startPressed = !flags.startPressed;
                xSemaphoreGive(flags_mutex);
                vTaskDelay(pdMS_TO_TICKS(50));
            }
    
            if (resetUltimo == 1 && resetActual == 0) {
                xSemaphoreTake(flags_mutex, portMAX_DELAY);
                  flags.resetPressed = true;
                xSemaphoreGive(flags_mutex);
                vTaskDelay(pdMS_TO_TICKS(50));
            }
    
            startUltimo = startActual;
            resetUltimo = resetActual;
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
  
    configLCD();
    configPin(PIN_START,GPIO_MODE_INPUT);
    configPin(PIN_RESET,GPIO_MODE_INPUT);
    configPin(LED_ROJO,GPIO_MODE_OUTPUT);
    configPin(LED_VERDE,GPIO_MODE_OUTPUT);

    xTaskCreate(readKey, "keys", 2 * configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY + 3, NULL);
    xTaskCreate(Blinking, "Led", 2 * configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY + 1, NULL);
    xTaskCreate(showTime, "show", 2 * configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY + 3, NULL);
    xTaskCreate(baseTime, "reloj",2 * configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY + 4, &taskHandle1);
    xTaskCreate(processTask, "process", 2 * configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY + 3, NULL);
}
