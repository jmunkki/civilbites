/*
 * Avengers Natasha Romanoff Cosplay Widow's Bite LED control.
 * http://blog.poista.net/2016/01/21/black-widow-part-8-control-software/
 
Copyright (c)2015, Juri Munkki
 
Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files 
(the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge,
publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */


//  Gamma-based brightness:
//  Normal mode (low power):
const unsigned char gammaLookupNormal[] = {
	0, 46/4, 138/4, 264/4, 418/4, 598/4, 800/4, 255  //  Apply gamma 1.6
};
//  Bright mode (higher power):
const unsigned char  gammaLookupBright[] = {
	0, 99/4, 228/4, 370/4, 523/4, 684/4, 851/4, 255 //  Apply gamma 1.2
};

const unsigned char *gammaLookup = gammaLookupNormal;

const int mainPixels = 8+8;
const int neoPixels = 8+8+4;
const int paletteSize = 16;
const int halfPalette = 8;

#if LEFTHAND
const unsigned char neoMappings[][neoPixels] PROGMEM = {
	{
		0, 1, 2, 3,   0, 1, 2, 3,
		0, 1, 2, 3,   0, 1, 2, 3,
		//3, 2, 1, 0,   3, 2, 1, 0,
		4, 4, 4, 4,   
	},
};
#else
const unsigned char neoMappings[][neoPixels] PROGMEM = {
	{ 
		3, 2, 1, 0,   3, 2, 1, 0,  
		3, 2, 1, 0,   3, 2, 1, 0,  
		//0, 1, 2, 3,   0, 1, 2, 3,
		4, 4, 4, 4,
	},
};
#endif

enum {
	kClassicMode,
	kSixPacks,
	kDirectMode
};

struct ledState
{
	unsigned char           power;
	unsigned char           backPower;
} ;

//  Restorable machine state:
struct machineState
{
	int                 pc;
	unsigned char const *program;
	unsigned char const *neoMapping;
	int                 loopStackP;
	int                 colorIndex;
	unsigned char       primeColor;   // 0-7
	unsigned char       accentColor;  //  8-15
	boolean             fullColorMode;
	unsigned char       neoValues[neoPixels];
	unsigned char const *palette;
	unsigned char       actualPaletteSize;
};

machineState  mc;               //  current machine state
machineState  backMachineState; //  Saved BG machine state

#define byteColor(r,g,b) (((r & 7) << 5) + ((g & 7) << 2) + ((b & 5) >> 1))

void initMachineState(struct machineState *nmc, unsigned char const *program)
{
	nmc->pc = 0;
	nmc->colorIndex = 0;
	nmc->program = program;
	nmc->palette = NULL;
	nmc->primeColor = byteColor(2,2,7);
	nmc->accentColor = byteColor(7, 0, 0);
	nmc->neoMapping = neoMappings[0];
	for(unsigned char i=0;i<neoPixels;i++)
		nmc->neoValues[i] = 0;
}

//  Repeat loop stack:
int           loopStackPC[8];
int           loopStackC[8];

//  Background process resume:
boolean       runningBG = true;
unsigned long backTime;
unsigned long pauseEnd;

#if 0
int           pc = 0;
unsigned char const *program;
unsigned long pauseEnd = 0;
int           loopStackPC[8];
int           loopStackC[8];
int           colorIndex = 0;

//  We can interrupt & resume one background process:
int           backPC;
unsigned char const *backProgram;
unsigned long backTime;
int           backStackP;
int           backColorIndex;
#endif

/*
 * Pace/timing control:
 */

unsigned long loopMicroseconds = 500;
unsigned long timeNow = 0;
unsigned long nextLoop = 0;

/*
 * Use delays to pace the loop into running at regular intervals.
 */
void paceControl()
{
	unsigned long now = micros();
	
	if(now < timeNow || nextLoop+loopMicroseconds < timeNow) //  wrapped around!
		nextLoop = now;
	
	if(nextLoop > now)
	{
		unsigned long delta = nextLoop - now;
		delayMicroseconds(delta - (delta >> 7));
	}
	
	nextLoop += loopMicroseconds;
	timeNow = micros();
}

//  That's the end of the neopixel library from josh.com

// NeoBites continued here...
boolean needsUpdate = true;

