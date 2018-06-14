# TM4C123HomeAutomation
Home Automation operating by two TM4C123GXL development board.
One MCU is the Master and another is the slave.

Master is consisted of 
1.) Nokia5110 - to display devices status.
2.) Keypad - to control devices, adjust pwm of LEDs, switch relays, etc.
3.) Bluetooth Module HC-05 - point-to-point Bluetooth full-duplex communication.
4.) Relays - to switch on/off for High voltage devices like Light pole, Fan, and Lamp.
5.) Buzzer - to play sound (future implementation).
Slave is consisted of
1.) two PIR sensors - detecting movement in Hallway and Bathroom.
2.) LED Strips - adjustible brightnes via PWM from microcontroller
3.) PN222A - transistors for switching on/off the LED strips, and allow currents from sources to the LED strips.
4.) Bluetooth Module HC-05 - point-to-point Bluetooth full-duplex communication.

Started on June 7, 2018 - on process.
