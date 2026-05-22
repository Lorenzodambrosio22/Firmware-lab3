/*
 * acquisition.c
 *
 *  Created on: May 18, 2026
 *      Author: max
 */

#include "main.h"
#include "cmsis_os.h"
#include <stdint.h>

// Declaration of output queue, defined in main.c
//extern meas the variable was declared in another .c file

extern osMessageQueueId_t acquisitionQueueHandle;

//reads the AIN4 pin connected to the 10k/10k fixed divider
extern ADC_HandleTypeDef hadc1;	//read the AIN3 pin connected to the NTC divider
extern ADC_HandleTypeDef hadc2; //legge il pin AIN4 connesso al partitore fisso 10k/10k

osEventFlagsId_t acqEventFlagsHandle;

//costante x preprocessore
#define ACQ_FLAG_START (1U << 0)	//sets bit 0 as the flag to trigger the ADC sampling
// (<<0) rapresents the left shift operation
//physical costants
#define R0_NTC		10000	//nominal resistence of the NTC at 25 degrees celcius
#define KELVIN_X10  2731	//celcius kelvin conversion constant, ten times larger for the decimal rappresentation
#define ADC_TIMEOUT_MS 10	//timeout
#define ADC_MAX_COUNT ((1U << 12) - 1U) //(2^12-1)=4095
#define NTC_COEFF_INV_X10 250 // (1/0.04) *10, this constant makes temperatures changes easily computable


// Acquisition task entry point

//all RTOS tasks must receive a generic pointer as a parameter
void StartAcquisitionTask( void *argument ) {
	acqEventFlagsHandle = osEventFlagsNew(NULL);
	//create a new eventsflag object
  /* Infinite loop */
/*all the RTOS tasks must be infinite loops, if a task returned
*	the RTOS would encounter an error*/
  for( ; ; ) {
    //osDelay( 1 );		??
	  (void)argument;  /* unused, silence compiler warning */

    osEventFlagsWait(acqEventFlagsHandle, ACQ_FLAG_START, osFlagsWaitAny, osWaitForever);

	//ADC1 read
    if( HAL_ADC_Start(&hadc1) != HAL_OK){
    	continue;
    }
    if( HAL_ADC_PollForConversion(&hadc1, ADC_TIMEOUT_MS) != HAL_OK) {
    	HAL_ADC_Stop(&hadc1);
    	continue;
    }
    uint32_t adc1 = HAL_ADC_GetValue(&hadc1);
    HAL_ADC_Stop(&hadc1);

    //ADC2 read
    if( HAL_ADC_Start(&hadc2) != HAL_OK){
        	continue;
        }
        if( HAL_ADC_PollForConversion(&hadc2, ADC_TIMEOUT_MS) != HAL_OK) {
        	HAL_ADC_Stop(&hadc2);
        	continue;
        }
        uint32_t adc2 = HAL_ADC_GetValue(&hadc2);
        HAL_ADC_Stop(&hadc2);

    //Denominator computation
    int32_t denom = (int32_t)(2u * adc2) - (int32_t)adc1;
    //protezione da circuito aperto o cortocircuito
    if (denom <= 0)
    	continue;

    //NTC computation
    int32_t r_ntc = (int32_t)R0_NTC * (int32_t)adc1 / denom;

	//calculates the temperature in celcius decimals
    int32_t t_celsius_x10 = 250 + ((int32_t)R0_NTC - r_ntc) * NTC_COEFF_INV_X10 / (int32_t)R0_NTC;
    //Tref + (R0-R_NTC)*inverso_coefficiente / R0 scaled x10
	//conversion in kelvin decimals
    int32_t t_kelvin_x10 = t_celsius_x10 + KELVIN_X10;

    //check value
    if(t_kelvin_x10 <= 0 || t_kelvin_x10 > 65535)
    	continue;
    uint16_t t_encoded = (uint16_t)t_kelvin_x10;

    //push in queue
    osMessageQueuePut(acquisitionQueueHandle, &t_encoded, 0U, 0U);

  }
}

