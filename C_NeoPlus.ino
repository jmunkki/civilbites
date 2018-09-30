/*
 * NeoPixel control software from josh.com
 * More info at http://wp.josh.com/2014/05/11/ws2812-neopixels-made-easy/
 */

// These are the timing constraints taken mostly from the WS2812 datasheets 
// These are chosen to be conservative and avoid problems rather than for maximum throughput 
#define T1H  900    // Width of a 1 bit in ns
#define T1L  600    // Width of a 1 bit in ns
#define T0H  400    // Width of a 0 bit in ns
#define T0L  900    // Width of a 0 bit in ns
#define RES 90000    // Width of the low gap between bits to cause a frame to latch

// Here are some convience defines for using nanoseconds specs to generate actual CPU delays
#define NS_PER_SEC (1000000000L)          // Note that this has to be SIGNED since we want to be able to check for negative values of derivatives
#define CYCLES_PER_SEC (F_CPU)
#define NS_PER_CYCLE ( NS_PER_SEC / CYCLES_PER_SEC )
#define NS_TO_CYCLES(n) ( (n) / NS_PER_CYCLE )
#define DELAY_CYCLES(n) ( ((n)>0) ? __builtin_avr_delay_cycles( n ) : __builtin_avr_delay_cycles( 0 ) ) // Make sure we never have a delay less than zero

#define	SET_RESET_OVERHEAD	6
#define	LOOP_OVERHEAD		10

inline void sendBit(char bitVal, byte setter)
{
    if ( bitVal < 0)
    {      // 1-bit
      PIXEL_PORT |= setter;
	  DELAY_CYCLES( NS_TO_CYCLES( T1H ) - SET_RESET_OVERHEAD);
      PIXEL_PORT ^= setter;
      sei();
      DELAY_CYCLES( NS_TO_CYCLES( T1L ) - LOOP_OVERHEAD ); // 1-bit gap less the overhead of the loop
	  cli();
    }
    else
    {             // 0-bit
      PIXEL_PORT |= setter;
      DELAY_CYCLES( NS_TO_CYCLES( T0H ) - SET_RESET_OVERHEAD);
      PIXEL_PORT ^= setter;
      sei();
      DELAY_CYCLES( NS_TO_CYCLES( T0L ) - LOOP_OVERHEAD ); // 0-bit gap less overhead of the loop
	  cli();
    } 
}

static byte setter_mask = 0x01;

void sendByte(char value)
{
	byte pv;
	byte setter = setter_mask;
	
	for(byte bit=0; bit < 4; bit++)
	{  
	  sendBit(value, setter);
	  value += value;
	}
#ifdef POLL_FN
	POLL_FN();
#endif
	for(byte bit=0; bit < 4; bit++)
	{  
	  sendBit(value, setter);
	  value += value;
	}
} 

/*
  The following three functions are the public API:
  ledSetup() - set up the pin that is connected to the string. Call once at the begining of the program.  
  sendPixel( r g , b ) - send a single pixel to the string. Call this once for each pixel in a frame.
  show() - show the recently sent pixel on the LEDs . Call once per frame. 
*/
void ledSetup(byte mask)
{
  PIXEL_DDR |= mask;
  setter_mask = mask & (0xFF ^ (mask - 1));
}

inline void sendPixel(unsigned char r, unsigned char g, unsigned char b)
{    
  sendByte(g);          // Neopixel wants colors in green then red then blue order
  sendByte(r);
  sendByte(b);
}

inline void	startNeopixels()
{
	cli();
}

inline void	endNeopixels()
{
	sei();
}

// Just wait long enough without sending any bots to cause the pixels to latch and display the last sent frame

void show() {
  _delay_us( (RES / 1000UL) + 1);       // Round up since the delay must be _at_least_ this long (too short might not work, too long not a problem)
}
//  That's the end of the neopixel library from josh.com

void strand_bit(byte n)
{
	setter_mask = 1 << n;
}
