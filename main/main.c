#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include <stdio.h>
#include "pcd8544.h"
#include "driver/spi_common.h"

#define PIN_ENTRADA GPIO_NUM_22  // Cambia esto por el pin que necesites

const gpio_num_t LED_ROJO = GPIO_NUM_32;
const gpio_num_t LED_VERDE = GPIO_NUM_5;
bool estado = 0;
float cuenta = 0;
TaskHandle_t taskHandle1;

void configurar_pin_entrada() {
    gpio_config_t io_conf = {};
    io_conf.pin_bit_mask = (1ULL << PIN_ENTRADA);  // Seleccionar el pin
    io_conf.mode = GPIO_MODE_INPUT;               // Configurar como entrada
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;     // Deshabilitar pull-up
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE; // Deshabilitar pull-down
    io_conf.intr_type = GPIO_INTR_DISABLE;        // Sin interrupciones

    gpio_config(&io_conf);  // Aplicar configuración
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
        vTaskDelay ( pdMS_TO_TICKS(100) );  // Esperar 100 milisegundos
        cuenta += 100;
    }
    
}
void showTime(void * argumentos){
    while (1) {
    
    float segundos = cuenta*0.001;
    pcd8544_set_pos(10,2);
    pcd8544_printf("seg: %2.2f%", segundos);
    pcd8544_sync_and_gc();
    //pcd8544_free();
    printf("El valor de cuenta es: %f\n", segundos);
   
    vTaskDelay(pdMS_TO_TICKS(50));  // Esperar 1 segundo
    }
}

void leerPin(void * argumentos){
    while (1) {
        estado = gpio_get_level(PIN_ENTRADA); // Leer estado del pin
        vTaskDelay(pdMS_TO_TICKS(50));  // Esperar 1 segundo
    }
    
}
void processKey (void * argumentos){ //NO FUNCIONA
    while (1) {
        if (estado == 1){ 
        vTaskSuspend(taskHandle1);
        printf("El valor de cuenta es: suspendido");
        } 
        else{
            vTaskResume(taskHandle1); // Reanuda Task1
        }

        vTaskDelay(pdMS_TO_TICKS(100));  // Esperar 1 segundo
    }

}

void Blinking(void * argumentos) {
    gpio_set_direction(LED_VERDE, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_VERDE, 0);

    vTaskDelay(pdMS_TO_TICKS(500));
    while (1) {
        if(estado){
        gpio_set_level(LED_VERDE, 1);
        vTaskDelay(pdMS_TO_TICKS(500));
    
        gpio_set_level(LED_VERDE, 0);
        vTaskDelay(pdMS_TO_TICKS(500));
    } else {
        vTaskDelay(pdMS_TO_TICKS(100));  
    }
}
}

void app_main() {
    configLCD();
    configurar_pin_entrada();
    //xTaskCreate(leerPin, "keyboard", 2048, NULL, tskIDLE_PRIORITY+2, NULL);
   // xTaskCreate(Blinking, "Verde", 2048, NULL, tskIDLE_PRIORITY + 1, NULL);
    //xTaskCreate(processKey, "process", 2048, NULL, tskIDLE_PRIORITY+5, NULL);
    xTaskCreate(showTime, "show", 2048, NULL, tskIDLE_PRIORITY + 3, NULL);
    xTaskCreate(reloj, "reloj", 2048, NULL, tskIDLE_PRIORITY + 4, &taskHandle1);
}
