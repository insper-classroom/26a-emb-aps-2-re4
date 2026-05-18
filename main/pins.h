#ifndef PINS_H
#define PINS_H

#define BTN_PIN_ENTER 4
#define BTN_PIN_ESC 5
#define BTN_PIN_GIRAR 6
#define BTN_PIN_PEGAR 7
#define SENSOR_PIN 8


void btn_callback(uint gpio, uint32_t events);


void x_task(void *p);
void y_task(void *p);
void input_task(void *p);


#endif