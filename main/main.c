#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <queue.h>
// #include "./pins.h"
#include "pico/stdlib.h"
#include <stdio.h>
#include "hardware/uart.h"
#include "hardware/adc.h"

#include "hardware/gpio.h"

#define BTN_PIN_ENTER 15
#define BTN_PIN_ESC 14
#define BTN_PIN_GIRAR 13
#define BTN_PIN_PEGAR 12
#define SENSOR_PIN 7
/* Semaphores */
SemaphoreHandle_t xSemaphoreLuz;
QueueHandle_t xQueueADC;
QueueHandle_t xQueueBtn;
QueueHandle_t xQueueData;

void btn_callback(uint gpio, uint32_t events) {
    if (events == 0x04) {

        if (gpio == BTN_PIN_ENTER) {
            int btn = 7;
            xQueueSendFromISR(xQueueBtn, &btn, 0);
        }
        if (gpio == BTN_PIN_ESC) {
            int btn = 8;
            xQueueSendFromISR(xQueueBtn, &btn, 0);
        }
        if (gpio == BTN_PIN_GIRAR) {
            int btn = 6;
            xQueueSendFromISR(xQueueBtn, &btn, 0);
        }
        if (gpio == BTN_PIN_PEGAR) {
            int btn = 9;
            xQueueSendFromISR(xQueueBtn, &btn, 0);
        }
    }
}

void buttons_init() {
    gpio_init(BTN_PIN_ENTER);
    gpio_init(BTN_PIN_ESC);
    gpio_init(BTN_PIN_GIRAR);
    gpio_init(BTN_PIN_PEGAR);

    gpio_set_dir(BTN_PIN_ENTER, GPIO_IN);
    gpio_set_dir(BTN_PIN_ESC, GPIO_IN);
    gpio_set_dir(BTN_PIN_GIRAR, GPIO_IN);
    gpio_set_dir(BTN_PIN_PEGAR, GPIO_IN);

    gpio_pull_up(BTN_PIN_ENTER);
    gpio_pull_up(BTN_PIN_ESC);
    gpio_pull_up(BTN_PIN_GIRAR);
    gpio_pull_up(BTN_PIN_PEGAR);

    gpio_set_irq_enabled_with_callback(BTN_PIN_ENTER, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &btn_callback);
    gpio_set_irq_enabled(BTN_PIN_ESC, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true);
    gpio_set_irq_enabled(BTN_PIN_GIRAR, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true);
    gpio_set_irq_enabled(BTN_PIN_PEGAR, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true);
}
/* Task function */
void input_task(void *pvParameters) {
    while ((1)) {
        /* code */
        int ang;
        int ang1;
        if (xQueueReceive(xQueueBtn, &ang, pdMS_TO_TICKS(50))) {
            printf("botao %d\n", ang);
        }
        if (xQueueReceive(xQueueADC, &ang1, pdMS_TO_TICKS(50))) {
            printf("seta %d\n", ang1);
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
void x_task(void *p) {

    adc_init();
    adc_gpio_init(27);
    int data_0 = 0;
    int data_1 = 0;
    int data_2 = 0;
    int data_3 = 0;
    // const float conversion_factor = 3.3f / (1 << 12);

    while (true) {
        adc_select_input(1);
        int result = adc_read();
        int result_4 = result + data_0 + data_1 + data_2 + data_3;
        result_4 /= 5;
        data_0 = data_1;
        data_1 = data_2;
        data_2 = data_3;
        data_3 = result_4;
        result_4 -= 2047;
        result_4 /= 8;
        if (result_4 > 60) {
            int pos = 4;
            xQueueSend(xQueueADC, &pos, pdMS_TO_TICKS(50));

        } else if (result_4 < -60) {
            int pos = 5;
            xQueueSend(xQueueADC, &pos, pdMS_TO_TICKS(50));
        }
        // printf("eixo x: %d\n", result);
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void y_task(void *p) {
    adc_init();
    // const float conversion_factor = 3.3f / (1 << 12);
    adc_gpio_init(26);
    int data_0 = 0;
    int data_1 = 0;
    int data_2 = 0;
    int data_3 = 0;
    while (true) {

        adc_select_input(0);
        int result = adc_read();
        int result_4 = result + data_0 + data_1 + data_2 + data_3;
        result_4 /= 5;
        data_0 = data_1;
        data_1 = data_2;
        data_2 = data_3;
        data_3 = result_4;
        result_4 -= 2047;
        result_4 /= 8;

        if (result_4 > 60) {
            int pos = 2;
            xQueueSend(xQueueADC, &pos, pdMS_TO_TICKS(50));

        } else if (result_4 < -60) {
            int pos = 3;
            xQueueSend(xQueueADC, &pos, pdMS_TO_TICKS(50));
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void sensor_task(void *p) {
    adc_init();
    // const float conversion_factor = 3.3f / (1 << 12);
    adc_gpio_init(28);
    int data_0 = 0;
    int data_1 = 0;
    int data_2 = 0;
    int data_3 = 0;
    while (1) {
        adc_select_input(2);
        int luz = adc_read();
        int luz_1 = luz + data_0 + data_1 + data_2 + data_3;
        luz_1 /= 5;
        data_0 = data_1;
        data_1 = data_2;
        data_2 = data_3;
        data_3 = luz_1;

        luz_1 -= 3330;
        printf("luz: %d\n", luz_1);
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
int main(void) {
    stdio_init_all();
    buttons_init();
    xSemaphoreLuz = xSemaphoreCreateBinary();
    xQueueADC = xQueueCreate(1, sizeof(int));
    xQueueBtn = xQueueCreate(32, sizeof(int));
    xQueueData = xQueueCreate(32, sizeof(int));

    // xTaskCreate(input_task, "input", configMINIMAL_STACK_SIZE, NULL, 1, NULL);
    // xTaskCreate(x_task, "x", configMINIMAL_STACK_SIZE, NULL, 1, NULL);
    // xTaskCreate(y_task, "y", configMINIMAL_STACK_SIZE, NULL, 1, NULL);
    xTaskCreate(sensor_task, "sensor", configMINIMAL_STACK_SIZE, NULL, 1, NULL);

    vTaskStartScheduler();

    // Should never reach here
    for (;;)
        ;
}
