#include "LPC17xx.h"                    // Device header
#include "PIN_LPC17xx.h"                // Keil::Device:PIN
#include "Board_LED.h"                  // ::Board Support:LED
#include "Open1768_LCD.h"
#include "LCD_ILI9325.h"
#include "asciiLib.h"
#include "I2C.h"

#include <stdio.h>
#include <string.h>

const int passcodeLength = 7;

// ram mock data
char ramPasscode[passcodeLength];
int ramAutolockTime;

// config data
typedef struct{
  char passcode[passcodeLength];
  int autolockTime;
} configData;

// logs
const int logsCount = 5;
const int logsStringLength = 13;
char logs[logsCount][logsStringLength];

volatile configData currentConfigData;

// keypad variables
unsigned int keypadColumnPins[4] = {16, 8, 10, 0};
unsigned int kaypadRowPins[4] = {17, 15, 9, 4};
char keypadKeys[4][4] = {
  {'1', '4', '7', '0'},
  {'2', '5', '8', 'F'},
  {'3', '6', '9', 'E'},
  {'A', 'B', 'C' ,'D'}
};

// logic variables
int keyPrevoiuslyWasPressed = 0;
char enteredPasscode[passcodeLength];
int enteredPasscodeIndex = 0;
int isLocked = 1;

//lcd variables
int firstPasscodeSign_x = 100;
int firstPasscodeSign_y = 100;
int currentHour_x = 180;
int currentHour_y = 30;
int padlock_x = 5;
int padlock_y = 5;
int signsSpacing = 10;
uint16_t signsColor = LCDBlack;
uint16_t signsBackgroundColor = LCDWhite;
int firstLogEntry_x = 200;
int firstLogEntry_y = 200;
const int timeToClearPasscode = 5;


// init
void Initialisation();
void LateInitialisation();
void keypad_Init();
void UART_Init();
void LCDInit();
void TIM0Init();
void TIM1Init();
void RTCInit();

// debug/utils
void sendString(char* string);
void my_sleep(int time);
void UART_SendChar(char c);
void sendDec(int number);

// passcode
void successfullyEnteredPasscode();
void wronglyEnteredPasscode();
int fillNextEnteredPasscode(char pressedKey, char displayedKey);
void clearEnteredPasscode();
int validatePasscode();
void logEntry();
void clearLogs();

// keypad
char scanKeypad();
void managePressedKey();

// i2c/fram
configData readConfigurationFromRAM();
void writeConfigurationToRam();

// LCD
void drawUI();
void drawCurrentHour(char* hourMinutes);
void drawOpenedPadlock();
void drawColsedPadlock();
void drawPasscodeSign(int position, char sign);
void drawPasscode();
void clearDisplayedPasscode();

void drawPixel(uint16_t x, uint16_t y, uint16_t Color);
void drawFilledSquare();
void setWindowAdressFunction(int x1, int y1, int x2, int y2);
void resetWindowAdressFunction();
void writeLetterInPosition(char letter, int x1, int y1, uint16_t LetterColor, uint16_t BackgroundColor);

void showLogs();
void hideLogs();

// RTC
int getHours();
int getMinutes();
int getSeconds();

// ext int 0
void EINT0_IRQhandler();
void EXTINTInit();

// systick
void SysTick_Handler(void);

int main(){
	Initialisation();

	clearLogs();
  currentConfigData = readConfigurationFromRAM();
	drawFilledSquare();
	
	clearDisplayedPasscode();
	
	LateInitialisation();

	while(1){
    managePressedKey();
	}
}

void Initialisation(){
  keypad_Init();
	LED_Initialize();
  UART_Init();
	lcdConfiguration();
	init_ILI9325();
	LCDInit();
	RTCInit();
  EXTINTInit();
	SysTick_Config(SystemCoreClock / 1000);

	LED_Off(2);
	LED_Off(3);
}

void LateInitialisation(){
	TIM0Init();
	TIM1Init();
}

void keypad_Init(){
	for(int i = 0; i < 4; i++){
		LPC_GPIO1->FIODIR |= 1<<keypadColumnPins[i];
	}
}

