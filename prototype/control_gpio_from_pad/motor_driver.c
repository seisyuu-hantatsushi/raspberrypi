/*
  motor driver
  (C) 2018 TANIGUCHI, Kazushige
 */


#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <pigpiod_if2.h>

#include "motor_driver.h"

// GPIO BCM 6  -> STBY
// GPIO BCM 13 -> PWMA
// GPIO BCM 19 -> AIN1
// GPIO BCM 26  > AIN2

#define STBY_PIN (6)
#define PWMA_PIN (13)
#define AIN1_PIN (19)
#define AIN2_PIN (26)

#define PWMB_PIN (12)
#define BIN1_PIN (16)
#define BIN2_PIN (20)

#define PMW_FREQ (100) // 100Hz

struct MotorDriverContext{
	int gpioHandlerId;
};

int32_t motorDriverMotorAccel(MotorDriverHandler handler, uint32_t motorid, int16_t accel){
	static const unsigned int  pinAssign[2][3] = {
		{PWMA_PIN, AIN1_PIN, AIN2_PIN},
		{PWMB_PIN, BIN1_PIN, BIN2_PIN}
	};
	struct MotorDriverContext *pCtx =
		(struct MotorDriverContext *)handler;
	unsigned int in1,in2,duty;
	int ret;

	//printf("motorDriverMotorAccel(%u,%d)\n", motorid, accel);
	if(accel > 0){
		//forward
		in1 = 1;
		in2 = 0;
	}
	else if(accel == 0){
		//short stop
		in1 = 1;
		in2 = 1;
	}
	else {
		//reverse
		in1 = 0;
		in2 = 1;
	}

	// decide duty
	if(accel < 0){
		duty = (unsigned int)(((uint64_t)(-1*accel)*500000ULL)/(uint64_t)SHRT_MAX);
	}
	else {
		duty = (unsigned int)(((uint64_t)accel*500000ULL)/(uint64_t)SHRT_MAX);
	}

	//printf("duty=%u\n",duty);
	gpio_write(pCtx->gpioHandlerId, pinAssign[motorid][1], in1);
	gpio_write(pCtx->gpioHandlerId, pinAssign[motorid][2], in2);

	hardware_PWM(pCtx->gpioHandlerId, pinAssign[motorid][0], PMW_FREQ, duty);
	return 0;
}


int32_t motorDriverCreate(MotorDriverHandler *pHandler){
	struct MotorDriverContext *pNewCtx;
	int newGpioHandlerId = -1;

	newGpioHandlerId = pigpio_start(NULL, NULL);
	if(newGpioHandlerId < 0){
		fprintf(stderr, "failed to create pigpio handler\n");
		return -1;
	}

	pNewCtx = malloc(sizeof(*pNewCtx));
	if(pNewCtx == NULL){
		fprintf(stderr, "out of memory\n");
		pigpio_stop(newGpioHandlerId);
		return -1;
	}
	pNewCtx->gpioHandlerId = newGpioHandlerId;

	// initialize GPIO setting

	set_mode(newGpioHandlerId, STBY_PIN, PI_OUTPUT);
	gpio_write(newGpioHandlerId, STBY_PIN, 0);

	set_mode(newGpioHandlerId, AIN1_PIN, PI_OUTPUT);
	gpio_write(newGpioHandlerId, AIN1_PIN, 0);

	set_mode(newGpioHandlerId, AIN2_PIN, PI_OUTPUT);
	gpio_write(newGpioHandlerId, AIN2_PIN, 0);

	set_mode(newGpioHandlerId, BIN1_PIN, PI_OUTPUT);
	gpio_write(newGpioHandlerId, BIN1_PIN, 0);

	set_mode(newGpioHandlerId, BIN2_PIN, PI_OUTPUT);
	gpio_write(newGpioHandlerId, BIN2_PIN, 0);

	// UP STBY_PIN
	gpio_write(newGpioHandlerId, STBY_PIN, 1);

	// setup hardware pwm frequency
	set_mode(newGpioHandlerId, PWMA_PIN, PI_ALT0);
	hardware_PWM(newGpioHandlerId, PWMA_PIN, PMW_FREQ, 0);
	set_mode(newGpioHandlerId, PWMB_PIN, PI_ALT0);
	hardware_PWM(newGpioHandlerId, PWMB_PIN, PMW_FREQ, 0);

	*pHandler = pNewCtx;

	return 0;
}

int32_t motorDriverDestroy(MotorDriverHandler handler){
	struct MotorDriverContext *pCtx =
		(struct MotorDriverContext *)handler;

	if(pCtx == NULL){
		fprintf(stderr, "invalid handler\n");
		return -1;
	}

	pigpio_stop(pCtx->gpioHandlerId);

	free(pCtx);

	return 0;
}