#define ACCENTDEBUG_X
#ifdef ACCENTDEBUG
boolean debugAccents = true;
#endif
void  ledControl()
{
	if(needsUpdate)
	{
		unsigned char mappedR[paletteSize];
		unsigned char mappedG[paletteSize];
		unsigned char mappedB[paletteSize];

		if(mc.palette)
		{
			sprintln("Palette in use");
			for(unsigned char i=0;i<mc.actualPaletteSize;i++)
			{
				unsigned char v = pgm_read_byte_near(mc.palette + i);
				mappedR[i] = gammaLookup[v>> 5];
				mappedG[i] = gammaLookup[(v >> 2) & 7];
				mappedB[i] = gammaLookup[(v & 3)<<1];
			}
		}
		else
		{ //  DuoChrome
			unsigned int   primeR = mc.primeColor >> 5;
			unsigned int   primeG = (mc.primeColor >> 2) & 7;
			unsigned int   primeB = (mc.primeColor & 3);
			primeB = (primeB & 1) + (primeB << 1);
			unsigned int   accentR = mc.accentColor >> 5;
			unsigned int   accentG = (mc.accentColor >> 2) & 7;
			unsigned int   accentB = (mc.accentColor & 3);
			accentB = (accentB & 1) + (accentB << 1);
  
			for(unsigned char i=0;i<halfPalette;i++)
			{
				mappedR[i] = primeR * gammaLookup[i] / 7;
				mappedG[i] = primeG * gammaLookup[i] / 7;
				mappedB[i] = primeB * gammaLookup[i] / 7;
			}

			for(unsigned char i=halfPalette;i<paletteSize;i++)
			{
				unsigned char pixel = i - 8;
				mappedR[i] = accentR * gammaLookup[pixel] / 7;
				mappedG[i] = accentG * gammaLookup[pixel] / 7;
				mappedB[i] = accentB * gammaLookup[pixel] / 7;
			}
#ifdef ACCENTDEBUG      
			if(debugAccents)
			{
				debugAccents = false;
			
				sprint("Accent ");
				sprint(" ");
				sprint(accentR);
				sprint(" ");
				sprint(accentG);
				sprint(" ");
				sprintln(accentB);
				for(unsigned char i=halfPalette;i<paletteSize;i++)
				{
					sprint(i);
					sprint(" ");
					sprint(mappedR[i]);
					sprint(" ");
					sprint(mappedG[i]);
					sprint(" ");
					sprintln(mappedB[i]);
				}
			}
			#endif
		}

#ifdef ACCENTDEBUG      
		for(unsigned char i=0;i<8;i++)
		{
			if(mc.neoValues[pgm_read_byte_near(mc.neoMapping + i)] > 8)
			{
				sprint("Values [");
				for(unsigned char i=0;i<8;i++)
				{
					unsigned char v = mc.neoValues[pgm_read_byte_near(mc.neoMapping + i)];
					
					sprint(v);
					sprint(" ");
				}
				sprintln("]");
				break;
			}
		}
#endif
		needsUpdate = false;
		startNeopixels();
		
		strand_bit(1);
		for(unsigned char i=0;i<8;i++)
		{
			unsigned char v = mc.neoValues[pgm_read_byte_near(mc.neoMapping + i)];
			sendPixel(mappedR[v], mappedG[v], mappedB[v]);
			//sendPixel(0,0,0);
		}
		
		strand_bit(3);
		for(unsigned char i=8;i<16;i++)
		{
			unsigned char v = mc.neoValues[pgm_read_byte_near(mc.neoMapping + i)];
			sendPixel(mappedR[v], mappedG[v], mappedB[v]);
		}
		
		strand_bit(0);
		for(unsigned char i=16;i<20;i++)
		{
			unsigned char v = mc.neoValues[pgm_read_byte_near(mc.neoMapping + i)];
			sendPixel(mappedR[v], mappedG[v], mappedB[v]);
		}
		endNeopixels();
		show();
	}
}

//  I had a macro here, but the function is inlined automatically, so this is cleaner...
void setLEDPower(int i, int brt)
{
	if(mc.neoValues[i] != brt)
	{
		needsUpdate = true;
		mc.neoValues[i] = brt;
	}
}

/*
 * Needs:
    Set palette: (size) RGBPX * size (max 16) 8-bit encoded colors (R3, G3, B2)
    Set monochrome X color palette (say 8 or 16)
    Set mapping: 4*3*2, 6*2*2, 12*2

    DuoChrome palette: 0-7 primary color, 8-15 hilite color (compatible with legacy animations)

16   generic
  01 Start Long Loop (reads a byte counter)
  02 End Loop (no additional data)
  03 End Program (no additional data)
  04 Set palette (size) RGBPX * size (max 16) 8-bit encoded colors (R3, G3, B2)
  05 Set monochrome X color palette (say 8 or 16)
  06 Set LED mapping: Classic4, (6+6)*2, 12*2, direct
  07 Pause X seconds
  08 Full classic mode emulation init (selects 8 color monochrome palette & Classic 4 LED mapping)
  09 Gamma normal (Gamma opcodes might becom two-byte 0F-ops later to make space)
  0A Gamma bright
  0B Gamma toggle
  0C-0E Expansion
  0F Two-byte opcode expansion

Decoding notes:
  If the value is negative, it's a pause. Let's scale this by 40, so the range is 0 to 5.08 seconds.
  If the value is positive, extract the low nibble and switch based on the high nibble.

 */
