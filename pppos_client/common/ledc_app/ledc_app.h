#ifndef LEDC_APP_H
#define LEDC_APP_H
#include "stdint.h"

void ledc_init(void);
void ledc_channel_pwm(int8_t channel, int8_t gpio_num);
void ledc_set_duty_t(int8_t duty,int8_t channel);
#endif
