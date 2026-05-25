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

// Design choice: we opted for a lookup table (LUT) because otherwise the linear
// formula wouldn't be accurate around 50 C. The NTC is exponential
// (R(T) = 10000 * 0.96^(T-25), -4% per degree from 10k at 25 C), so the previous
// linear approximation (kept commented below) was only valid near 25 C and never
// reached 50 C. The exact inverse needs a logarithm, which would require floating
// point; a precomputed integer table avoids floats and stays accurate over 0..80 C.
//
// NTC lookup table: resistance (ohm) vs temperature (tenths of Kelvin), computed
// offline from R(T) = 10000 * 0.96^(T-25). Resistance is strictly decreasing.
// Integer only (no float at runtime).
static const int32_t ntcLutR[]    = {27744,22624,18447,15041,12264,10000,8154,6648,
                                     5420,4420,3604,2939,2396,1954,1594,1299,1060};
static const int32_t ntcLutTk10[] = { 2731, 2781, 2831, 2881, 2931, 2981,3031,3081,
                                     3131,3181,3231,3281,3331,3381,3431,3481,3531};
#define NTC_LUT_N (sizeof(ntcLutR)/sizeof(ntcLutR[0]))

// Convert an NTC resistance (ohm) into temperature (tenths of Kelvin) by piecewise
// linear interpolation over the table above. Out-of-range values are clamped.
static int32_t ntcResistanceToKelvinX10(int32_t r) {
	// Clamp values outside the table range
	if (r >= ntcLutR[0]) {
		return ntcLutTk10[0];             // colder than the first table point
	}
	if (r <= ntcLutR[NTC_LUT_N-1]) {
		return ntcLutTk10[NTC_LUT_N-1];   // hotter than the last table point
	}

	// Find the bracket [i, i+1] such that ntcLutR[i] >= r >= ntcLutR[i+1]
	uint32_t i = 0;
	while (i < NTC_LUT_N-1 && ntcLutR[i+1] > r) {
		i++;
	}

	// Read the two bracketing points
	int32_t rHi = ntcLutR[i];
	int32_t rLo = ntcLutR[i+1];
	int32_t tHi = ntcLutTk10[i];
	int32_t tLo = ntcLutTk10[i+1];

	// Linear interpolation between the two points (integer math)
	return tHi + (tLo - tHi) * (rHi - r) / (rHi - rLo);
}


// Acquisition task entry point

//all RTOS tasks must receive a generic pointer as a parameter
void StartAcquisitionTask( void *argument ) {
	acqEventFlagsHandle = osEventFlagsNew(NULL);
	//create a new eventsflag object

	// Calibrate both ADCs once, while they are still stopped (required on STM32L4
	// for accurate conversions). Single-ended inputs.
	HAL_ADCEx_Calibration_Start(&hadc1, ADC_SINGLE_ENDED);
	HAL_ADCEx_Calibration_Start(&hadc2, ADC_SINGLE_ENDED);

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

	// --- OLD linear approximation (only valid near 25 C, kept for reference) ---
	// int32_t t_celsius_x10 = 250 + ((int32_t)R0_NTC - r_ntc) * NTC_COEFF_INV_X10 / (int32_t)R0_NTC;
	// //Tref + (R0-R_NTC)*inverso_coefficiente / R0 scaled x10
	// int32_t t_kelvin_x10 = t_celsius_x10 + KELVIN_X10;
	// --- NEW: accurate over the whole range via the NTC lookup table ---
	// (temperature already in tenths of Kelvin)
    int32_t t_kelvin_x10 = ntcResistanceToKelvinX10(r_ntc);

    //check value
    if(t_kelvin_x10 <= 0 || t_kelvin_x10 > 65535)
    	continue;
    uint16_t t_encoded = (uint16_t)t_kelvin_x10;

    //push in queue
    osMessageQueuePut(acquisitionQueueHandle, &t_encoded, 0U, 0U);

  }
}