enum {
  opOopsUnused  = 0x00,
  opStartBigLoop= 0x01,
  opEndLoop     = 0x02,
  opEndProgram  = 0x03,
  opSetPalette  = 0x04,   // To be implemented
  opSetMonochrome = 0x05,
  opSetMapping  = 0x06,
  opSetLongPause= 0x07,   //  Tenth of seconds up to 25.5 seconds?
  opSetClassic  = 0x08,
  opNormalGamma = 0x09,
  opBrightGamma = 0x0A,
  opToggleGamma = 0x0B,
  opDebugOn     = 0x0C,
  opDebugOff    = 0x0D,

  opSetFrontX   = 0x10, //  4 bit embedded parameter
  opSetRingX    = 0x20,
  opSetBothX    = 0x30,
  opSetAllX     = 0x40,
  opStartLoopX  = 0x50,
  opSetColorX   = 0x60,
  opRotateFromX  = 0x70,
  opPauseX       = 0x80,  //  7 bit embedded parameter
};

//  Macros to generate opcodes with embedded constants:
#define opPauseTime(ms) (opPauseX + (((ms + 5) / 10) & 0x7F))
#define opPause1Sec   opPauseTime(1000)

#define opAll(b)          (opSetAllX + b)
#define opRing(color)     (opSetRingX + color)

#define opRepeat(c)       (opStartLoopX + c)
#define opStartLoop       opStartLoopX
#define opClearAll        opAll(0)
#define opSetFront(led)   (opSetFrontX + led)
#define opSetRing(led)    (opSetRingX + led)
#define opSetBoth(led)    (opSetBothX + led)
#define opSetColor(color) (opSetColorX + color)
#define opYield           opPauseX
#define L(l,c)            opSetColor(c), opSetBoth(l)


const unsigned char deadBG[] PROGMEM = {
	opClearAll,
	opStartLoop,
	opPause1Sec,
	opEndLoop,
};

//  LED control programs:
//
//  This is where you can experiment with different LED lighting sequences.

const int blinkBlinkCount = 2;
const int blinkBlinkSpeed = 70;
const unsigned char blinkBlinkBG[] PROGMEM = {
  opClearAll,
  opStartLoop,
    opRepeat(20),
      opSetRing(1),
      opRepeat(blinkBlinkCount),
        L(0,2), opPauseTime(blinkBlinkSpeed),
        L(0,0), opPauseTime(blinkBlinkSpeed),
      opEndLoop,
      opSetRing(1),
      opRepeat(blinkBlinkCount),
        L(1,2), opPauseTime(blinkBlinkSpeed),
        L(1,0), opPauseTime(blinkBlinkSpeed),
      opEndLoop,
      opSetRing(1),
      opRepeat(blinkBlinkCount),
        L(2,2), opPauseTime(blinkBlinkSpeed),
        L(2,0), opPauseTime(blinkBlinkSpeed),
      opEndLoop,
      opSetRing(1),
      opRepeat(blinkBlinkCount),
        L(3,2), opPauseTime(blinkBlinkSpeed),
        L(3,0), opPauseTime(blinkBlinkSpeed),
      opEndLoop,
    opEndLoop,
    opSetRing(5), opPauseTime(50),
    opSetRing(4), opPauseTime(50),
    opSetRing(3), opPauseTime(50),
    opSetRing(2), opPauseTime(100),
  opEndLoop,
};

const int lowPowerIdleSpeed = 70;
const unsigned char lowPowerIdle[] PROGMEM = {
  opStartLoop,
    opClearAll,
    opPauseTime(250),
    opRing(1),
    opRepeat(10),
      opPause1Sec,
    opEndLoop,
    opRepeat(4),
      L(0,4), L(3,1), opPauseTime(lowPowerIdleSpeed),
      L(1,4), L(0,1), opPauseTime(lowPowerIdleSpeed),
      L(2,4), L(1,1), opPauseTime(lowPowerIdleSpeed),
      L(3,4), L(2,1), opPauseTime(lowPowerIdleSpeed),
    opEndLoop,
  opEndLoop,
};

