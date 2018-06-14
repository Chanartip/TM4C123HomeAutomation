/************************************************************************
* Name:     Chanartip Soonthornwan
* Email:    Chanartip.Soonthornwan@gmail.com
* Filename: BT_Master.c
* Project:  HomeAutomation via Bluetooth Part 1
*************************************************************************/
/*
    TM4C123G pins used
    PA2,3,5,7   -   Nokia5110 (SSI0)
    PB0,1       -   Bluetooth module HC-05 (UART1)
    PC4,5,6,7   -   Keypad Colomn
    PD0,1,2,3   -   Keypad Row
    PE2,3,4,5   -   output to Transistors or Relays
    
 */
#include "../lib/PLL.h"
#include "../lib/tm4c123gh6pm.h"
#include "../lib/UART.h"
#include "../lib/Nokia5110.h"
#include "../lib/Timer.h"

#define RELAY1  (*((volatile unsigned long *)0x40024010)) // PE2
#define RELAY2  (*((volatile unsigned long *)0x40024020)) // PE3
#define BUZZER  (*((volatile unsigned long *)0x40024040)) // PE4
#define FANPIN  (*((volatile unsigned long *)0x40024080)) // PE5
    
#define SPEAKER  0x0001
#define DESK1    0x0002
#define DESK2    0x0004
#define LAMP     0x0008
#define POLE     0x0010
#define DESK3    0x0020
#define RELAY3   0x0040
#define RELAY4   0x0080
#define FAN      0x0100
#define HALLWAY  0x0200
#define BATHROOM 0x0400
#define ALLOFF   0x0800
#define PWM_UP   0x1000
#define PWM_DN   0x2000
#define BTN_C    0x4000
#define BTN_D    0x8000

void DisableInterrupts(void);       // Disable interrupts
void EnableInterrupts(void);        // Enable interrupts
void WaitForInterrupt(void);        // low power mode
void PortE_Init(void);              // Relays & Buzzer Init
void SysTick_Init(unsigned long);   // Systick Init
void Nokia_Task(void);              // Task for Nokia5110 at 60Hz
char ReadKey(void);                 // Reading input from Keypad
void Keypad_Init(void);             // 4x4 Keypad Init

unsigned long SoundTime;            // Timer for sound
static unsigned int device;        // a Register holding device flags

// PortC and PortD Initialization
// PC 4,5,6,7 are Keypad's column 1,2,3,4 as outputs
// PD 0,1,2,3 are Keypad's row    1,2,3,4 as inputs, PUR.
void Keypad_Init(void){ volatile unsigned long delay;
  SYSCTL_RCGC2_R |= 0x00000004;      // (a) activate clock for port C
  delay = 0;                         // (b) initialize counter
  GPIO_PORTC_LOCK_R   =  0x4C4F434B; //     unlock PortC
  GPIO_PORTC_DIR_R   |=  0xF0;       // (c) make PC4-7 outputs
  GPIO_PORTC_AFSEL_R &= ~0xF0;       //     disable alt funct on PC4-7
  GPIO_PORTC_DEN_R   |=  0xF0;       //     enable digital I/O on PC4-7
  GPIO_PORTC_PCTL_R  &= ~0xFFFF0000; // configure PC4-7 as GPIO
  GPIO_PORTC_AMSEL_R &= ~0xFFFF0000; // disable analog functionality on PC4-7
	
  SYSCTL_RCGC2_R |= 0x00000008;      // (a) activate clock for port D
  delay = 0;             			 // (b) initialize counter
  GPIO_PORTD_LOCK_R   =  0x4C4F434B; //     unlock PortD
  GPIO_PORTD_DIR_R   &=  ~0x0F;  	 // (c) make PD0-3 inputs
  GPIO_PORTD_AFSEL_R &= ~0x0F;  	 //     disable alt funct on PD0-3
  GPIO_PORTD_DEN_R   |=  0x0F;  	 //     enable digital I/O on PD0-3
  GPIO_PORTD_PCTL_R  &= ~0x0000FFFF; // configure PD0-3 as GPIO
  GPIO_PORTD_AMSEL_R &= ~0x0000FFFF; // disable analog functionality on PD0-3
  GPIO_PORTD_PUR_R   |= 0x0F;       // Enable weak pull up resistors.
}