void UART_Init(){
	PIN_Configure(0, 2, 1, PIN_PINMODE_TRISTATE, 0);
	PIN_Configure(0, 3, 1, PIN_PINMODE_TRISTATE, 0);
	LPC_UART0->LCR = (1<<7) | 3;
	LPC_UART0->DLL = 13;
	LPC_UART0->DLM = 0;
	LPC_UART0->FDR = 1 | (15<<4);
	LPC_UART0->LCR = 3;
}

void LCDInit(){
	PIN_Configure(0, 19, 0, PIN_PINMODE_PULLUP, 0);	// raczej nie potrzebne
	LPC_GPIOINT->IO0IntEnF = 1<<19;	// wlaczenie generowania przerwania przy dotyku
	NVIC_EnableIRQ(EINT3_IRQn);	// wlaczenie przerwania z tego portu
	lcdWriteReg(0x03, 0x1030 ^ (1<<4 | 1<<5));
}

void TIM0Init(){
	LPC_TIM0->PR = 0;
	LPC_TIM0->MR0 = (SystemCoreClock/4 * 5) - 1;
	LPC_TIM0->MCR |= 3; //ustaw generowanie przerwania dla mr0
	NVIC_EnableIRQ(TIMER0_IRQn);
}

void TIM1Init(){
	LPC_TIM1->PR = 0;
	LPC_TIM1->MR0 = (SystemCoreClock/4 * timeToClearPasscode) - 1;
	LPC_TIM1->MCR |= 3; //ustaw generowanie przerwania dla mr0
	NVIC_EnableIRQ(TIMER1_IRQn);
}

void EXTINTInit()
{
	PIN_Configure(2, 10, 1, PIN_PINMODE_PULLUP, 0);	// pullup, bo przycisk zwiera z masa, wiec robi zero, wiec jak chcemy miec rozroznienie, to musimy dac stan wysoki jak nie ma zwarcia
	LPC_SC->EXTMODE = 1;	// ustawienie przerwania zboczem
	LPC_SC->EXTPOLAR = 0;	// ustawienie zboczem opadajacym
	NVIC_EnableIRQ(EINT0_IRQn);
	LPC_SC->EXTINT = 1;
}

int getHours(){
	int mask = 0b11111;
	return mask & (LPC_RTC->CTIME0 >> 16);
}

int getMinutes(){
	int mask = 0b111111;
	return mask & (LPC_RTC->CTIME0 >> 8);
}

int getSeconds(){
	int mask = 0b111111;
	return mask & LPC_RTC->CTIME0;
}

void RTCInit(){
	LPC_RTC->CCR = 1; // wlacz RTC
	LPC_RTC->ILR = 1; // wlaczenie generowania przerwan
	LPC_RTC->CIIR = 1; // przerwanie co sekunde
	NVIC_EnableIRQ(RTC_IRQn);
}

void sendString(char* string){
	int t = 0;
	while (string[t] != '\0')
	{
		if(LPC_UART0->LSR & 1<<5)
		{
			LPC_UART0->THR = string[t];
			t++;
		}
	}
}

void my_sleep(int time){
	for(int i = 0; i < time; i++);
}

void successfullyEnteredPasscode(){
  sendString("Good passcode entered");
	LED_On(2);

	LPC_TIM0->TC = 0;
	LPC_TIM0->TCR |= 1; //wlacz Timer
	logEntry(1);
}

void wronglyEnteredPasscode(){
  sendString("Wrong passcode entered");
	logEntry(0);
}

int fillNextEnteredPasscode(char pressedKey, char displayedKey){
  enteredPasscode[enteredPasscodeIndex] = pressedKey;
	drawPasscodeSign(enteredPasscodeIndex, displayedKey);
	enteredPasscodeIndex++;
  if(enteredPasscodeIndex >= passcodeLength){
    return 1;
  }
  return 0;
}

void clearEnteredPasscode(){
	enteredPasscodeIndex = 0;
}

void logEntry(int successfull){
	char buffer[logsStringLength];
	sprintf(buffer, "%c - %02d:%02d:%02d", successfull ? 'P' : 'N', getHours(), getMinutes(), getSeconds());
	for(int i = 1; i < logsCount; i++){
		for(int j = 0; j < logsStringLength; j++){
			logs[i-1][j] = logs[i][j];
		}
	}
	for(int i = 0; i < logsStringLength; i++){
		UART_SendChar(logs[logsCount-1][i]);
		logs[logsCount-1][i] = buffer[i];
	}
}

