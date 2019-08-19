// Test playing a succession of MIDI files from the SD card.
// Example program to demonstrate the use of the MIDFile library
// Just for fun light up a LED in time to the music.
//
// Hardware required:
//  SD card interface - change SD_SELECT for SPI comms
//  3 LEDs (optional) - to display current status and beat. 
//  Change pin definitions for specific hardware setup - defined below.

#include <SdFat.h>
#include <MD_MIDIFile.h>
#include <Adafruit_PWMServoDriver.h>

#define NUM_NOTES 37

#define USE_MIDI  0
#define USE_SERVO 1


#if USE_SERVO

Adafruit_PWMServoDriver pwm1 = Adafruit_PWMServoDriver(&Wire, 0x40);
Adafruit_PWMServoDriver pwm2 = Adafruit_PWMServoDriver(&Wire, 0x41);

#define SERVOMIN  125 // this is the 'minimum' pulse length count (out of 4096)
#define SERVOMAX  575 // this is the 'maximum' pulse length count (out of 4096)


#endif


#if USE_MIDI // set up for direct MIDI serial output

#define DEBUG(x)
#define DEBUGX(x)
#define SERIAL_RATE 31250

#else // don't use MIDI to allow printing debug statements

#define DEBUG(x)  Serial.print(x)
#define DEBUGX(x) Serial.print(x, HEX)
#define SERIAL_RATE 57600

#endif // USE_MIDI



// SD chip select pin for SPI comms.
// Arduino Ethernet shield, pin 4.
// Default SD chip select is the SPI SS pin (10).
// Other hardware will be different as documented for that hardware.
#define  SD_SELECT  4

// LED definitions for user indicators
#define READY_LED     7 // when finished
#define SMF_ERROR_LED 6 // SMF error
#define SD_ERROR_LED  5 // SD error
#define BEAT_LED      6 // toggles to the 'beat'

#define WAIT_DELAY    2000 // ms

#define ARRAY_SIZE(a) (sizeof(a)/sizeof(a[0]))

// The files in the tune list should be located on the SD card 
// or an error will occur opening the file and the next in the 
// list will be opened (skips errors).
char *tuneList[] = 
{
  "TEST4.MID",
//  "LOOPDEMO.MID",  // simplest and shortest file
//  "TWINKLE.MID",
//  "GANGNAM.MID",
//  "FUGUEGM.MID",
//  "POPCORN.MID",
//  "AIR.MID",
//  "PRDANCER.MID",
//  "MINUET.MID",
//  "FIRERAIN.MID",
//  "MOZART.MID",
//  "FERNANDO.MID",
//  "SONATAC.MID",
//  "SKYFALL.MID",
//  "XMAS.MID",
//  "GBROWN.MID",
//  "PROWLER.MID",
//  "IPANEMA.MID",
//  "JZBUMBLE.MID",
};

// These don't play as they need more than 16 tracks but will run if MIDIFile.h is changed
//#define MIDI_FILE  "SYMPH9.MID"		// 29 tracks
//#define MIDI_FILE  "CHATCHOO.MID"		// 17 tracks
//#define MIDI_FILE  "STRIPPER.MID"		// 25 tracks

SdFat	SD;
MD_MIDIFile SMF;

const int pentatonic[NUM_NOTES] = {21, 24, 26, 28, 31, 33, 36, 38, 40, 43, 45, 48, 50, 52, 55, 57, 60, 62, 64, 67, 69, 72, 74, 76, 79, 81, 84, 86, 88, 91, 93, 96, 98, 100, 103, 105, 108};  
int servoarr[NUM_NOTES] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};  


void midiCallback(midi_event *pev)
// Called by the MIDIFile library when a file event needs to be processed
// thru the midi communications interface.
// This callback is set up in the setup() function.
{
#if USE_MIDI
  if ((pev->data[0] >= 0x80) && (pev->data[0] <= 0xe0))
  {
    Serial.write(pev->data[0] | pev->channel);
    Serial.write(&pev->data[1], pev->size-1);
  }
  else
    Serial.write(pev->data, pev->size);
#endif
//  DEBUG("\n");
//  DEBUG(millis());
//  DEBUG("\tM T");
//  DEBUG(pev->track);
//  DEBUG(":  Ch ");
//  DEBUG(pev->channel+1);
//  DEBUG(" Data ");
//  for (uint8_t i=0; i<pev->size; i++)
//  {
//	DEBUGX(pev->data[i]);
//    DEBUG(' ');
//  }


  if (pev->data[0] == 144)
  {
for (int i=0; i < NUM_NOTES; i++) {
        if (pev->data[1] == pentatonic[i])
        {
          int time = millis();
          servoarr[i] = time;
          Serial.println(servoarr[i]);
          
          pwm1.setPWM(i, 0, 200);
      

         // pwm1.setPWM(servo, 0, 200);
//         Serial.print("servo number ");
//         Serial.print(i);
//         Serial.print("\n");
//    Serial.println(pev->data[0]);
//    Serial.println(pev->data[1]);
//    Serial.println(pev->data[2]);
//    Serial.println(pev->data[3]);
//    Serial.println(pev->data[4]);
//    Serial.print("\n");
    
        }

    }
  }
}

 


