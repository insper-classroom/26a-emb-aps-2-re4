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

#define UART_ID uart0
#define BAUD_RATE 115200
#define CORE_0 (1 << 0)
#define CORE_1 (1 << 1)

// Nome - MARIO
// PIN - 4269

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
    gpio_put(BUZZER, 1);
    vTaskDelay(pdMS_TO_TICKS(150));
    gpio_put(BUZZER, 0);
}

void buttons_init() {
    gpio_init(BTN_PIN_ENTER);
    gpio_init(BTN_PIN_ESC);
    gpio_init(BTN_PIN_GIRAR);
    gpio_init(BTN_PIN_PEGAR);
    gpio_init(BUZZER);

    gpio_set_dir(BTN_PIN_ENTER, GPIO_IN);
    gpio_set_dir(BTN_PIN_ESC, GPIO_IN);
    gpio_set_dir(BTN_PIN_GIRAR, GPIO_IN);
    gpio_set_dir(BTN_PIN_PEGAR, GPIO_IN);
    gpio_set_dir(BUZZER, GPIO_OUT);

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
    gpio_init(tx_task_pin);
    gpio_set_dir(tx_task_pin, GPIO_OUT);
    while (true) {
        gpio_put(tx_task_pin, 1);
        if (xQueueReceive(xQueueTX, &data, portMAX_DELAY) == pdTRUE) {
            uart_putc_raw(HC06_UART_ID, data.axis);
            uart_putc_raw(HC06_UART_ID, data.val);
            uart_putc_raw(HC06_UART_ID, data.val >> 8);
            uart_putc_raw(HC06_UART_ID, -1);
        }
        gpio_put(tx_task_pin, 0);

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
                uart_putc_raw(HC06_UART_ID, -1);
                uart_putc_raw(HC06_UART_ID, 1);
                uart_putc_raw(HC06_UART_ID, valor);
                uart_putc_raw(HC06_UART_ID, (valor >> 8));
                ligado = 1;
                vibra();
            }
            if (xQueueReceive(xQueueBtn, &ang, pdMS_TO_TICKS(50))) {
                int valor = 42;
                uart_putc_raw(HC06_UART_ID, -1);
                uart_putc_raw(HC06_UART_ID, ang);
                uart_putc_raw(HC06_UART_ID, valor);
                uart_putc_raw(HC06_UART_ID, (valor >> 8));
                vibra();
                vTaskDelay(pdMS_TO_TICKS(500));
            } else if (xQueueReceive(xQueueADC, &joystick, pdMS_TO_TICKS(50))) {
                uart_putc_raw(HC06_UART_ID, -1);
                uart_putc_raw(HC06_UART_ID, joystick.axis);
                uart_putc_raw(HC06_UART_ID, joystick.val);
                uart_putc_raw(HC06_UART_ID, (joystick.val >> 8));
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
        if (result_4 > 70) {
            adc_t pos;
            pos.axis = 4;
            pos.val = result_4;
            xQueueSend(xQueueADC, &pos, pdMS_TO_TICKS(30));
        }
        if (result_4 < -70) {
            adc_t pos;
            pos.axis = 5;
            pos.val = result_4;
            xQueueSend(xQueueADC, &pos, pdMS_TO_TICKS(30));
        }
        gpio_put(X_PIN, 0);
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void y_task(void *p) {
    adc_init();
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
        if (result_4 > 70) {
            adc_t pos;
            pos.axis = 2;
            pos.val = result_4;
            xQueueSend(xQueueADC, &pos, pdMS_TO_TICKS(30));
        }
        if (result_4 < -70) {
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
            xSemaphoreGive(xSemaphoreLuz);
        }
        gpio_put(SENSOR_PIN_1, 0);
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

// void stack_monitor_task(void *p) {
//     static TaskStatus_t tasks[16];
//     while (true) {
//         vTaskDelay(pdMS_TO_TICKS(2000));
//         UBaseType_t n = uxTaskGetSystemState(tasks, 16, NULL);
//         printf("+------------------+-------+\n");
//         printf("| %-16s | %5s |\n", "task", "free");
//         printf("+------------------+-------+\n");
//         for (UBaseType_t i = 0; i < n; i++) {
//             printf("| %-16s | %5u |\n",
//                    tasks[i].pcTaskName,
//                    (unsigned)tasks[i].usStackHighWaterMark);
//         }
//         printf("+------------------+-------+\n");
//         printf("| heap livre min   | %5u |\n",
//                (unsigned)xPortGetMinimumEverFreeHeapSize());
//         printf("+------------------+-------+\n\n");
//     }
// }


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

    xSemaphoreLuz = xSemaphoreCreateBinary();
    xQueueADC = xQueueCreate(1, sizeof(adc_t));
    xQueueBtn = xQueueCreate(32, sizeof(int));
    xQueueData = xQueueCreate(64, sizeof(adc_t));
    xQueueTX = xQueueCreate(32, sizeof(adc_t));

    TaskHandle_t xHandle_input;
    TaskHandle_t xHandle_x_task;
    TaskHandle_t xHandle_y_task;
    TaskHandle_t xHandle_sensor;

    xTaskCreate(input_task, "input", configMINIMAL_STACK_SIZE, NULL, 1, &(xHandle_input));
    xTaskCreate(x_task, "x", configMINIMAL_STACK_SIZE, NULL, 1, &(xHandle_x_task));
    xTaskCreate(y_task, "y", configMINIMAL_STACK_SIZE, NULL, 1, &(xHandle_y_task));
    xTaskCreate(sensor_task, "sensor", configMINIMAL_STACK_SIZE, NULL, 1, &(xHandle_sensor));
    // xTaskCreate(stack_monitor_task, "stackk 1", 4096, NULL, 1, NULL);


    vTaskCoreAffinitySet(xHandle_input, CORE_0);
    vTaskCoreAffinitySet(xHandle_sensor, CORE_1);
    vTaskCoreAffinitySet(xHandle_x_task, CORE_1);
    vTaskCoreAffinitySet(xHandle_y_task, CORE_1);
    vTaskStartScheduler();
    // Should never reach here
    for (;;)
        ;
}
