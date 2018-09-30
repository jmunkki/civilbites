/*
 *  Main Loop Timing:
 */

#define	MILLITIMING						1
#if MILLITIMING		//	Millisecond timing:
#define	TIMING_FUNCTION		millis()
#define	TIME_TYPE			unsigned int
#define	QUICK_MILLIS		0

#define	LIMIT_FREQUENCY(v, dt) \
	static unsigned long v = 0; \
	if((int)(loopStartTime - v) < 0) \
		return; \
	else \
		v = loopStartTime + dt;

#else				//	Microsecond timing:
#define	TIMING_FUNCTION			micros()
#define	TIME_TYPE				unsigned long
#define	QUICK_MILLIS			10

#define	LIMIT_FREQUENCY(v, dt) \
	static unsigned long v = 0; \
	if((long)(loopStartTime - v + dt * 1000) < 0) \
		return; \
	else \
		v = loopStartTime + dt * 1000;
#endif

TIME_TYPE   	loopStartTime;
		
int blinking(int value)
{
	if(bitRead(loopStartTime, (8+QUICK_MILLIS)))
		return value;
	else
		return 0;	
}