void sysexCallback(sysex_event *pev)
// Called by the MIDIFile library when a system Exclusive (sysex) file event needs 
// to be processed through the midi communications interface. Most sysex events cannot 
// really be processed, so we just ignore it here.
// This callback is set up in the setup() function.
{
//  DEBUG("\nS T");
//  DEBUG(pev->track);
//  DEBUG(": Data ");
//  for (uint8_t i=0; i<pev->size; i++)
//  {
////    DEBUGX(pev->data[i]);
////    DEBUG(' ');
//  }
}

void midiSilence(void)
// Turn everything off on every channel.
// Some midi files are badly behaved and leave notes hanging, so between songs turn
// off all the notes and sound
{
  midi_event ev;

  // All sound off
  // When All Sound Off is received all oscillators will turn off, and their volume
  // envelopes are set to zero as soon as possible.
  ev.size = 0;
  ev.data[ev.size++] = 0xb0;
  ev.data[ev.size++] = 120;
  ev.data[ev.size++] = 0;

  for (ev.channel = 0; ev.channel < 16; ev.channel++)
    midiCallback(&ev);
}

void setup(void)
{
  pwm1.begin();

  pwm1.setPWMFreq(60);  // Analog servos run at ~60 Hz updates

  //yield();
  pwm2.begin();

  pwm2.setPWMFreq(60);  // Analog servos run at ~60 Hz updates

//
//for (int i = 0; i < 11; i++) {
//    pwm1.setPWM(i, 0, 125);
//}m
  // Set up LED pins
  pinMode(READY_LED, OUTPUT);
  pinMode(SD_ERROR_LED, OUTPUT);
  pinMode(SMF_ERROR_LED, OUTPUT);

  Serial.begin(SERIAL_RATE);

  DEBUG("\n[MidiFile Play List]");

  // Initialize SD
  if (!SD.begin(SD_SELECT, SPI_FULL_SPEED))
  {
    DEBUG("\nSD init fail!");
    digitalWrite(SD_ERROR_LED, HIGH);
    while (true) ;
  }

  // Initialize MIDIFile
  SMF.begin(&SD);
  SMF.setMidiHandler(midiCallback);
  SMF.setSysexHandler(sysexCallback);

  digitalWrite(READY_LED, HIGH);
}

void tickMetronome(void)
// flash a LED to the beat
{
  static uint32_t	lastBeatTime = 0;
  static boolean	inBeat = false;
  uint16_t	beatTime;

  beatTime = 60000/SMF.getTempo();		// msec/beat = ((60sec/min)*(1000 ms/sec))/(beats/min)
  if (!inBeat)
  {
    if ((millis() - lastBeatTime) >= beatTime)
    {
      lastBeatTime = millis();
      digitalWrite(BEAT_LED, HIGH);
      inBeat = true;
    }
  }
  else
  {
    if ((millis() - lastBeatTime) >= 100)	// keep the flash on for 100ms only
    {
      digitalWrite(BEAT_LED, LOW);
      inBeat = false;
    }
  }
}

void loop(void)
{
    int  err;
  
  for (uint8_t i=0; i<ARRAY_SIZE(tuneList); i++)
  {
    // reset LEDs
    digitalWrite(READY_LED, LOW);
    digitalWrite(SD_ERROR_LED, LOW);

    // use the next file name and play it
    DEBUG("\nFile: ");
    DEBUG(tuneList[i]);
    SMF.setFilename(tuneList[i]);
    err = SMF.load();
    if (err != -1)
    {
    DEBUG("\nSMF load Error ");
    DEBUG(err);
    digitalWrite(SMF_ERROR_LED, HIGH);
    delay(WAIT_DELAY);
    }
    else
    {
    // play the file
    while (!SMF.isEOF())
    {
      
      if (SMF.getNextEvent())
      tickMetronome();
      for (int i=0; i < NUM_NOTES; i++){
        int time = millis();
      if (servoarr[i] && time - servoarr[i] > 500){
           Serial.println(time);
           Serial.print("Millisconds");
            pwm1.setPWM(i, 0, 125);
            Serial.println("reset");
            servoarr[i] = 0;
      }
  }
    }

    // done with this one
    SMF.close();
    midiSilence();

    // signal finish LED with a dignified pause
    digitalWrite(READY_LED, HIGH);
    delay(WAIT_DELAY);
    }
  }
}

