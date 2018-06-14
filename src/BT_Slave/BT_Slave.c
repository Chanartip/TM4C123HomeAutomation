/***************************************************************************
    TM4C123g Development Board Pins used
        Nokia5110 (PA2,3,5,6,7, 3.3V, GND)
            - PA2 SCLK  (SSI0)
            - PA3 SCE   (SSI0)
            - PA5 DN(MOSI) (SSI0)
            - PA6 D/C   (GPIO)
            - PA7 Reset (GPIO)
        KeyPad (PC4-7, PD0-3)
            - PC4-7 ROW (GPIO) Input w/ PUR
            - PD0-3 COL (GPIO) Output
        Bluetooth HC-05 (PB0,1, 5V, GND)
            - PB0 BT_TX (UART1_RX)
            - PB1 BT_RX (UART1_TX)
        PIR Sensor (PA4, 5V, GND)
            - PA4 Data_In (GPIO)
        POT 
            -
            
***************************************************************************/

/***************************************************************************
    Includes, defines, prototypes, global variables.
***************************************************************************/
#include "../lib/tm4c123gh6pm.h"
#include "../lib/PLL.h"
#include "../lib/UART.h"
#include "../lib/PWM.h"

#define HALL_PIR  (*((volatile unsigned long *)0x40024004))       // PE0
#define BATH_PIR  (*((volatile unsigned long *)0x40024008))       // PE1

#define HALLWAY   0x0001
#define BATHROOM  0x0002

void DisableInterrupts(void);       // Disable interrupts
void EnableInterrupts(void);        // Enable interrupts
void WaitForInterrupt(void);        // low power mode
void PIR_Init(void);                // PIR sensor init
void SysTick_Init(unsigned long);   // Systick Interrupt Init

unsigned int device;
unsigned int bathroom_brightness;
unsigned int hallway_brightness;

/*
    Initialization
*/
void PIR_Init(void){ volatile unsigned long delay;
  SYSCTL_RCGC2_R |= SYSCTL_RCGC2_GPIOE; // activate port E
  delay = SYSCTL_RCGC2_R;               // allow time to finish activating
  GPIO_PORTE_AFSEL_R &= ~0x03;          // disable alt funct on PE0,1
  GPIO_PORTE_PCTL_R  &= ~0x000000FF;    // GPIO PE0,1
  GPIO_PORTE_AMSEL_R &= ~0x03;          // disable analog func. PE0,1
  GPIO_PORTE_DIR_R   &= ~0x03;          // make PE0,1 in
  GPIO_PORTE_DEN_R   |=  0x03;          // enable digital I/O on PE0,1
    
  // Set PE0 as an interrupt (Edge Intertupt)
  GPIO_PORTE_IS_R    &= ~0x03;          // 0 as Edge Interrupt
  GPIO_PORTE_IBE_R   |=  0x03;          // 1 as both Edge Interrupt
  GPIO_PORTE_IM_R    |=  0x03;          // arm interrupt on PE0,2
  NVIC_PRI1_R = (NVIC_PRI1_R & ~0x000000E0) | 0x0000000C; // priority 6 
  NVIC_EN0_R |= 0x00000010;             // Enable PortE Interrupt Enable Register
}

/*
 * SysTick Init
 *      initializes system timer with Interrupt Enable for
 *      utilizing Systick_Handler as an Interrupt Service Routine(ISR)
 */
void SysTick_Init(unsigned long period) {
    NVIC_ST_CTRL_R = 0;         // disable SysTick during setup
    NVIC_ST_RELOAD_R = period-1;// reload value
    NVIC_ST_CURRENT_R = 0;      // any write to current clears it
    NVIC_SYS_PRI3_R = (NVIC_SYS_PRI3_R&0x00FFFFFF)|0xC0000000; // priority 6
    // enable SysTick with core clock and interrupts
    NVIC_ST_CTRL_R = NVIC_ST_CTRL_ENABLE+NVIC_ST_CTRL_CLK_SRC+NVIC_ST_CTRL_INTEN;
}