const unsigned char stealthIdle[] PROGMEM = {
  opStartLoop,
    opClearAll,
    opRepeat(10),
      opPause1Sec,
    opEndLoop,
    opRepeat(4),
      opClearAll, L(0,1), opPauseTime(lowPowerIdleSpeed),
      opClearAll, L(1,1), opPauseTime(lowPowerIdleSpeed),
      opClearAll, L(2,1), opPauseTime(lowPowerIdleSpeed),
      opClearAll, L(3,1), opPauseTime(lowPowerIdleSpeed),
    opEndLoop,
  opEndLoop,
};

const int idleLoopOuter = 5;
const int idleSpinSpeed = 70;
const int fasterSpinSpeed = 50;
const unsigned char flashyBG[] PROGMEM = {
    // Rotate one way:
  opClearAll,
   opStartLoop,
     opRepeat(idleLoopOuter),
     opRepeat(10),
       opPauseTime(idleSpinSpeed),   opRing(1),   L(0,2), L(3,1), L(2,1),
       opPauseTime(idleSpinSpeed),   opRing(1),   L(1,2), L(0,1), L(3,1),
       opPauseTime(idleSpinSpeed),   opRing(1),   L(2,2), L(1,1), L(0,1),
       opPauseTime(idleSpinSpeed),   opRing(1),   L(3,2), L(2,1), L(1,1),
    opEndLoop,
    opEndLoop,

    // Flash:
    opPauseTime(fasterSpinSpeed),   opClearAll,   L(0,6), L(3,2), L(2,1),
    opPauseTime(fasterSpinSpeed),   opClearAll,   L(1,6), L(0,2), L(3,1),
    opPauseTime(fasterSpinSpeed),   opClearAll,   L(2,6), L(1,2), L(0,1),
    opPauseTime(fasterSpinSpeed),   opClearAll,   L(3,6), L(2,2), L(1,1),
    opPauseTime(fasterSpinSpeed), opRing(7),
    opPauseTime(fasterSpinSpeed), opRing(5),
    opPauseTime(fasterSpinSpeed), opRing(3),
    opPauseTime(fasterSpinSpeed), opRing(1),

    // Rotate the other way:
     opRepeat(idleLoopOuter),
     opRepeat(10),
       opPauseTime(idleSpinSpeed),   opRing(1),   L(3,2), L(2,1), L(1,1),
       opPauseTime(idleSpinSpeed),   opRing(1),   L(2,2), L(1,1), L(0,1),
       opPauseTime(idleSpinSpeed),   opRing(1),   L(1,2), L(0,1), L(3,1),
       opPauseTime(idleSpinSpeed),   opRing(1),   L(0,2), L(3,1), L(2,1),
    opEndLoop,
    opEndLoop,

    // Flash the other way:
    opPauseTime(fasterSpinSpeed),   opClearAll,   L(3,6), L(2,2), L(1,1),
    opPauseTime(fasterSpinSpeed),   opClearAll,   L(2,6), L(1,2), L(0,1),
    opPauseTime(fasterSpinSpeed),   opClearAll,   L(1,6), L(0,2), L(3,1),
    opPauseTime(fasterSpinSpeed),   opClearAll,   L(0,6), L(3,2), L(2,1),
    opPauseTime(fasterSpinSpeed), opRing(7),
    opPauseTime(fasterSpinSpeed), opRing(5),
    opPauseTime(fasterSpinSpeed), opRing(3),
    opPauseTime(fasterSpinSpeed), opRing(1),
   
   opEndLoop,
   opEndProgram
};

