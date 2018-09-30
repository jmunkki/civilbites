/*
 * Avengers Natasha Romanoff Cosplay Widow's Bite - Civil/Infinity War LED control.
 */

//  Configuration options:
#define LEFTHAND              1
#define DEBUGBUTTON           0
#define	SERIALDEBUG			  0

const int button = 10;
const int debugLED = 13;

#define	BUTTON_PIN		PINB
#define	BUTTON_DDR		DDRB
#define	BUTTON_PORT		PORTB
#define	BUTTON_MASK		0x04

#define PIXEL_PORT  	PORTB  // Port of the pin the pixels are connected to
#define PIXEL_DDR   	DDRB   // Port of the pin the pixels are connected to
#define	POLL_FN			pollButton

#if SERIALDEBUG
#define sprint(v) 		Serial.print(v)
#define sprintln(v)  	Serial.println(v)
#else
#define sprint(v) 		
#define sprintln(v)  	
#endif