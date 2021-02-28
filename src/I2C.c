#include "I2C.h"

volatile uint32_t msTicksI2C = 0;

uint8_t I2C_CheckForStatus(uint8_t expectedStatus)
{
	uint8_t state = 0xFF;
	msTicksI2C = 100;
	while((state != expectedStatus) && msTicksI2C)
		state = LPC_I2C2->I2STAT;
	return (state == expectedStatus) ? 0 : state;
}

void I2C_InitWrite()
{
	PIN_Configure(0, 10, 2, PIN_PINMODE_TRISTATE, PIN_PINMODE_OPENDRAIN);
	PIN_Configure(0, 11, 2, PIN_PINMODE_TRISTATE, PIN_PINMODE_OPENDRAIN);
	LPC_I2C2->I2SCLH = 125;
	LPC_I2C2->I2SCLL = 125;
	LPC_I2C2->I2CONSET = I2C_ENABLE;
}

void I2C_Stop()
{
	LPC_I2C2 -> I2CONSET = I2C_STOP;
	LPC_I2C2 -> I2CONCLR = I2C_START | I2C_SEND;
}

uint8_t I2C_Write(uint8_t slaveAddress, uint8_t registerAddress, uint8_t* dataToSend, int dataSize)
{
	I2C_InitWrite();
	slaveAddress &= 0xFE;
	uint8_t status;
	
	LPC_I2C2 -> I2CONSET = I2C_START;
	status = I2C_CheckForStatus(0x08);
	if(status)
	{
		I2C_Stop();
		return status;
	}

	LPC_I2C2 -> I2DAT = slaveAddress;
	LPC_I2C2 -> I2CONCLR = I2C_START | I2C_SEND;
	status = I2C_CheckForStatus(0x18);
	if(status)
	{
		I2C_Stop();
		return status;
	}

	LPC_I2C2 -> I2DAT = registerAddress;
	LPC_I2C2 -> I2CONCLR = I2C_SEND;
	status = I2C_CheckForStatus(0x28);
	if(status)
	{
		I2C_Stop();
		return status;
	}
	for (int i = 0; i < dataSize; i++)
	{
		LPC_I2C2 -> I2DAT = dataToSend[i];
		LPC_I2C2 -> I2CONCLR = I2C_SEND;
		status = I2C_CheckForStatus(0x28);
		if (status)
		{
			I2C_Stop();
			return status;
		}
	}
	I2C_Stop();
	return 0;
}


uint8_t I2C_Read(uint8_t slaveAddress, uint8_t registerAddress, uint8_t* buffer, int dataSize)
{
	I2C_InitWrite();
	uint8_t status;
	
	////////////////////////
	
	for(int i = 0; i<dataSize; i++)
	{
		LPC_I2C2 -> I2CONSET = I2C_START;
		status = I2C_CheckForStatus(0x08);
		if(status)
		{
			I2C_Stop();
			return status;
		}
		LPC_I2C2 -> I2DAT = slaveAddress & 0xFE;
		LPC_I2C2 -> I2CONCLR = I2C_START | I2C_SEND;
		
		////////////////////////
		
		status = I2C_CheckForStatus(0x18);
		if (status)
		{
			I2C_Stop();
			return status;
		}
		
		LPC_I2C2 -> I2DAT = registerAddress+i;
		LPC_I2C2 -> I2CONCLR = I2C_SEND;
		status = I2C_CheckForStatus(0x28);
		if (status) //blad
		{
			return status;
		}

		LPC_I2C2 -> I2CONSET = I2C_START;
		LPC_I2C2 -> I2CONCLR = I2C_SEND;
		status = I2C_CheckForStatus(0x10);
		if (status) //blad
		{
			I2C_Stop();
			return status;
		}
		
		LPC_I2C2 -> I2DAT = slaveAddress | 0x01;
		LPC_I2C2 -> I2CONCLR = I2C_SEND | I2C_START;
		status = I2C_CheckForStatus(0x40);
		if (status) //blad
		{
			I2C_Stop();
			return status;
		}
		LPC_I2C2 -> I2CONCLR = I2C_START | I2C_SEND;
		
		for(int j=0; j<3500; j++){}
		
		LPC_I2C2 -> I2CONSET = I2C_RECEIVE;
		LPC_I2C2 -> I2CONCLR = I2C_START | I2C_STOP | I2C_SEND | I2C_RECEIVE;
		buffer[i] = LPC_I2C2 -> I2DAT;
		status = I2C_CheckForStatus(0x58);
		if (status)
		{
			I2C_Stop();
			return status;
		}
		LPC_I2C2 -> I2CONCLR = I2C_RECEIVE | I2C_SEND | I2C_START | I2C_STOP;
		
	//	for (int i=0; i<dataSize; i++)
	//	{
	//		LPC_I2C2 -> I2CONSET = I2C_RECEIVE;
	//		status = I2C_CheckForStatus(0x58);
	//		if (status)
	//		{
	//			UART_SendString("[I2C] ERROR 0x58\n\r");
	//			I2C_Stop();
	//			return status;
	//		}
	//		buffer[i] = LPC_I2C2 -> I2DAT;
	//		UART_SendChar(buffer[i]);
	//	}
	//	LPC_I2C2 -> I2CONCLR = I2C_RECEIVE | I2C_SEND | I2C_START | I2C_STOP;
		
		I2C_Stop();
	}
	return 0;
}
