#ifndef I2C
#define I2C

#include "LPC17xx.h"                    // Device header
#include "PIN_LPC17xx.h"                // Keil::Device:PIN
#include "Board_LED.h"                  // ::Board Support:LED

#define I2C_ENABLE  (1<<6)
#define I2C_START   (1<<5)
#define I2C_STOP    (1<<4)
#define I2C_SEND    (1<<3)
#define I2C_RECEIVE (1<<2)

extern volatile uint32_t msTicksI2C;

uint8_t I2C_CheckForStatus(uint8_t);
void I2C_InitWrite();
void I2C_Stop();
uint8_t I2C_Write(uint8_t slaveAddress, uint8_t registerAddress, uint8_t* dataToSend, int dataSize);
uint8_t I2C_Read(uint8_t slaveAddress, uint8_t registerAddress, uint8_t* buffer, int dataSize);

#endif