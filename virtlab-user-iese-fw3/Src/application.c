/*
 * application.c
 *
 *  Created on: Oct 20, 2020
 *      Author: max
 */

#include "main.h"
#include "cmsis_os2.h"
#include "stm32l4xx_hal_tim.h"
#include "stm32l4xx_hal_tim_ex.h"
#include "io.h"

// Declare external references to timer instances

extern TIM_HandleTypeDef htim1;

// Callback used by the TIM1 on period interrupt
// Registered before starting timer1

void tim1Callback( TIM_HandleTypeDef *htim ) {
	HAL_GPIO_WritePin( IO10_GPIO_GPIO_Port, IO10_GPIO_Pin, GPIO_PIN_SET );
	HAL_GPIO_WritePin( IO10_GPIO_GPIO_Port, IO10_GPIO_Pin, GPIO_PIN_RESET );
}

// Declaration of queues, defined in main.c

extern osMessageQueueId_t acquisitionQueueHandle;
extern osMessageQueueId_t displayQueueHandle;

//aggiunta mia
extern osEventFlagsId_t acqEventFlagsHandle;

#define ACQ_FLAG_START (1U << 0)

void StartDefaultTask( void *argument ) {
	HAL_TIM_RegisterCallback(&htim1, HAL_TIM_PWM_PULSE_FINISHED_CB_ID, tim1Callback);
	while( 1 ) {
		if( getSwitch0() ) { // Blink LEDs
			while( getSwitch0() ) {
				led0( OFF );
				led1( ON );
				led2( OFF );
				led3( ON );
				osDelay( 500 );
				led0( ON );
				led1( OFF );
				led2( ON );
				led3( OFF );
				osDelay( 500 );
			}
		} else if( getSwitch1() ) { // This must be filled by students, to perform requested functions
			HAL_TIM_PWM_Start_IT(&htim1, TIM_CHANNEL_1);
			//starts the timer in PWM mode on channel 1
			while( getSwitch1() ){
					uint16_t temperature;
					osStatus_t status = osMessageQueueGet(acquisitionQueueHandle, &temperature, NULL, osWaitForever);
					if(status == osOK){
						osMessageQueuePut(displayQueueHandle, &temperature, 0U, 0U);
					}
				}
				HAL_TIM_PWM_Stop_IT(&htim1, TIM_CHANNEL_1);

			}
		}
}