const unsigned char accentedFlashyBG[] PROGMEM = {
    // Rotate one way:
  opClearAll,
   opStartLoop,
     opRepeat(idleLoopOuter),
     opRepeat(10),
       opPauseTime(idleSpinSpeed),   opRing(1),   L(0,2+8), L(3,1), L(2,1),
       opPauseTime(idleSpinSpeed),   opRing(1),   L(1,2+8), L(0,1), L(3,1),
       opPauseTime(idleSpinSpeed),   opRing(1),   L(2,2+8), L(1,1), L(0,1),
       opPauseTime(idleSpinSpeed),   opRing(1),   L(3,2+8), L(2,1), L(1,1),
    opEndLoop,
    opEndLoop,

    // Flash:
    opPauseTime(fasterSpinSpeed),   opClearAll,   L(0,6), L(3,2), L(2,1),
    opPauseTime(fasterSpinSpeed),   opClearAll,   L(1,6), L(0,2), L(3,1),
    opPauseTime(fasterSpinSpeed),   opClearAll,   L(2,6), L(1,2), L(0,1),
    opPauseTime(fasterSpinSpeed),   opClearAll,   L(3,6), L(2,2), L(1,1),
    opPauseTime(fasterSpinSpeed), opRing(7),
    opPauseTime(fasterSpinSpeed), opRing(5),
    opPauseTime(fasterSpinSpeed), opRing(3),
    opPauseTime(fasterSpinSpeed), opRing(1),

    // Rotate the other way:
     opRepeat(idleLoopOuter),
     opRepeat(10),
       opPauseTime(idleSpinSpeed),   opRing(1),   L(3,2+8), L(2,1), L(1,1),
       opPauseTime(idleSpinSpeed),   opRing(1),   L(2,2+8), L(1,1), L(0,1),
       opPauseTime(idleSpinSpeed),   opRing(1),   L(1,2+8), L(0,1), L(3,1),
       opPauseTime(idleSpinSpeed),   opRing(1),   L(0,2+8), L(3,1), L(2,1),
    opEndLoop,
    opEndLoop,

    // Flash the other way:
    opPauseTime(fasterSpinSpeed),   opClearAll,   L(3,6), L(2,2), L(1,1),
    opPauseTime(fasterSpinSpeed),   opClearAll,   L(2,6), L(1,2), L(0,1),
    opPauseTime(fasterSpinSpeed),   opClearAll,   L(1,6), L(0,2), L(3,1),
    opPauseTime(fasterSpinSpeed),   opClearAll,   L(0,6), L(3,2), L(2,1),
    opPauseTime(fasterSpinSpeed), opRing(7),
    opPauseTime(fasterSpinSpeed), opRing(5),
    opPauseTime(fasterSpinSpeed), opRing(3),
    opPauseTime(fasterSpinSpeed), opRing(1),
   
   opEndLoop,
   opEndProgram
};

const unsigned char effect00[] PROGMEM = {
  opAll(3),
  opPauseTime(200), L(0,1),
  opPauseTime(200), L(1,1),
  opPauseTime(200), L(2,1),
  opPauseTime(200), L(3,2),
  opPauseTime(1000),
  opClearAll,
};

const int bam_fade = 70;
const unsigned char bamEffect[] PROGMEM = {
  opAll(7),
  opPauseTime(bam_fade), L(0,5), L(1,6), L(2,4), L(3,5),
  opPauseTime(bam_fade), L(0,3), L(1,5), L(2,4), L(3,3),
  opPauseTime(bam_fade), L(0,1), L(1,4), L(2,3), L(3,3),
  opPauseTime(bam_fade), L(0,1), L(1,2), L(2,2), L(3,2),
  opPauseTime(bam_fade), L(0,1), L(1,1), L(2,1), L(3,1),
  opPauseTime(bam_fade), opClearAll,
  opEndProgram,
};

const int back_forth_speed = 70;
const unsigned char backForth[] PROGMEM = {
  opAll(7),
  opPauseTime(bam_fade), L(0,5), L(1,6), L(2,4), L(3,5),
  opPauseTime(bam_fade), L(0,3), L(1,5), L(2,4), L(3,3),
  opPauseTime(bam_fade), L(0,1), L(1,4), L(2,3), L(3,3),
  opPauseTime(bam_fade), L(0,1), L(1,2), L(2,2), L(3,2),
  opPauseTime(bam_fade), L(0,1), L(1,1), L(2,1), L(3,1),
  //opPauseTime(bam_fade), opClearAll,

  opRepeat(3),
    opRepeat(3),
      opPauseTime(back_forth_speed), opClearAll, L(0,2),
      opPauseTime(back_forth_speed), opClearAll, L(1,2),
      opPauseTime(back_forth_speed), opClearAll, L(2,2),
      opPauseTime(back_forth_speed), opClearAll, L(3,2),
    opEndLoop,
#if 0
    opRepeat(2),
      opPauseTime(back_forth_speed), opClearAll, L(2,2),
      opPauseTime(back_forth_speed), opClearAll, L(1,2),
      opPauseTime(back_forth_speed), opClearAll, L(0,2),
      opPauseTime(back_forth_speed), opClearAll, L(3,2),
    opEndLoop,
#endif
  opEndLoop,
  opEndProgram,
};

const int startup_tick = 100;
const unsigned char startupAction[] PROGMEM = {  
  opAll(4),
#if 0
  opPauseTime(startup_tick), opClearAll, L(0,3),
  opPauseTime(startup_tick), opClearAll, L(1,3),
  opPauseTime(startup_tick), opClearAll, L(2,3),
  opPauseTime(startup_tick), opClearAll, L(3,3),
#else
  opPauseTime(startup_tick), L(0,2),
  opPauseTime(startup_tick), L(0,1),
  opPauseTime(startup_tick), L(1,2),
  opPauseTime(startup_tick), L(1,1),
  opPauseTime(startup_tick), L(2,2),
  opPauseTime(startup_tick), L(2,1),
  opPauseTime(startup_tick), L(3,2),
  opPauseTime(startup_tick), L(3,1),
#endif
  opPauseTime(startup_tick), opClearAll,
  opEndProgram,
};