// Port for Relays and Buzzer
//  Relay 1 - PE2 (GPIO out)
//  Relay 2 - PE3 (GPIO out)
//  Buzzer  - PE4 (GPIO out)
//  Fan     - PE5 (GPIO out)
void PortE_Init(void){ volatile unsigned long delay;
  SYSCTL_RCGC2_R |= SYSCTL_RCGC2_GPIOE; // activate port E
  delay = SYSCTL_RCGC2_R;               // allow time to finish activating
  GPIO_PORTE_AFSEL_R &= ~0x3C;          // disable alt funct on PE2,3,4,5
  GPIO_PORTE_PCTL_R  &= ~0x00FFFF00;    // GPIO PE2,3,4,5
  GPIO_PORTE_AMSEL_R &= ~0x3C;          // disable analog func. PE2,3,4,5
  GPIO_PORTE_DIR_R   |=  0x3C;          // make PE2,3,4,5 out
  GPIO_PORTE_DEN_R   |=  0x3C;          // enable digital I/O on PE2,3,4,5
  GPIO_PORTE_DATA_R  &= ~0x3C;
}

// SysTick Init, period will be loaded such that the interrupts happen
// at 1ms intervals.
void SysTick_Init(unsigned long period) {
    NVIC_ST_CTRL_R = 0;         // disable SysTick during setup
    NVIC_ST_RELOAD_R = period-1;// reload value
    NVIC_ST_CURRENT_R = 0;      // any write to current clears it
    NVIC_SYS_PRI3_R = (NVIC_SYS_PRI3_R&0x00FFFFFF)|0x20000000; // priority 1
    // enable SysTick with core clock and interrupts
    NVIC_ST_CTRL_R = NVIC_ST_CTRL_ENABLE+NVIC_ST_CTRL_CLK_SRC+NVIC_ST_CTRL_INTEN;
}

/*****************************************************************
Key Read Function
*****************************************************************/

// Row    is input  [PD 0,1,2,3]
// Column is output [PC 4,5,6,7]
// Sets a column to zero then checks if any of the rows have
// been shorted to zero. By comparing the column and shorted
// row pin we can tell which key has been pressed. 
// GPIO_PORTD_DATA_R Row D In with PUR
// GPIO_PORTC_DATA_R Col C Out
#define ROW (*((volatile unsigned long *) 0x4000703C)) 
#define COL (*((volatile unsigned long *) 0x400063C0)) 
char ReadKey(void){

    // Set Columns to LOW, and check if any Row reads Low,
    //  then there is an input. Otherwise, skip checking input.
    COL &= ~0xF0;   
    if(ROW!=0x0F0){
        
        // Set COL[0] to GND
        COL |=  0xF0;
        COL &= ~0x10;
        switch(ROW){
            case 0x0E: return '1';
            case 0x0D: return '4';
            case 0x0B: return '7';
            case 0x07: return '*';
        }
        
        // Set COL[1] to GND
        COL |=  0xF0;
        COL &= ~0x20;
        switch(ROW){
            case 0x0E: return '2';
            case 0x0D: return '5';
            case 0x0B: return '8';
            case 0x07: return '0';
        }
        
        // Set COL[2] to GND
        COL |=  0xF0;
        COL &= ~0x40;
        switch(ROW){
            case 0x0E: return '3';
            case 0x0D: return '6';
            case 0x0B: return '9';
            case 0x07: return '#';
        }
        
        // Set COL[3] to GND
        COL |=  0xF0;
        COL &= ~0x80;
        switch(ROW){
            case 0x0E: return 'A';
            case 0x0D: return 'B';
            case 0x0B: return 'C';
            case 0x07: return 'D';
        }
    }
    return 0;
}

