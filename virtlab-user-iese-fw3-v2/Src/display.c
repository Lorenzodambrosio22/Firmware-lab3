/*
 * acquisition.c
 *
 *  Created on: May 18, 2026
 *      Author: max
 */

#include "main.h"
#include "cmsis_os.h"
#include "io.h"
#include <stdint.h>

// Declaration of input queue, defined in main.c

extern osMessageQueueId_t displayQueueHandle;
//declaration of function that prints temperature on lcd screen
static void lcdWriteTemperature (uint16_t t_tenths_kelvin){
	uint8_t d3 = t_tenths_kelvin % 10;
	uint8_t d2 = ( t_tenths_kelvin / 10 ) % 10;
	uint8_t d1 = ( t_tenths_kelvin / 100 ) % 10;
	uint8_t d0 = ( t_tenths_kelvin / 1000 ) % 10;
	//writes the corresponding digits of the temperature on the lcd screen
	lcdWriteDigit('0' + d0, 0);
	lcdWriteDigit('0' + d1, 1);
	lcdWriteDigit('0' + d2, 2);
	lcdWriteDigit('0' + d3, 3);

	lcdUpdateDisplay();
}


void StartDisplayTask( void *argument ) {
	(void)argument;
  /* Infinite loop */
  for( ; ; ) {
    //osDelay( 1 );
	  uint16_t temperature; //temperature variable
	  osStatus status = osMessageQueueGet (displayQueueHandle, &temperature, NULL, osWaitForever); //Gets the temperature of the resistence and stores it in temperature
	  if(status==osOK){//checks if the operation "osMessageQueueGet" was successfull
		 lcdWriteTemperature(temperature);//displays the temperature on the lcd
	  }
  }
}