void clearLogs(){
	
	for(int i = 0; i < logsCount; i++){
		for(int j = 0; j < logsStringLength - 1; j++){
			logs[i][j] = ' ';
		}

		logs[i][logsStringLength - 1] = '\0';
	}
}

void clearDisplayedPasscode(){
	for(int i = 0; i < passcodeLength; i++){
		drawPasscodeSign(i, '_');
	}
}

int validatePasscode(){
  for(int i = 0; i < passcodeLength; i++){
    if(currentConfigData.passcode[i] != enteredPasscode[i]){
      return 0;
    }
  }
  return 1;
}

char scanKeypad(){
  for(int i = 0; i < 4; i++){
    LPC_GPIO1->FIOPIN |= 1 << keypadColumnPins[i];
  }
  
  for(int i = 0; i < 4; i++){
    LPC_GPIO1->FIOCLR = 1 << keypadColumnPins[i];
    
    for(int j = 0; j < 4; j++){
      if(((LPC_GPIO1->FIOPIN >> kaypadRowPins[j]) & 1) == 0)
        return keypadKeys[i][j];
    }
    LPC_GPIO1->FIOPIN |= 1 << keypadColumnPins[i];
  }
  return 0;
}

void UART_SendChar(char c){
	while(!(LPC_UART0 -> LSR & 1<<5)) {}
	LPC_UART0 -> THR = c;
}

void sendDec(int number){
	char buffer[50];
	sprintf(buffer, "%d", number);
	sendString(buffer);
}

void managePressedKey(){
  char pressedKey = scanKeypad();
	
	if(pressedKey){
		LPC_TIM1->TC = 0;
		LPC_TIM1->TCR |= 1; //wlacz Timer
	}
	
	my_sleep(100000);

  if(!pressedKey){
    keyPrevoiuslyWasPressed = 0;
		return;
  }
	
	if(keyPrevoiuslyWasPressed){
		return;
	}
	
	if(pressedKey == 'A' || pressedKey == 'B' || pressedKey == 'C' || pressedKey == 'D'){
		if(pressedKey == 'A'){
			showLogs();
		}
		if(pressedKey == 'B'){
			hideLogs();
		}
		return;
	}

	
  int wasLastKey = fillNextEnteredPasscode(pressedKey, '*');

  if(wasLastKey){
    if(validatePasscode()){
      successfullyEnteredPasscode();
    }
    else {
      wronglyEnteredPasscode();
    }
    clearEnteredPasscode();
		clearDisplayedPasscode();
  }

  keyPrevoiuslyWasPressed = 1;
}

configData readConfigurationFromRAM(){
  configData data;

	I2C_Read(0b10100000, 0, (uint8_t*)data.passcode, passcodeLength);
	
  data.autolockTime = 4;
	return data;
}

void drawPixel(uint16_t x, uint16_t y, uint16_t Color){
	lcdWriteReg(ADRX_RAM, x);
	lcdWriteReg(ADRY_RAM, y);
	lcdWriteReg(DATA_RAM, Color);
}

void writeConfigurationToRam(){
	I2C_Write(0b10100000, 0, (uint8_t*)currentConfigData.passcode, passcodeLength);
}

void drawFilledSquare(){
	lcdWriteReg(HADRPOS_RAM_START, 0);
	lcdWriteReg(HADRPOS_RAM_END, 240);
	lcdWriteReg(VADRPOS_RAM_START, 0);
	lcdWriteReg(VADRPOS_RAM_END, 320);
	
	lcdSetCursor(0, 0);
	lcdWriteReg(DATA_RAM, LCDWhite);

	for (int i = 0; i < (321*241 - 1); i++){
		lcdWriteData(LCDWhite);
	}
}

void setWindowAdressFunction(int x1, int y1, int x2, int y2){
	lcdWriteReg(HADRPOS_RAM_START, x1);
	lcdWriteReg(HADRPOS_RAM_END, x2 - 1);
	lcdWriteReg(VADRPOS_RAM_START, y1);
	lcdWriteReg(VADRPOS_RAM_END, y2 - 1);
	lcdSetCursor(x1, y1);
}