// Nokia_Task
//      - Display status of Hallway light, Buzzer, Lamp, and Humidity on
//          Nokia5110 display by utilizing Nokia_token that polls statuses 
//          and are updated at Systick_Handler
//
//  Input  - Nokia_token
//  Output - Nokia5110 LCD display through Nokia functions.
//
#define NOKIA_DEFAULT 0x00
void Nokia_Task(){
    
    //  Display default screen
    if(device == NOKIA_DEFAULT){
        Nokia5110_Clear();
        Nokia5110_OutString("___MASTER___");
        Nokia5110_SetCursor(0,1);
        Nokia5110_OutString("HALLWAY: OFF");
        Nokia5110_SetCursor(0,2);
        Nokia5110_OutString("BATHROOM:OFF");
        Nokia5110_SetCursor(0,3);
        Nokia5110_OutString("LAMP:    OFF");
        Nokia5110_SetCursor(0,4);
        Nokia5110_OutString("POLE:    OFF");
        Nokia5110_SetCursor(0,5);
        Nokia5110_OutString("FAN:     OFF");
//        Nokia5110_OutString("HUMID:  100%");
    }else{
    
        // Display Hallway light status
        if((device & HALLWAY) == HALLWAY){
            Nokia5110_SetCursor(9,1);
            Nokia5110_OutString(" ON");   
        }
        else{
            Nokia5110_SetCursor(9,1);
            Nokia5110_OutString("OFF");
        }
        
        // Display Sound status
        if((device & BATHROOM) == BATHROOM){
            Nokia5110_SetCursor(9,2);
            Nokia5110_OutString(" ON");
        }
        else{
            Nokia5110_SetCursor(9,2);
            Nokia5110_OutString("OFF");
        }
        
        // Display Lamp status
        if((device & LAMP) == LAMP){
            Nokia5110_SetCursor(9,3);
            Nokia5110_OutString(" ON");
        }
        else{
            Nokia5110_SetCursor(9,3);
            Nokia5110_OutString("OFF");
        }
        
        // Display Lamp status
        if((device & POLE) == POLE){
            Nokia5110_SetCursor(9,4);
            Nokia5110_OutString(" ON");
        }
        else{
            Nokia5110_SetCursor(9,4);
            Nokia5110_OutString("OFF");
        }
        
        // Display Fan status
        if((device & FAN) == FAN){
            Nokia5110_SetCursor(9,5);
            Nokia5110_OutString(" ON");
        }
        else{
            Nokia5110_SetCursor(9,5);
            Nokia5110_OutString("OFF");
        }
        
        // Always display Humidity
//            Nokia5110_SetCursor(6,5);
//            Nokia5110_OutUDec(100);
//            Nokia5110_OutChar('%');
    }
        
}

