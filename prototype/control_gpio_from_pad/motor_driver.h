/*
  motor driver
  (C) 2018 TANIGUCHI, Kazushige
 */
#ifndef _MOTOR_DRIVER_H_
#define _MOTOR_DRIVER_H_

#include <stdint.h>

struct MotorDriverContext;

typedef struct MotorDriverContext *MotorDriverHandler;

int32_t motorDriverMotorAccel(MotorDriverHandler handler, uint32_t motorid, int16_t accel);

int32_t motorDriverCreate(MotorDriverHandler *pHandler);
int32_t motorDriverDestroy(MotorDriverHandler handler);

#endif  /* _MOTOR_DRIVER_H_ */
