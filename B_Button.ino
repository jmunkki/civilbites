
byte	oldButtonRaw;

byte 	rbuttonState;
boolean rbuttonDown = false;
boolean	rbuttonUp = false;
boolean	firstPress = false;
unsigned long	rbuttonPressTime;
TIME_TYPE	rbuttonDownTime;
TIME_TYPE	rbuttonUpTime;
char	debouncer = 0;

void clearButtonEvents()
{
  rbuttonDown = false;
  rbuttonUp = false;
}

void buttonSetup()
{
  DDRB &= 0xFF ^ BUTTON_MASK;	//	Configure as input
  BUTTON_PORT |= BUTTON_MASK;		//	Internal pullup
  oldButtonRaw  = BUTTON_PIN & BUTTON_MASK;
  rbuttonState = oldButtonRaw & BUTTON_MASK;
  clearButtonEvents();
}

#define	READMASK(value, mask)	(0 != (mask & value))

void doButton(byte regValue)
{
	byte buttonNew = regValue & BUTTON_MASK;
	if((buttonNew ^ rbuttonState) == BUTTON_MASK)
	{
		rbuttonState = buttonNew;
		if(buttonNew)
		{
			rbuttonUp = true;
			firstPress = true;
			rbuttonPressTime = (loopStartTime - rbuttonDownTime) >> QUICK_MILLIS;
			rbuttonUpTime = loopStartTime;
		}
		else
		{
			rbuttonDown = true;
			rbuttonDownTime = loopStartTime;
		}
	}
}

inline void pollButton()
{
	byte	newButtonRaw = BUTTON_PIN & BUTTON_MASK;
	
	if(newButtonRaw)
	{	if(++debouncer > 10)
			debouncer = 10;
	}
	else
	{	if(--debouncer < -9)
			debouncer = -9;
	}
	
	if(debouncer > 0)
		newButtonRaw = BUTTON_MASK;
	else
		newButtonRaw = 0;
	
	if(newButtonRaw != oldButtonRaw)
	{
		oldButtonRaw = newButtonRaw;
		doButton(newButtonRaw);
	}
}