#if 0
//  Classic version:
const int ramp_tick = 75;
const unsigned char longRamp[] PROGMEM = {  
  opPauseTime(ramp_tick),    opAll(1),
  opPauseTime(ramp_tick),    opAll(2),
  opPauseTime(ramp_tick),    opAll(3),
  opPauseTime(ramp_tick),    opAll(4),
  opPauseTime(ramp_tick*2),  opAll(5),
  opPauseTime(ramp_tick*3),  opAll(6),
  opPauseTime(ramp_tick),    opAll(7),
  opPauseTime(ramp_tick*20), opAll(5),
  opPauseTime(ramp_tick*20), opAll(6),
  opPauseTime(ramp_tick*20), opAll(7),
  opPauseTime(ramp_tick*20), opAll(6),
  opPauseTime(ramp_tick),    opAll(4),
  opPauseTime(ramp_tick),    opAll(3),
  opPauseTime(ramp_tick*2),  opAll(2),
  opPauseTime(ramp_tick*2),  opAll(1),
  opPauseTime(ramp_tick*30),
  opEndProgram,
};
#else
//  2018 version:
const int ramp_tick = 20;
const unsigned char longRamp[] PROGMEM = {  
  opClearAll,
  opRing(1),            opPauseTime(ramp_tick),
  opRing(2),            opPauseTime(ramp_tick),
  opRing(3),            opPauseTime(ramp_tick),
  opRing(4),            opPauseTime(ramp_tick),
  opRing(4),  L(4, 1),  opPauseTime(ramp_tick),
  opRing(5),  L(4, 2),  opPauseTime(ramp_tick),
  opRing(4),  L(4, 3),  opPauseTime(ramp_tick),
  opRing(3),  L(4, 4),  opPauseTime(ramp_tick),
  opRing(2),  L(4, 5),  opPauseTime(ramp_tick),
  opRepeat(10),
  opRing(2), L(4, 6),   opPauseTime(ramp_tick),
  opRing(1), L(4, 6),   opPauseTime(ramp_tick),
  opRing(2), L(4, 5),   opPauseTime(ramp_tick),
  opRing(1), L(4, 5),   opPauseTime(ramp_tick),
  opEndLoop,
  opStartLoop,
  opRing(2+8), L(4, 6), opPauseTime(ramp_tick),
  opRing(1+8), L(4, 7), opPauseTime(ramp_tick),
  opEndLoop,
  opEndProgram
};
#endif

//const int bam_fade = 70;
const unsigned char bamLoop[] PROGMEM = {
  opStartLoop,
  opAll(7),
  opPauseTime(ramp_tick),    opAll(1),
  opPauseTime(ramp_tick),    opAll(2),
  opPauseTime(ramp_tick),    opAll(3),
  opPauseTime(ramp_tick),    opAll(4),
  opPauseTime(ramp_tick*2),  opAll(5),
  opPauseTime(ramp_tick*3),  opAll(6),
  opPauseTime(ramp_tick),    opAll(7),
  opPauseTime(ramp_tick*20), opAll(5),
  opPauseTime(ramp_tick*20), opAll(6),
  opPauseTime(ramp_tick*20), opAll(7),
  opPauseTime(ramp_tick*20), opAll(6),
  opPauseTime(ramp_tick),    opAll(4),
  opPauseTime(ramp_tick),    opAll(3),
  opPauseTime(ramp_tick*2),  opAll(2),
  opPauseTime(ramp_tick*2),  opAll(1),
  opPauseTime(ramp_tick*30),
  opEndLoop,
  opEndProgram,
};

const unsigned char toggleBrightnessAction[] PROGMEM = {
	opToggleGamma,
	opRing(3),
	opPause1Sec,
};

const unsigned char quickStop[] PROGMEM = {
	opClearAll,
	opPauseTime(1),
	opEndProgram,
};



const unsigned char normalBrightnessAction[] PROGMEM = {
	opNormalGamma,
	opClearAll,
	L(0,7),
	opPause1Sec,
	opPause1Sec,
	opEndProgram,
};

const unsigned char highBrightnessAction[] PROGMEM = {
	opBrightGamma,
	opRing(7),
	L(0,0),
	opPause1Sec,
	opPause1Sec,
	opEndProgram,
};

