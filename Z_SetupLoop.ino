// the setup routine runs once when you press reset on the Trinket:
void setup()
{
#if SERIALDEBUG
	Serial.begin(115200);
	sprintln(F("Civil War Wrists"));
#endif
	ledSetup(11);
	//  Initialize button input:
	//pinMode(button, INPUT);
	//digitalWrite(button, HIGH);
	buttonSetup();
	
	pinMode(debugLED, OUTPUT);
	digitalWrite(debugLED, LOW);
	
	//loadBackground(accentedFlashyBG);
	loadBackground(lowPowerIdle);
	//loadBackground(stealthIdle);
	loadAction(startupAction);
}

// This is the main loop routine.
//  It runs over and over again until power goes out.
void loop() {
	//  Run at a constant tick:
	paceControl();
	loopStartTime = TIMING_FUNCTION;

	//  Check for user input:
	buttonControl();

	//  Execute bytecode:
	runCPU();

	//  Update LEDs on/off based on desired brightness:
	ledControl();
}