/***************************************************************************
    Interrupts, ISRs
        - Consistently checking input from Bluetooth and Keypad
            and then set up Devices register to operate the device
            while setting up the Nokia_token for updating the Nokia display.
***************************************************************************/
void SysTick_Handler(void){

    static char key, prev_key;   // Variable to hold current key character
    static int  select_led;      // Variable to hold last led selected.       
    char bt_in;                  // Variable to hold a character received from Bluetooth
    
    // Save previous key before receiving new key
    //  Because 'key' would be spam with null
    //  or previous key, the program would action
    //  only time the key has changed.
    prev_key = key;
    
    // Check input from Bluetooth
    //  Set key if got Bluetooth char,
    //  else key from keypad.
    bt_in = UART1_NonBlockingInChar();    
    if(bt_in != 0){
        
        // '%' - case that HALLWAY PIR to turn on
        // '_' - case that HALLWAY PIR to turn off
        // '@' - case that Slave MCU is restarted.
        switch(bt_in){
            case '%':{device |=  HALLWAY; break;}
            case '_':{device &= ~HALLWAY; break;}
            case '$':{device |=  BATHROOM; break;}
            case '-':{device &= ~BATHROOM; break;}
            case '@':{device &= ~(HALLWAY|BATHROOM); break;}    
        }
    }
    else key = ReadKey();  // Update key received from Keypad
    
    // Only action when key is different.
    //  Toggle On/Off Devices of Action according to the button
    if(prev_key != key){
        switch(key){
            case '1':{ device |= SPEAKER; SoundTime = 0; break; }
            case '2':{ device ^= DESK1;  break; }
            case '3':{ device ^= DESK2;  break; }
            case '4':{ device ^= LAMP;   RELAY1 ^= 0x04; break; }
            case '5':{ device ^= POLE;   RELAY2 ^= 0x08; break; }
            case '6':{ device ^= DESK3;  break; }
            case '7':{ device ^= RELAY3; break; }
            case '8':{ device ^= RELAY4; break; }
            case '9':{ device ^=    FAN; FANPIN ^= 0x20; break; }
            case '0':{ device ^= BATHROOM; 
                select_led = BATHROOM;
                if((device&BATHROOM)!=BATHROOM) // if BATHROOM is off
                     UART1_OutChar('2');        // send '2' to turn off BATHROOM
                else UART1_OutChar('3');        // send '3' to turn on BATHROOM
                break;
            }
            case 'A': { //PWM_UP
                // if either HALLWAY or BATHROOM is on.
                if( ((device& HALLWAY)==HALLWAY)||((device&BATHROOM)==BATHROOM))
                {
                    switch(select_led){
                        case HALLWAY: {UART1_OutChar('A'); break;}
                        case BATHROOM:{UART1_OutChar('C'); break;}
                    }
                }
                break;
            }
            case 'B': { //PWM_DN
                // if either HALLWAY or BATHROOM is on.
                if( ((device& HALLWAY)==HALLWAY)||((device&BATHROOM)==BATHROOM))
                {
                    switch(select_led){
                        case HALLWAY: {UART1_OutChar('B'); break;}
                        case BATHROOM:{UART1_OutChar('D'); break;}
                    }
                }
                break;
            }
            case 'C':{ device ^= BTN_C; break; }
            case 'D':{ device ^= BTN_D; break; }
            case '*':{ device ^= HALLWAY;
                select_led = HALLWAY;
                if((device&HALLWAY)!=HALLWAY)   // if HALLWAY is off
                     UART1_OutChar('0');        // send '0' to turn off HALLWAY
                else UART1_OutChar('1');        // send '1' to turn on HALLWAY
                break;
            }
            case '#': {
                // Turn off all devices
                device &= ~(SPEAKER|DESK1|DESK2|DESK3|LAMP|POLE|
                            RELAY3|RELAY4|FAN|BATHROOM|HALLWAY);
                select_led = 0;
                RELAY1 &= ~0x04;
                RELAY2 &= ~0x08;
                BUZZER &= ~0x10;
                FANPIN &= ~0x20;
                UART1_OutChar('#'); // send a command to slave to turn off devices
                break;
            }
            case '@':{  // Slave is just turn on.
                // Reset Slave's devices status
                device &= ~(HALLWAY|BATHROOM);
                select_led = 0;
            }
        }
    }
    
    // Playing sound
    if((device & SPEAKER)== SPEAKER){
        if(SoundTime < 59) BUZZER ^= 0x10;
        else if(SoundTime == 59){
            BUZZER &= ~0x10;
            device &= ~SPEAKER;
        }
    }
    SoundTime = ++SoundTime%60;

}

// Main
//  - main program where it initializes utilities and timers in uses
//    while stay at low power mode waiting for interrupt.
//
int main(void){
    PLL_Init();              // 50MHz PLL                
    UART0_Init();            // To display value received from BT on Serial Terminal
    UART1_Init();            // BlueTooth Module Init
    Keypad_Init();           // Keypad 
    PortE_Init();            // Relays and Buzzer Init
    Nokia5110_Init();        // Nokia5110 Init
    SysTick_Init( 1666666 ); // 30Hz Systick Interrupt 
    Timer0_Init(&Nokia_Task, 833333); // initialize timer0 (60 Hz) for Nokia5110
    EnableInterrupts();      // Enable interrupts
    
    UART0_OutString("Starting...\r\n");
    
    while(1){
        WaitForInterrupt();
    }
}



