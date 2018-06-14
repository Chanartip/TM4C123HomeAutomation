# TM4C123HomeAutomation
## Home Automation operating by two TM4C123GXL development board.
### One MCU is the Master and another is the slave.

Master is consisted of 
- Nokia5110 - to display devices status.
- Keypad - to control devices, adjust pwm of LEDs, switch relays, etc.
- Bluetooth Module HC-05 - point-to-point Bluetooth full-duplex communication.
- Relays - to switch on/off for High voltage devices like Light pole, Fan, and Lamp.
- Buzzer - to play sound (future implementation).

Slave is consisted of
- two PIR sensors - detecting movement in Hallway and Bathroom.
- LED Strips - adjustible brightnes via PWM from microcontroller
- PN222A - transistors for switching on/off the LED strips, and allow currents from sources to the LED strips.
- Bluetooth Module HC-05 - point-to-point Bluetooth full-duplex communication.

Started on June 7, 2018 - on process.