int valueToMillis(unsigned long v)
{
	return v * 10;
}

unsigned char readPCByte()
{
	return pgm_read_byte_near(mc.program + mc.pc++);
}

boolean baseOp(unsigned char op, unsigned long now)
{
	boolean fetch = true;
		
	switch(op)
	{
		case opStartBigLoop:
		{
			loopStackPC[++mc.loopStackP] = mc.pc;
			loopStackC[mc.loopStackP] = readPCByte();
			if(mc.loopStackP > 7)
				mc.loopStackP = 7;
			break;
		}
		case opEndLoop:
		{
			// Look at loop counter (on stack):
			int c = loopStackC[mc.loopStackP];
			if(c--)		//	Decrement counter
			{
				if(c)		//	Still going
				{
					//	Loop back to start & store decremented count
					mc.pc = loopStackPC[mc.loopStackP];
					loopStackC[mc.loopStackP] = c;
					//	Force yield here
					fetch = false;
				}
				else		//	All done
				{ //	Pop loop start
					mc.loopStackP--;
					if(mc.loopStackP < 0)
						mc.loopStackP = -1;
				}
			}
			else
			{
				//	Loop forever, if start count is 0
				mc.pc = loopStackPC[mc.loopStackP];
			}
			break;
		}
		case opEndProgram:
		{
			if(runningBG)
			{ //	Just stop here and yield forever:
				mc.pc--;
				fetch = false;
			}
			else
			{ //	Restore BG process:
				runningBG = true;
				mc = backMachineState;
				needsUpdate = true;
				
				if(backTime)
				{
					pauseEnd = now + backTime;
				}
				else
				{
					pauseEnd = 0;
				}
			}
			break;
		}
		case opSetPalette:
		case opSetMonochrome:
		case opSetMapping:
			break;
		case opSetLongPause:
		{
			pauseEnd = millis() + valueToMillis(10*readPCByte());
			fetch = false;
			break;
		}
		case opSetClassic:
			break;
		case opToggleGamma:
			{
				if(gammaLookup == gammaLookupNormal)
				{
					gammaLookup = gammaLookupBright;
				}
				else
				{
					gammaLookup = gammaLookupNormal;
				}
				break;
			}
			case opNormalGamma:
			{
				gammaLookup = gammaLookupNormal;
				break;
			}
			case opBrightGamma:
			{
				gammaLookup = gammaLookupBright;
				break;
			}
			case opDebugOn:
			{
				digitalWrite(debugLED, HIGH);
				break;
			}
			case opDebugOff:
			{
				digitalWrite(debugLED, LOW);
				break;
			}
	}

	return fetch;
}

void runCPU()
{
	unsigned char	op;
	boolean			fetch = true;
	unsigned long	now = millis();

	if(pauseEnd)
	{
		if(now < pauseEnd)
		{
			fetch = false;
		}
		else
		{
			pauseEnd = 0;
		}
	}
	
	while(fetch)
	{
		op = pgm_read_byte_near(mc.program + mc.pc++);

		if(op & 0x80)
		{ //	Pause uses 127 opcodes
			pauseEnd = millis() + valueToMillis(op-128);
			fetch = false;
		}
		else
		{
			unsigned char value = op & 0xF;

			switch(op & 0x70)
			{
				case 0:
				{
					fetch = baseOp(value, now);
					break;
				}
				case opSetFrontX:
					setLEDPower(value, mc.colorIndex);
					break;
				case opSetRingX:
					needsUpdate = true;
					for(unsigned char i=0;i<mainPixels;i++)
					{ 
						mc.neoValues[pgm_read_byte_near(mc.neoMapping + i)] = value;
					}
					break;
				case opSetBothX:
				{
					setLEDPower(value, mc.colorIndex);
				//	setLEDPower(value+12, mc.colorIndex); //	Remnant of Avengers 2012 wrists with front and back row.
					break;
				}
				case opSetAllX:
				{
					needsUpdate = true;
					for(unsigned char i=0;i<neoPixels;i++)
						mc.neoValues[i] = value;
					break;
				}
				case opStartLoopX:
				{
					loopStackPC[++mc.loopStackP] = mc.pc;
					loopStackC[mc.loopStackP] = value;
					if(mc.loopStackP > 7)
						mc.loopStackP = 7;
					break;
				}
				case opSetColorX:
				{
					mc.colorIndex = value;
					break;
				}
				case opRotateFromX:
					if(mc.palette)
					{
						for(unsigned char i = 0;i<neoPixels;i++)
						{
							unsigned char v = mc.neoValues[i];

							if(v >= value)
							{
								if(++v > mc.actualPaletteSize)
									v = value;

								mc.neoValues[i] = v;
							}
						}
					}
					break;
			}
		}
	}
}

