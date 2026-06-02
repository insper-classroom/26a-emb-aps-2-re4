#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <queue.h>
#include "pico/stdlib.h"
#include <stdio.h>
#include "hardware/uart.h"
#include "hardware/adc.h"
#include "hardware/gpio.h"
#include "pins.h"
#include "hc06.h"

#define BTN_PIN_ENTER 15
#define BTN_PIN_ESC 14
#define BTN_PIN_GIRAR 13
#define BTN_PIN_PEGAR 12
#define SENSOR_PIN 7
#define buzz 11

#define UART_ID uart0
#define BAUD_RATE 115200
#define CORE_0 (1 << 0)
#define CORE_1 (1 << 1)

#define SENSOR_PIN_1 2
#define X_PIN 3
#define Y_PIN 4
#define INPUT_PIN 5

/* Semaphores */
SemaphoreHandle_t xSemaphoreLuz;
QueueHandle_t xQueueADC;
QueueHandle_t xQueueBtn;
QueueHandle_t xQueueData;
QueueHandle_t xQueueTX;

typedef struct {
    int axis;
    int val;
} adc_t;

void uart_rx_handler() {
    uart_getc(HC06_UART_ID);
    // xSemaphoreGiveFromISR(xSemaphoreConnection, 0);
}
void btn_callback(uint gpio, uint32_t events) {
    if (events == 0x08) {

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

void vibra() {
    gpio_put(buzz, 1);
    vTaskDelay(pdMS_TO_TICKS(150));
    gpio_put(buzz, 0);
}

void init_uart_irq() {
    // Turn off FIFO's - we want to do this character by character
    uart_set_fifo_enabled(HC06_UART_ID, false);

    // Set up a RX interrupt
    // We need to set up the handler first
    // Select correct interrupt for the UART we are using
    int UART_IRQ = HC06_UART_ID == uart0 ? UART0_IRQ : UART1_IRQ;

    // And set up and enable the interrupt handlers
    irq_set_exclusive_handler(UART_IRQ, uart_rx_handler);
    irq_set_enabled(UART_IRQ, true);

    // Now enable the UART to send interrupts - RX only
    uart_set_irq_enables(HC06_UART_ID, true, false);
}

void buttons_init() {
    gpio_init(BTN_PIN_ENTER);
    gpio_init(BTN_PIN_ESC);
    gpio_init(BTN_PIN_GIRAR);
    gpio_init(BTN_PIN_PEGAR);
    gpio_init(buzz);

    gpio_set_dir(BTN_PIN_ENTER, GPIO_IN);
    gpio_set_dir(BTN_PIN_ESC, GPIO_IN);
    gpio_set_dir(BTN_PIN_GIRAR, GPIO_IN);
    gpio_set_dir(BTN_PIN_PEGAR, GPIO_IN);
    gpio_set_dir(buzz, GPIO_OUT);

    gpio_pull_up(BTN_PIN_ENTER);
    gpio_pull_up(BTN_PIN_ESC);
    gpio_pull_up(BTN_PIN_GIRAR);
    gpio_pull_up(BTN_PIN_PEGAR);

    gpio_set_irq_enabled_with_callback(BTN_PIN_ENTER, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &btn_callback);
    gpio_set_irq_enabled(BTN_PIN_ESC, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true);
    gpio_set_irq_enabled(BTN_PIN_GIRAR, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true);
    gpio_set_irq_enabled(BTN_PIN_PEGAR, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true);
}

static void tx_task(void *p) {
    adc_t data;

    while (true) {
        if (xQueueReceive(xQueueTX, &data, portMAX_DELAY) == pdTRUE) {
            uart_putc_raw(HC06_UART_ID, data.axis);
            uart_putc_raw(HC06_UART_ID, data.val);
            uart_putc_raw(HC06_UART_ID, data.val >> 8);
            uart_putc_raw(HC06_UART_ID, -1);
        }
        vTaskDelay(pdMS_TO_TICKS(15));
    }
}

/* Task function */
void input_task(void *pvParameters) {
    int ligado = 0;
    gpio_init(INPUT_PIN);
    gpio_set_dir(INPUT_PIN, GPIO_OUT);

    uart_init(UART_ID, BAUD_RATE);

    gpio_set_function(0, GPIO_FUNC_UART); // TX
    gpio_set_function(1, GPIO_FUNC_UART); // RX
    while ((1)) {
        /* code */
        gpio_put(INPUT_PIN, 1);
        int ang;
        adc_t joystick;
        if (xSemaphoreTake(xSemaphoreLuz, pdMS_TO_TICKS(50))) {
            if (!ligado) {
                int valor = 42;
                uart_putc(HC06_UART_ID, -1);
                uart_putc(HC06_UART_ID, 1);
                uart_putc(HC06_UART_ID, valor);
                uart_putc(HC06_UART_ID, (valor >> 8));
                ligado = 1;
                vibra();
            }
            if (xQueueReceive(xQueueBtn, &ang, pdMS_TO_TICKS(50))) {
                // printf("botao %d\n", ang);
                int valor = 42;
                uart_putc(HC06_UART_ID, -1);
                uart_putc(HC06_UART_ID, ang);
                uart_putc(HC06_UART_ID, valor);
                uart_putc(HC06_UART_ID, (valor >> 8));
                vibra();
                vTaskDelay(pdMS_TO_TICKS(500));
            } else if (xQueueReceive(xQueueADC, &joystick, pdMS_TO_TICKS(50))) {
                // printf("seta %d\n", joystick.axis);
                // printf("%d \n", joystick.val);
                uart_putc(HC06_UART_ID, -1);
                uart_putc(HC06_UART_ID, joystick.axis);
                uart_putc(HC06_UART_ID, joystick.val);
                uart_putc(HC06_UART_ID, (joystick.val >> 8));
                vTaskDelay(pdMS_TO_TICKS(500));
            }
        } else {
            ligado = 0;
        }
        gpio_put(INPUT_PIN, 0);
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
    gpio_init(X_PIN);
    gpio_set_dir(X_PIN, GPIO_OUT);
    while (true) {
        gpio_put(X_PIN, 1);
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
        if (result_4 > 100) {
            adc_t pos;
            pos.axis = 4;
            pos.val = result_4;
            // printf("aaa %d\n",pos.val);
            xQueueSend(xQueueADC, &pos, pdMS_TO_TICKS(30));
        }
        if (result_4 < -100) {
            adc_t pos;
            pos.axis = 5;
            pos.val = result_4;
            // printf("aaa %d\n",pos.val);
            xQueueSend(xQueueADC, &pos, pdMS_TO_TICKS(30));
        }
        // printf("eixo x: %d\n", result);
        gpio_put(X_PIN, 0);
        vTaskDelay(pdMS_TO_TICKS(50));
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
    gpio_init(Y_PIN);
    gpio_set_dir(Y_PIN, GPIO_OUT);
    while (true) {
        gpio_put(Y_PIN, 1);
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
        if (result_4 > 100) {
            adc_t pos;
            pos.axis = 2;
            pos.val = result_4;
            xQueueSend(xQueueADC, &pos, pdMS_TO_TICKS(30));
        }
        if (result_4 < -100) {
            adc_t pos;
            pos.axis = 3;
            pos.val = result_4;
            xQueueSend(xQueueADC, &pos, pdMS_TO_TICKS(30));
        }
        gpio_put(Y_PIN, 0);
        vTaskDelay(pdMS_TO_TICKS(50));
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
    gpio_init(SENSOR_PIN_1);
    gpio_set_dir(SENSOR_PIN_1, GPIO_OUT);
    while (1) {
        gpio_put(SENSOR_PIN_1, 1);
        adc_select_input(2);
        int luz = adc_read();
        int luz_1 = luz + data_0 + data_1 + data_2 + data_3;
        luz_1 /= 5;
        data_0 = data_1;
        data_1 = data_2;
        data_2 = data_3;
        data_3 = luz_1;

        luz_1 -= 3330;
        if (luz_1 > 200) {
            // printf("luz: %d\n", luz_1);
            xSemaphoreGive(xSemaphoreLuz);
        }
        gpio_put(SENSOR_PIN_1, 0);
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void init_uart_hc06(void) {
    uart_init(HC06_UART_ID, HC06_BAUD_RATE);

    gpio_set_function(HC06_TX_PIN, UART_FUNCSEL_NUM(HC06_UART_ID, HC06_TX_PIN));
    gpio_set_function(HC06_RX_PIN, UART_FUNCSEL_NUM(HC06_UART_ID, HC06_RX_PIN));

    int __unused actual = uart_set_baudrate(HC06_UART_ID, HC06_BAUD_RATE);

    uart_set_hw_flow(HC06_UART_ID, false, false);

    uart_set_format(HC06_UART_ID, 8, 1, UART_PARITY_NONE);
}

int main(void) {
    stdio_init_all();
    buttons_init();
    init_uart_hc06();
    init_uart_irq();
    hc06_config("MARIO", "3425");

    xSemaphoreLuz = xSemaphoreCreateBinary();
    xQueueADC = xQueueCreate(1, sizeof(adc_t));
    xQueueBtn = xQueueCreate(32, sizeof(int));
    xQueueData = xQueueCreate(64, sizeof(adc_t));
    xQueueTX = xQueueCreate(32, sizeof(adc_t));

    TaskHandle_t xHandle_input;
    TaskHandle_t xHandle_x_task;
    TaskHandle_t xHandle_y_task;
    TaskHandle_t xHandle_sensor;
    // TaskHandle_t xHandle_bluetooth;

    xTaskCreate(input_task, "input", configMINIMAL_STACK_SIZE, NULL, 1, &(xHandle_input));
    xTaskCreate(x_task, "x", configMINIMAL_STACK_SIZE, NULL, 1, &(xHandle_x_task));
    xTaskCreate(y_task, "y", configMINIMAL_STACK_SIZE, NULL, 1, &(xHandle_y_task));
    xTaskCreate(sensor_task, "sensor", configMINIMAL_STACK_SIZE, NULL, 1, &(xHandle_sensor));
    // xTaskCreate(bluetooth_task, "Bluetooth task", configMINIMAL_STACK_SIZE, NULL, 1, &(xHandle_bluetooth));

    vTaskCoreAffinitySet(xHandle_input, CORE_0);
    // vTaskCoreAffinitySet(xHandle_bluetooth, CORE_0);
    vTaskCoreAffinitySet(xHandle_sensor, CORE_1);
    vTaskCoreAffinitySet(xHandle_x_task, CORE_1);
    vTaskCoreAffinitySet(xHandle_y_task, CORE_1);
    vTaskStartScheduler();

    // Should never reach here
    for (;;)
        ;
}