void GPIOPortE_Handler(void){
    if((GPIO_PORTE_RIS_R & 0x01) == 0x01){
        GPIO_PORTE_ICR_R |= 0x01;           // Acknowledge PE0  
        
        if((device&HALLWAY)!=HALLWAY){      // if HALLWAY is off.
            if(HALL_PIR == 0x01) {
                UART1_OutChar('%');         // Indicate Master that HALLWAY is on.
                M0PWM0_Duty(hallway_brightness);// Assign current Brightness
            }
            else{
                UART1_OutChar('_');         // Indicate Master that HALLWAY is off.
                M0PWM0_Duty(2);             // Turn off PWM
            }
        }
        
    }
    
    if((GPIO_PORTE_RIS_R & 0x02) == 0x02){
        GPIO_PORTE_ICR_R |=  0x02;           // Acknowledge PE2   
        
        if((device&BATHROOM)!=BATHROOM){    // if BATHROOM is off.
            
            if(BATH_PIR == 0x02) {
                UART1_OutChar('$');         // Indicate Master that BATHROOM is on.
                M0PWM2_Duty(bathroom_brightness);// Assign current Brightness
            }
            else{
                UART1_OutChar('-');         // Indicate Master that BATHROOM is off.
                M0PWM2_Duty(2);             // Turn off PWM
            }
        }
    }
}
/***************************************************************************
 * Interrupts, ISR
 *      - Consistently check Bluetooth input at 30Hz and take action
 *          based on the Bluetooth input; updating devices status
 *          and adjusting brightness.
 ***************************************************************************/
void SysTick_Handler(void){

    static char bt_data;
    static char key,prev_key;
    
    // Save the previous key
    prev_key = key;
    
    // Check input from Bluetooth
    //  
    bt_data = UART1_NonBlockingInChar();
    if(bt_data != 0) {
        key = bt_data;
        UART0_OutChar(bt_data);
    }
    else{
        key = 0;
    }
    
    // Take action when there is a change on the key 
    if(prev_key != key){
        switch(key){
            case '0':{ 
                device  &= ~HALLWAY;    // Turn off HALLWAY
                M0PWM0_Duty(2);         // Low the PWM
                break; }
            case '1': {
                device  |= HALLWAY;     // Turn on HALLWAY
                M0PWM0_Duty(hallway_brightness);// Assign PWM
                break;
            }
            case '2':{
                device  &= ~BATHROOM;   // Turn off BATHROOM
                M0PWM2_Duty(2);         // Low the PWM
                break;
            }
            case '3': {
                device  |= BATHROOM;    // Turn on BATHROOM
                M0PWM2_Duty(bathroom_brightness);// Assign PWM
                break;
            }
            case 'A':{
                // Increment PWM duty but within it's period
                if(hallway_brightness+3500 > 50000){
                    hallway_brightness = 49999;
                }
                else hallway_brightness += 3500;
                
                // Updating Brightness 
                M0PWM0_Duty(hallway_brightness);
                break;
            }
            case 'B':{
                // Decrement PWM duty but within it's period
                if(hallway_brightness-3500 < 3500){
                    hallway_brightness = 3500;
                }
                else hallway_brightness -= 3500;
                
                // Updating Brightness 
                M0PWM0_Duty(hallway_brightness);
                break;
            }
            case 'C':{
                // Increment PWM duty but within it's period
                if(bathroom_brightness+3500 > 50000){
                    bathroom_brightness = 49999;
                }
                else bathroom_brightness += 3500;
                
                // Updating Brightness 
                M0PWM2_Duty(bathroom_brightness);
                break;
            }
            case 'D':{
                // Decrement PWM duty but within it's period
                if(bathroom_brightness-3500 < 3500){
                    bathroom_brightness = 3500;
                }
                else bathroom_brightness -= 3500;
                
                // Updating Brightness 
                M0PWM2_Duty(bathroom_brightness);
                break;
            }
            case '#':{
                // Turn all off signal from Master
                device &= ~(HALLWAY|BATHROOM);
                M0PWM0_Duty(2); // Low the light
                M0PWM2_Duty(2); // Low the light
                break;
            }
        }
    }
}


/***************************************************************************
 Main function / loop.
***************************************************************************/
int main( void ) {

    PLL_Init();                 // 50MHz
    UART0_Init();               // UART0 (microUSB port)
    UART1_Init();               // UART1 (PB0(RX) to TX pin, PB1(TX) to RX pin)
    PIR_Init();                 // PIR sensor init
    M0PWM0_M0PM2_Init(50000,2); // PWM for Bathroom and Hallway init 
    SysTick_Init( 1666666 );    // 30Hz Systick Interrupt 
    EnableInterrupts();
    
    UART0_OutString(">>> Welcome to Serial Terminal <<<\r\n"); 
    hallway_brightness = bathroom_brightness = 40000;
    UART1_OutChar('@');         // Indicate Master as it's just Turn on.
    
    while(1) {
        WaitForInterrupt();
    } //end while
} //end main