//	Start running an action, interrupting any background animation
//	currently running or replacing an existing action.
void loadAction(unsigned char const *pgm)
{
	if(runningBG)
	{
		unsigned long now = millis();

		runningBG = false;
		backMachineState = mc;
		initMachineState(&mc, pgm);
		needsUpdate = true;

		if(pauseEnd and pauseEnd > now)
		{
			backTime = pauseEnd - now;
		}
		else
		{
			backTime = 0;
		}
		pauseEnd = 0;
	}
}

//	Start running a background animation. Resets the bytecode processor and
//	starts running this program.
void loadBackground(unsigned char const *pgm)
{
	initMachineState(&mc, pgm);
	mc.loopStackP = -1;
	runningBG = true;
	pauseEnd = 0;
}

/*
 * Button control functions including debounce and long/short sequence capture.
 */
const int			onePress = 1024;
const int			downTreshold = 5 * onePress;
const unsigned long	shortestTapTime = 20;		 // At least 20 milliseconds to detect a press
const unsigned long	shortPressMaxTime = 300; //	 Milliseconds
const unsigned long	longPressMaxTime = 3000; // 3 seconds
const unsigned long	sequencePause = 600;	//	Milliseconds
const int 			noSequence = 0;
const int 			shortTap = 1;
const int 			longTap = 2;

unsigned long		pressStart = 0;
unsigned long		pressEnd = 0;
int					pressSequence = noSequence;
int					fullSequence = 0;
int					taps = 0;

void buttonControl()
{
	static boolean	longTriggered = false;
	int				tapType;

	pollButton();

	if(!(rbuttonState & BUTTON_MASK)) //	If button is currently down
	{
		unsigned int downMillis = (loopStartTime - rbuttonDownTime) >> QUICK_MILLIS;
		// Has it been down for a bit?
		if(downMillis > shortPressMaxTime)
		{
			//	Trigger long action, if we haven't started it already.
			if(!longTriggered)
			{
				loadAction(longRamp);
				longTriggered = true;
				sprint(F("Long triggered "));
				sprintln(downMillis);
			}
		}
	}
	
	//	Check for button up event:
	if(rbuttonUp && rbuttonPressTime > shortestTapTime)
	{
		longTriggered = false;
		//	Identify short, long & super long presses:
		tapType = (rbuttonPressTime > shortPressMaxTime) + (rbuttonPressTime > longPressMaxTime) + 1;

		sprint(F("Tap "));
		sprint(tapType);
		sprint(" ");
		sprintln(rbuttonPressTime);
		
		//	Append press to octal sequence
		if(pressSequence)
		{
			fullSequence = (fullSequence << 3) + tapType;
		}
		else
		{
			pressSequence = tapType;
			fullSequence = tapType;
		}

		sprint(F("Sequence: "));
		sprintln(fullSequence);
		// This is where you can add new button tap sequences and
		//	run C code or animations based on user input:
		switch(fullSequence)
		{
			//	Starting with a short press (actions):
			case 01:
				loadAction(bamEffect);
				break;
			case 011:
				loadAction(backForth);
				break;
			case 0111:
				fullSequence = 01;

			//	Starting with a long press (background loops & control):
			case 02:
				loadBackground(lowPowerIdle);
				break;
			case 03:
				loadBackground(deadBG);
				break;
			case 021:
				loadBackground(flashyBG);
				break;
			case 012:
				sprintln(F("Accented"));
#ifdef ACCENTDEBUG			
				debugAccents = true;
#endif
				loadBackground(accentedFlashyBG);
				break;
			case 0212:
				loadBackground(stealthIdle);
				break;
			case 0211:
				loadBackground(deadBG);
				break;
			case 022:
				loadBackground(blinkBlinkBG);
				break;
			case 0221:
				loadBackground(lowPowerIdle);
				loadAction(normalBrightnessAction);
				break;
			case 0222:
				loadBackground(lowPowerIdle);
				loadAction(highBrightnessAction);
				break;
			default:
				if(!runningBG)
				{
					loadAction(quickStop);
				}
		}
	}

	if(pressSequence && (rbuttonState & BUTTON_MASK))	 //	 If button is currently up
	{
		unsigned int upMillis = (loopStartTime - rbuttonUpTime) >> QUICK_MILLIS;
		if(upMillis > sequencePause)
		{
			sprint(F("Resetting tap sequence: "));
			sprintln(upMillis);
			pressSequence = 0;
			fullSequence = 0;
		}
	}
	
	clearButtonEvents();
}
