// PWM.c
// Runs on TM4C123
// Use PWM0/PB6 and PWM1/PB7 to generate pulse-width modulated outputs.
// Daniel Valvano
// March 28, 2014

/* This example accompanies the book
   "Embedded Systems: Real Time Interfacing to ARM Cortex M Microcontrollers",
   ISBN: 978-1463590154, Jonathan Valvano, copyright (c) 2014
  Program 6.7, section 6.3.2

 Copyright 2014 by Jonathan W. Valvano, valvano@mail.utexas.edu
    You may use, edit, run or distribute this file
    as long as the above copyright notice remains
 THIS SOFTWARE IS PROVIDED "AS IS".  NO WARRANTIES, WHETHER EXPRESS, IMPLIED
 OR STATUTORY, INCLUDING, BUT NOT LIMITED TO, IMPLIED WARRANTIES OF
 MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE APPLY TO THIS SOFTWARE.
 VALVANO SHALL NOT, IN ANY CIRCUMSTANCES, BE LIABLE FOR SPECIAL, INCIDENTAL,
 OR CONSEQUENTIAL DAMAGES, FOR ANY REASON WHATSOEVER.
 For more information about my classes, my research, and my books, see
 http://users.ece.utexas.edu/~valvano/
 */
#include <stdint.h>
#include "PWM.h"
#include "tm4c123gh6pm.h"
#define PWM_0_GENA_ACTCMPAD_ONE 0x000000C0  // Set the output signal to 1
#define PWM_0_GENA_ACTLOAD_ZERO 0x00000008  // Set the output signal to 0
#define PWM_0_GENB_ACTCMPBD_ONE 0x00000C00  // Set the output signal to 1
#define PWM_0_GENB_ACTLOAD_ZERO 0x00000008  // Set the output signal to 0

#define SYSCTL_RCC_USEPWMDIV    0x00100000  // Enable PWM Clock Divisor
#define SYSCTL_RCC_PWMDIV_M     0x000E0000  // PWM Unit Clock Divisor
#define SYSCTL_RCC_PWMDIV_2     0x00000000  // /2


// period is 16-bit number of PWM clock cycles in one period (3<=period)
// period for PA6 and PA7 must be the same
// duty is number of PWM clock cycles output is high  (2<=duty<=period-1)
// PWM clock rate = processor clock rate/SYSCTL_RCC_PWMDIV
//                = BusClock/2 
//                = 50 MHz/2 = 25 MHz 
void M0PWM0_M0PM2_Init(uint16_t period, uint16_t duty){
  SYSCTL_RCGCPWM_R |= 0x01;             // 1) activate PWM0
  SYSCTL_RCGCGPIO_R |= 0x02;            // 2) activate port B
  while((SYSCTL_PRGPIO_R&0x02) == 0){};
  GPIO_PORTB_AFSEL_R |= 0x50;           // enable alt funct on PB4,6
  GPIO_PORTB_PCTL_R &= ~0x0F0F0000;     // configure PB4,6 as PWM0
  GPIO_PORTB_PCTL_R |= 0x04040000;
  GPIO_PORTB_AMSEL_R &= ~0x50;          // disable analog functionality on PB4,6
  GPIO_PORTB_DEN_R |= 0x50;             // enable digital I/O on PB4,6
      
  SYSCTL_RCC_R = 0x00100000 |           // 3) use PWM divider
      (SYSCTL_RCC_R & (~0x000E0000));   //    configure for /2 divider
  PWM0_0_CTL_R = 0;                     // 4) re-loading down-counting mode
  PWM0_1_CTL_R = 0;                     // 4) re-loading down-counting mode
      
  PWM0_0_GENA_R = 0xC8;                 // low on LOAD, high on CMPA down
  PWM0_1_GENA_R = 0xC8;                 // low on LOAD, high on CMPA down 
  PWM0_0_LOAD_R = period - 1;           // 5) cycles needed to count down to 0
  PWM0_1_LOAD_R = period - 1;           // 5) cycles needed to count down to 0
  PWM0_0_CMPA_R = duty - 1;             // 6) count value when output rises
  PWM0_1_CMPA_R = duty - 1;             // 6) count value when output rises
      
  PWM0_0_CTL_R  |= 0x00000001;          // 7) start PWM0
  PWM0_1_CTL_R  |= 0x00000001;          // 7) start PWM0
  PWM0_ENABLE_R |= PWM_ENABLE_PWM0EN | PWM_ENABLE_PWM2EN; // enable PB6,4/M0PWM0,2
}
// change duty cycle of PA6
// duty is number of PWM clock cycles output is high  (2<=duty<=period-1)
void M0PWM0_Duty(uint16_t duty){
  PWM0_0_CMPA_R = duty - 1;             // 6) count value when output rises
}
// change duty cycle of PA7
// duty is number of PWM clock cycles output is high  (2<=duty<=period-1)
void M0PWM2_Duty(uint16_t duty){
  PWM0_1_CMPA_R = duty - 1;             // 6) count value when output rises
}