void resetWindowAdressFunction(){
	setWindowAdressFunction(0, 0, 240, 320);
}

void writeLetterInPosition(char letter, int x1, int y1, uint16_t LetterColor, uint16_t BackgroundColor){
	unsigned char buffer[16];
	GetASCIICode(0, buffer, letter);
	setWindowAdressFunction(x1, y1, x1 + 16, y1 + 8);
	lcdWriteReg(DATA_RAM, BackgroundColor);
	for(int x = 0; x < 8; x++)
		for(int y = 0; y < 16; y++)
		{
			if(buffer[y] & 1<<x){
				lcdWriteData(LetterColor);
			}
			else{
				lcdWriteData(BackgroundColor);
			}
		}
}

void writeString(char* string, int x1, int y1, uint16_t LetterColor, uint16_t BackgroundColor){
	int i = 0;
	while(*string != '\0'){
		writeLetterInPosition(*string, x1, y1 + 8 * i, LetterColor, BackgroundColor);
		i++;
		string++;
	}
}


void drawUI(){

}

void drawCurrentHour(char* hourMinutes){
	writeString(hourMinutes, currentHour_x, currentHour_y, signsColor, signsBackgroundColor);
  resetWindowAdressFunction();
}

void drawOpenedPadlock(){
}

void drawColsedPadlock(){
}

void drawPasscodeSign(int position, char sign){
  writeLetterInPosition(sign, firstPasscodeSign_x, firstPasscodeSign_y + position * (16), signsColor, signsBackgroundColor);
	resetWindowAdressFunction();
}

void drawPasscode(){
  writeString(enteredPasscode, firstPasscodeSign_x, firstPasscodeSign_y, signsColor, signsBackgroundColor);
}

void showLogs(){
	for(int i = 0; i < logsCount; i++){
			writeString(logs[i], firstLogEntry_x - i * 16, firstLogEntry_y, signsColor, signsBackgroundColor);
	}
}

void hideLogs(){
	char clearString[logsStringLength];
	for(int i = 0; i < logsStringLength - 1; i++){
		clearString[i] = ' ';
	}
	clearString[logsStringLength - 1] = '\0';
	
	for(int i = 0; i < logsCount; i++){
			writeString(clearString, firstLogEntry_x - i * 16, firstLogEntry_y, signsColor, signsBackgroundColor);
	}
}

void TIMER0_IRQHandler(void){
	LED_Off(2);
	LPC_TIM0->IR = 1; // usun sygnal przerwania
}

void TIMER1_IRQHandler(void){
	clearEnteredPasscode();
	clearDisplayedPasscode();
	LPC_TIM1->IR = 1; // usun sygnal przerwania
}


void RTC_IRQHandler(void){
	char buffer[50];
	sprintf(buffer, "%02d:%02d:%02d", getHours(), getMinutes(), getSeconds());
	drawCurrentHour(buffer);
	LPC_RTC->ILR = 1;	// czyszczenie informacji o przerwaniu
}

void EINT0_IRQHandler(void)
{
	clearEnteredPasscode();
	clearDisplayedPasscode();
	
  int finishedInsertingNewPasscode = 0;
  while(!finishedInsertingNewPasscode){
    char pressedKey = scanKeypad();
    my_sleep(10000);

    if(!pressedKey){
      keyPrevoiuslyWasPressed = 0;
      continue;
    }
    
    if(keyPrevoiuslyWasPressed){
      continue;
    }
    
    if(pressedKey == 'A' || pressedKey == 'B' || pressedKey == 'C' || pressedKey == 'D'){
      continue;
    }
		
    int wasLastKey = fillNextEnteredPasscode(pressedKey, pressedKey);

    if(wasLastKey){
      for(int i = 0; i < passcodeLength; i++){
        currentConfigData.passcode[i] = enteredPasscode[i];
      }
			clearEnteredPasscode();
      clearDisplayedPasscode();
			finishedInsertingNewPasscode = 1;
    }
    keyPrevoiuslyWasPressed = 1;
  }
	
	writeConfigurationToRam();
	
	my_sleep(10000000);
	LPC_SC->EXTINT = 1;
}

void SysTick_Handler(void){
	if (msTicksI2C){
		msTicksI2C--;
	}
}