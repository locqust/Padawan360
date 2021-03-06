// =======================================================================================
// /////////////////////////Padawan360 Body Code v1.6 ////////////////////////////////////
// =======================================================================================
/*
v1.6 by Andy Smith (locqust)
-added psybertech's code used to control Flthy HP via serial to overcome I2C issues
v1.5 by Andy Smith (locqust)
- added in I2C command codes for magic panel 
- added in Flthy I2C command codes (currently not working)
v1.4 by Andy Smith (locqust)
-reactivated I2C for Teeces
v1.3 by Andy Smith (locqust)
-added in code to move automation to a function so can leave him chatting without controller being on
- code kindly supplied by Tony (DRK-N1GT)
v1.2 by Andy Smith (locqust)
-added in my list of expanded sound files and their associated command triggers. Sound files taken from BHD's setup
v1.1
by Robert Corvus
- fixed inverse throttle on right turn
-   where right turn goes full throttle on slight right and slow when all the way right
- fixed left-shifted deadzone for turning
- fixed left turn going in opposite direction
- fixed default baud rate for new syren10
- clarified variable names
v1.0
by Dan Kraus
dskraus@gmail.com
Astromech: danomite4047

Heavily influenced by DanF's Padwan code which was built for Arduino+Wireless PS2
controller leveraging Bill Porter's PS2X Library. I was running into frequent disconnect
issues with 4 different controllers working in various capacities or not at all. I decided
that PS2 Controllers were going to be more difficult to come by every day, so I explored
some existing libraries out there to leverage and came across the USB Host Shield and it's
support for PS3 and Xbox 360 controllers. Bluetooth dongles were inconsistent as well
so I wanted to be able to have something with parts that other builder's could easily track
down and buy parts even at your local big box store.

Hardware:
Arduino UNO
USB Host Shield from circuits@home
Microsoft Xbox 360 Controller
Xbox 360 USB Wireless Reciver
Sabertooth Motor Controller
Syren Motor Controller
Sparkfun MP3 Trigger

This sketch supports I2C and calls events on many sound effect actions to control lights and sounds.
It is NOT set up for Dan's method of using the serial packet to transfer data up to the dome
to trigger some light effects. If you want that, you'll need to reference DanF's original
Padawan code.

Set Sabertooth 2x25/2x12 Dip Switches 1 and 2 Down, All Others Up
For SyRen Simple Serial Set Switches 1 and 2 Down, All Others Up
For SyRen Simple Serial Set Switchs 2 & 4 Down, All Others Up
Placed a 10K ohm resistor between S1 & GND on the SyRen 10 itself

*/

//************************** Set speed and turn speeds here************************************//

//set these 3 to whatever speeds work for you. 0-stop, 127-full speed.
const byte DRIVESPEED1 = 50;
//Recommend beginner: 50 to 75, experienced: 100 to 127, I like 100.
const byte DRIVESPEED2 = 100;
//Set to 0 if you only want 2 speeds.
const byte DRIVESPEED3 = 127;

byte drivespeed = DRIVESPEED1;

// the higher this number the faster the droid will spin in place, lower - easier to control.
// Recommend beginner: 40 to 50, experienced: 50 $ up, I like 70
const byte TURNSPEED = 40;
// If using a speed controller for the dome, sets the top speed. You'll want to vary it potenitally
// depending on your motor. My Pittman is really fast so I dial this down a ways from top speed.
// Use a number up to 127 for serial
const byte DOMESPEED = 90;

// Ramping- the lower this number the longer R2 will take to speedup or slow down,
// change this by incriments of 1
const byte RAMPING = 5;

// Compensation is for deadband/deadzone checking. There's a little play in the neutral zone
// which gets a reading of a value of something other than 0 when you're not moving the stick.
// It may vary a bit across controllers and how broken in they are, sometimex 360 controllers
// develop a little bit of play in the stick at the center position. You can do this with the
// direct method calls against the Syren/Sabertooth library itself but it's not supported in all
// serial modes so just manage and check it in software here
// use the lowest number with no drift
// DOMEDEADZONERANGE for the left stick, DRIVEDEADZONERANGE for the right stick
const byte DOMEDEADZONERANGE = 20;
const byte DRIVEDEADZONERANGE = 20;


// Set the baude rate for the Sabertooth motor controller (feet)
// 9600 is the default baud rate for Sabertooth packet serial.
// for packetized options are: 2400, 9600, 19200 and 38400. I think you need to pick one that works
// and I think it varies across different firmware versions.
const int SABERTOOTHBAUDRATE = 9600;

// Set the baude rate for the Syren motor controller (dome)
// for packetized options are: 2400, 9600, 19200 and 38400. I think you need to pick one that works
// and I think it varies across different firmware versions.
const int DOMEBAUDRATE = 9600;

//String hpEvent = "";
//char char_array[11];

// I have a pin set to pull a relay high/low to trigger my upside down compressed air like R2's extinguisher
#define EXTINGUISHERPIN 3



#include <Sabertooth.h>
#include <SyRenSimplified.h>
#include <Servo.h>
#include <MP3Trigger.h>
#include <Wire.h>
#include <XBOXRECV.h>
#include <SoftwareSerial.h>

//Serial connection to Flthys HP's
#define FlthyTXPin 14 // OR OTHER FREE DIGITAL PIN
#define FlthyRXPin 15 // OR OTHER FREE DIGITAL PIN  // NOT ACTUALLY USED, BUT NEEDED FOR THE SOFTWARESERIAL FUNCTION
const int FlthyBAUD = 9600; // OR SET YOUR OWN BAUD AS NEEDED
SoftwareSerial FlthySerial(FlthyRXPin, FlthyTXPin); 

//#include <SoftwareSerial.h>
// These are the pins for the Sabertooth and Syren
//SoftwareSerial Sabertooth2xSerial(NOT_A_PIN, 4);
//SoftwareSerial Syren10Serial(2, 5);

Sabertooth Sabertooth2x(128, Serial1);
Sabertooth Syren10(128, Serial2);
/////////////////////////////////////////////////////////////////
//Sabertooth Sabertooth2x(128, Sabertooth2xSerial);
//#if defined(SYRENSIMPLE)
//SyRenSimplified Syren10(Syren10Serial); // Use SWSerial as the serial port.
//#else
//Sabertooth Syren10(128, Syren10Serial);
//#endif

// Satisfy IDE, which only needs to see the include statment in the ino.
#ifdef dobogusinclude
#include <spi4teensy3.h>
#endif

// Set some defaults for start up
// 0 = full volume, 255 off
byte vol = 20;
// 0 = drive motors off ( right stick disabled ) at start
boolean isDriveEnabled = false;

// Automated function variables
// Used as a boolean to turn on/off automated functions like periodic random sounds and periodic dome turns
boolean isInAutomationMode = false;
unsigned long automateMillis = 0;
byte automateDelay = random(5,20);// set this to min and max seconds between sounds
//How much the dome may turn during automation.
int turnDirection = 20;
// Action number used to randomly choose a sound effect or a dome turn
byte automateAction = 0;
char driveThrottle = 0;
char rightStickValue = 0;
char domeThrottle = 0;
char turnThrottle = 0;

boolean firstLoadOnConnect = false;
boolean manuallyDisabledController = false;

// this is legacy right now. The rest of the sketch isn't set to send any of this
// data to another arduino like the original Padawan sketch does
// right now just using it to track whether or not the HP light is on so we can
// fire the correct I2C event to turn on/off the HP light.
//struct SEND_DATA_STRUCTURE{
//  //put your variable definitions here for the data you want to send
//  //THIS MUST BE EXACTLY THE SAME ON THE OTHER ARDUINO
//  int hpl; // hp light
//  int dsp; // 0 = random, 1 = alarm, 5 = leia, 11 = alarm2, 100 = no change
//};
//SEND_DATA_STRUCTURE domeData;//give a name to the group of data

boolean isHPOn = false;



MP3Trigger mp3Trigger;
USB Usb;
XBOXRECV Xbox(&Usb);

//=====================================
//   SETUP                           //
//=====================================

void setup(){

 Serial1.begin(SABERTOOTHBAUDRATE);
  Serial2.begin(DOMEBAUDRATE);

  //Flthy HP
  FlthySerial.begin(FlthyBAUD);

#if defined(SYRENSIMPLE)
  Syren10.motor(0);
#else
  Syren10.autobaud();
#endif

  // Send the autobaud command to the Sabertooth controller(s).
  /* NOTE: *Not all* Sabertooth controllers need this command.
    It doesn't hurt anything, but V2 controllers use an
    EEPROM setting (changeable with the function setBaudRate) to set
    the baud rate instead of detecting with autobaud.
    If you have a 2x12, 2x25 V2, 2x60 or SyRen 50, you can remove
    the autobaud line and save yourself two seconds of startup delay.
  */
  Sabertooth2x.autobaud();
  // The Sabertooth won't act on mixed mode packet serial commands until
  // it has received power levels for BOTH throttle and turning, since it
  // mixes the two together to get diff-drive power levels for both motors.
  Sabertooth2x.drive(0);
  Sabertooth2x.turn(0);


  Sabertooth2x.setTimeout(950);
  Syren10.setTimeout(950);

  pinMode(EXTINGUISHERPIN, OUTPUT);
  digitalWrite(EXTINGUISHERPIN, HIGH);

  mp3Trigger.setup();
  mp3Trigger.setVolume(vol);


  // Start I2C Bus. The body is the master.
  Wire.begin();
    mp3Trigger.play(23);
   //Serial.begin(115200);
  // Wait for serial port to connect - used on Leonardo, Teensy and other boards with built-in USB CDC serial connection
  while (!Serial);
    if (Usb.Init() == -1) {
      //Serial.print(F("\r\nOSC did not start"));
      while (1); //halt
    }
  //Serial.print(F("\r\nXbox Wireless Receiver Library Started"));
}

//============================
//    MAIN LOOP             //
//============================
void loop(){
  Usb.Task();
  // if we're not connected, return so we don't bother doing anything else.
  // set all movement to 0 so if we lose connection we don't have a runaway droid!
  // a restraining bolt and jawa droid caller won't save us here!
  if(!Xbox.XboxReceiverConnected || !Xbox.Xbox360Connected[0]){
    Sabertooth2x.drive(0);
    Sabertooth2x.turn(0);
    Syren10.motor(1,0);
    firstLoadOnConnect = false;
        // If controller is disconnected, but was in automation mode, then droid will continue
    // to play random sounds and dome movements
    if(isInAutomationMode){
      triggerAutomation();
    }
    if(!manuallyDisabledController){
      
    }
    return;
  }

  // After the controller connects, Blink all the LEDs so we know drives are disengaged at start
  if(!firstLoadOnConnect){
    firstLoadOnConnect = true;
    mp3Trigger.play(21);
    Xbox.setLedMode(ROTATING, 0);
    manuallyDisabledController=false;
    //triggerI2C(10, 0);
    //domeData.ctl = 1; domeData.dsp = 0; ET.sendData();  
    //Tell the dome that the controller is now connected
  }
  
  if (Xbox.getButtonClick(XBOX, 0)) {
    if(Xbox.getButtonPress(L1, 0) && Xbox.getButtonPress(R1, 0)){
      manuallyDisabledController=true;
      Xbox.disconnect(0);
    }
  }

  // enable / disable right stick (droid movement) & play a sound to signal motor state
  if(Xbox.getButtonClick(START, 0)) {
    if(isDriveEnabled){
      isDriveEnabled = false;
      Xbox.setLedMode(ROTATING, 0);
      mp3Trigger.play(53);
    } else {
      isDriveEnabled = true;
      mp3Trigger.play(52);
      // //When the drive is enabled, set our LED accordingly to indicate speed
      if(drivespeed == DRIVESPEED1){
        Xbox.setLedOn(LED1, 0);
      } else if(drivespeed == DRIVESPEED2 && (DRIVESPEED3!=0)){
        Xbox.setLedOn(LED2, 0);
      } else {
        Xbox.setLedOn(LED3, 0);
      }
    }
  }

  //Toggle automation mode with the BACK button
  if(Xbox.getButtonClick(BACK, 0)) {
    if(isInAutomationMode){
      isInAutomationMode = false;
      automateAction = 0;
      mp3Trigger.play(53);
    } else {
      isInAutomationMode = true;
      mp3Trigger.play(52);
    }
  }

  // Plays random sounds or dome movements for automations when in automation mode
  if(isInAutomationMode){
    triggerAutomation();
  }

  // Volume Control of MP3 Trigger
  // Hold R1 and Press Up/down on D-pad to increase/decrease volume
  if(Xbox.getButtonClick(UP, 0)){
    // volume up
    if(Xbox.getButtonPress(R1, 0)){
      if (vol > 0){
        vol--;
        mp3Trigger.setVolume(vol);
      }
    }
  }
  if(Xbox.getButtonClick(DOWN, 0)){
    //volume down
    if(Xbox.getButtonPress(R1, 0)){
      if (vol < 255){
        vol++;
        mp3Trigger.setVolume(vol);
      }
    }
  }

  // Logic display brightness.
  // Hold L1 and press up/down on dpad to increase/decrease brightness
  if(Xbox.getButtonClick(UP, 0)){
    if(Xbox.getButtonPress(L1, 0)){
      triggerI2C(10, 24);
    }
  }
  if(Xbox.getButtonClick(DOWN, 0)){
    if(Xbox.getButtonPress(L1, 0)){
      triggerI2C(10, 25);
    }
  }


  //FIRE EXTINGUISHER
  // When holding L2-UP, extinguisher is spraying. WHen released, stop spraying

  // TODO: ADD SERVO DOOR OPEN FIRST. ONLY ALLOW EXTINGUISHER ONCE IT'S SET TO 'OPENED'
  // THEN CLOSE THE SERVO DOOR
  if(Xbox.getButtonPress(L1, 0)){
    if(Xbox.getButtonPress(UP, 0)){
      digitalWrite(EXTINGUISHERPIN, LOW);
    } else {
      digitalWrite(EXTINGUISHERPIN, HIGH);
    }
  }


  // GENERAL SOUND PLAYBACK AND DISPLAY CHANGING

  // Y Button and Y combo buttons
  if(Xbox.getButtonClick(Y, 0)){
   if (Xbox.getButtonPress(L1, 0) && Xbox.getButtonPress(R1, 0)) {
      //Addams
      mp3Trigger.play(168);
      //logic lights
      triggerI2C(10, 19);
      //HPEvent Disco for 53s
      FlthySerial.print("A0040|53\r");
      //Magic Panel event - Flash Q
      triggerI2C(20, 28);
  } else if (Xbox.getButtonPress(L1, 0)) {
      //Annoyed
      mp3Trigger.play(8);
      //logic lights, random
      triggerI2C(10, 0);
    } else if (Xbox.getButtonPress(L2, 0)) {
      //Chortle
      mp3Trigger.play(2);
      //logic lights, random
      triggerI2C(10, 0);
    } else if (Xbox.getButtonPress(R1, 0)) {
      //Theme
      mp3Trigger.play(9);
      //logic lights, random
      triggerI2C(10, 0);
      //Magic Panel event - Trace up 1
      triggerI2C(20, 8);
    } else if (Xbox.getButtonPress(R2, 0)) {
      //More Alarms
      mp3Trigger.play(random(56, 71));
      //logic lights, random
      triggerI2C(10, 0);
      //Magic Panel event - FlashAll 5s
      triggerI2C(20, 26);
    } else {
      //Alarms
      mp3Trigger.play(random(13, 17));
      //logic lights, random
      triggerI2C(10, 0);
      //Magic Panel event - FlashAll 5s
      triggerI2C(20, 26);
  }
  }

  // A Button and A combo Buttons
  if(Xbox.getButtonClick(A, 0)){
   if (Xbox.getButtonPress(L1, 0) && Xbox.getButtonPress(R1, 0)) {
       //Gangnam
      mp3Trigger.play(169);
      //logic lights
      triggerI2C(10, 18);
      //HPEvent Disco for 24s
      FlthySerial.print("A0040|24\r");
      //Magic Panel event - Flash Q
      triggerI2C(20, 28);
  } else if (Xbox.getButtonPress(L1, 0)) {
      //shortcircuit
      mp3Trigger.play(6);
      //logic lights
      triggerI2C(10, 6);
      // HPEvent 11 - SystemFailure - I2C
      FlthySerial.print("A0050|10\r");
      //Magic Panel event - Fade Out
      triggerI2C(20, 25);
    } else if (Xbox.getButtonPress(L2, 0)) {
      //scream
      mp3Trigger.play(1);
      //logic lights, alarm
      triggerI2C(10, 1);
      //HPEvent pulse Red for 4 seconds
      FlthySerial.print("A0031|4\r");
      //Magic Panel event - Alert 4s
      triggerI2C(20, 6);
    } else if (Xbox.getButtonPress(R1, 0)) {
       //Imp March
      mp3Trigger.play(11);
      //logic lights, Imperial March
      triggerI2C(10, 11);
      //HPEvent - flash - I2C
      FlthySerial.print("A0030|175\r");
      //magic Panel event - Flash V
      triggerI2C(20, 27);
    } else if (Xbox.getButtonPress(R2, 0)) {
      //More Misc
      mp3Trigger.play(random(71, 99));
      //logic lights, random
      triggerI2C(10, 0);
    } else {
      //Misc noises
      mp3Trigger.play(random(17, 25));
      //logic lights, random
      triggerI2C(10, 0);
    }
  }

  // B Button and B combo Buttons
 if (Xbox.getButtonClick(B, 0)) {
    if (Xbox.getButtonPress(L1, 0) && Xbox.getButtonPress(R1, 0)) {
      //Muppets
      mp3Trigger.play(172);
      //logic lights
      triggerI2C(10, 17);
      //HPEvent Disco for 30s
      FlthySerial.print("A0040|30\r");
      //Magic Panel event - Trace Up 1
      triggerI2C(20, 8);
  } else if (Xbox.getButtonPress(L1, 0)) {
      //patrol
      mp3Trigger.play(7);
      //logic lights, random
      triggerI2C(10, 0);
    } else if (Xbox.getButtonPress(L2, 0)) {
       //DOODOO
      mp3Trigger.play(3);
      //logic lights, random
      triggerI2C(10, 0);
      //Magic Panel event - One loop sequence
      triggerI2C(20, 30);
    } else if (Xbox.getButtonPress(R1, 0)) {
      //Cantina
      mp3Trigger.play(10);
      //logic lights bargrap
      triggerI2C(10, 10);
      // HPEvent 1 - Cantina Music - Disco - I2C
      FlthySerial.print("A0040|165\r");
      //magic Panel event - Trace Down 1
      triggerI2C(20, 10);
    } else if (Xbox.getButtonPress(R2, 0)) {
      //Proc/Razz
      mp3Trigger.play(random(100, 139));
      //logic lights, random
      triggerI2C(10, 0);
    } else {
      //Sent/Hum
      mp3Trigger.play(random(32, 52));
      //logic lights, random
      triggerI2C(10, 0);
      //Magic Panel event - Expand 2
      triggerI2C(20, 17);
    }
  }

  // X Button and X combo Buttons
  if(Xbox.getButtonClick(X, 0)){
   if (Xbox.getButtonPress(L1, 0) && Xbox.getButtonPress(R1, 0)) {
      //Leia Short
      mp3Trigger.play(170);
      //logic lights
      triggerI2C(10, 16);
      //HPEvent hologram for 6s
      FlthySerial.print("F001|6\r"); 
      //magic Panel event - Eye Scan
      triggerI2C(20, 23);
    } else if (Xbox.getButtonPress(L2, 0) && Xbox.getButtonPress(R1, 0)) {
      //Luke message
      mp3Trigger.play(171);
      //logic lights
      triggerI2C(10, 15);
      //HPEvent hologram for 26s
      FlthySerial.print("F001\26");
      //magic Panel event - Cylon Row
      triggerI2C(20, 22);
    }
    // leia message L1+X
    else if (Xbox.getButtonPress(L1, 0)) {
      mp3Trigger.play(5);
      //logic lights, leia message
      triggerI2C(10, 5);
      // Front HPEvent 1 - HoloMessage leia message 35 seconds
      FlthySerial.print("F001|35\r");
      //magic Panel event - Cylon Row
      triggerI2C(20, 22);
    } else if (Xbox.getButtonPress(L2, 0)) {
      //WolfWhistle
      mp3Trigger.play(4);
      //logic lights - whistle
      triggerI2C(10, 4);
      //HPEvent pulse Red for 4 seconds
      FlthySerial.print("A00312|5\r");
      //magic Panel event - Heart
      triggerI2C(20, 40);
    } else if (Xbox.getButtonPress(R1, 0)) {
      //Duel of the Fates
      mp3Trigger.play(12);
      //logic lights, random
      triggerI2C(10, 0);
      //magic Panel event - Flash Q
      triggerI2C(20, 28);
    } else if (Xbox.getButtonPress(R2, 0)) {
      //Proc/Razz
      mp3Trigger.play(random(139, 168));
      //logic lights, random
      triggerI2C(10, 0);
      //magic Panel event - Compress 2
      triggerI2C(20, 19);
    } else  {
      //ohh/sent
      mp3Trigger.play(random(25, 32));
      //logic lights, random
      triggerI2C(10, 0);
    }
  }

  // turn hp light on & off with Left Analog Stick Press (L3)
  if(Xbox.getButtonClick(L3, 0))  {
    // if hp light is on, turn it off
    if(isHPOn){
      isHPOn = false;
      // turn hp light off
      // Front HPEvent 2 - ledOFF - I2C
      FlthySerial.print("A098\r");
      FlthySerial.print("A198\r");
    } else {
      isHPOn = true;
      // turn hp light on
      // Front HPEvent 4 - whiteOn - I2C
      FlthySerial.print("A099\r");
      FlthySerial.print("A199\r");
    }
  }


  // Change drivespeed if drive is enabled
  // Press Right Analog Stick (R3)
  // Set LEDs for speed - 1 LED, Low. 2 LED - Med. 3 LED High
  if(Xbox.getButtonClick(R3, 0) && isDriveEnabled) {
    //if in lowest speed
    if(drivespeed == DRIVESPEED1){
      //change to medium speed and play sound 3-tone
      drivespeed = DRIVESPEED2;
      Xbox.setLedOn(LED2, 0);
      mp3Trigger.play(53);
      //Teeces event
      triggerI2C(10, 22);
      //magic Panel event - AllOn 5s
      triggerI2C(20, 3);
    } else if(drivespeed == DRIVESPEED2 && (DRIVESPEED3!=0)){
      //change to high speed and play sound scream
      drivespeed = DRIVESPEED3;
      Xbox.setLedOn(LED3, 0);
      mp3Trigger.play(1);
      //Teeces event
      triggerI2C(10, 23);
      //magic Panel event - AllOn 10s
      triggerI2C(20, 4);
    } else {
      //we must be in high speed
      //change to low speed and play sound 2-tone
      drivespeed = DRIVESPEED1;
      Xbox.setLedOn(LED1, 0);
      mp3Trigger.play(52);
      //Teeces event
      triggerI2C(10, 21);
      //magic Panel event - AllOn 2s
      triggerI2C(20, 2);
    }
  }


  // FOOT DRIVES
  // Xbox 360 analog stick values are signed 16 bit integer value
  // Sabertooth runs at 8 bit signed. -127 to 127 for speed (full speed reverse and  full speed forward)
  // Map the 360 stick values to our min/max current drive speed
  rightStickValue = (map(Xbox.getAnalogHat(RightHatY, 0), -32768, 32767, -drivespeed, drivespeed));
  if(rightStickValue > -DRIVEDEADZONERANGE && rightStickValue < DRIVEDEADZONERANGE){
    // stick is in dead zone - don't drive
    driveThrottle = 0;
  } else {
    if(driveThrottle < rightStickValue){
      if(rightStickValue - driveThrottle < (RAMPING+1) ){
        driveThrottle+=RAMPING;
      } else {
        driveThrottle = rightStickValue;
      }
    } else if(driveThrottle > rightStickValue){
      if(driveThrottle - rightStickValue < (RAMPING+1) ){
        driveThrottle-=RAMPING;
      } else {
        driveThrottle = rightStickValue;
      }
    }
  }

  turnThrottle = map(Xbox.getAnalogHat(RightHatX, 0), -32768, 32767, -TURNSPEED, TURNSPEED);


  // DRIVE!
  // right stick (drive)
  if (isDriveEnabled){
    // Only do deadzone check for turning here. Our Drive throttle speed has some math applied
    // for RAMPING and stuff, so just keep it separate here
    if(turnThrottle > -DRIVEDEADZONERANGE && turnThrottle < DRIVEDEADZONERANGE){
      // stick is in dead zone - don't turn
      turnThrottle = 0;
    }
    Sabertooth2x.turn(-turnThrottle);
    Sabertooth2x.drive(driveThrottle);
  }

  // DOME DRIVE!
  domeThrottle = (map(Xbox.getAnalogHat(LeftHatX, 0), -32768, 32767, -DOMESPEED, DOMESPEED));
  if (domeThrottle > -DOMEDEADZONERANGE && domeThrottle < DOMEDEADZONERANGE){
    //stick in dead zone - don't spin dome
    domeThrottle = 0;
  }

//  #if defined(SYRENSIMPLE)
//    Syren10.motor(domeThrottle);
//  #else
//    Syren10.motor(1,domeThrottle);
//  #endif
 Syren10.motor(1, domeThrottle);
} // END loop()

//=============================
// FUNCTIONS FOR LOOP        //
//=============================

void triggerI2C(byte deviceID, byte eventID){
  Wire.beginTransmission(deviceID);
  Wire.write(eventID);
  Wire.endTransmission();
}

void triggerAutomation() {
  // Plays random sounds or dome movements for automations when in automation mode
    unsigned long currentMillis = millis();

    if (currentMillis - automateMillis > (automateDelay * 1000)) {
      automateMillis = millis();
      automateAction = random(1, 5);

      if (automateAction > 1) {
        mp3Trigger.play(random(17, 167));
      }
      if (automateAction < 4) {
#if defined(SYRENSIMPLE)
        Syren10.motor(turnDirection);
#else
        Syren10.motor(1, turnDirection);
#endif

        delay(750);

#if defined(SYRENSIMPLE)
        Syren10.motor(0);
#else
        Syren10.motor(1, 0);
#endif

        if (turnDirection > 0) {
          turnDirection = -45;
        } else {
          turnDirection = 45;
        }
      }

      // sets the mix, max seconds between automation actions - sounds and dome movement
      automateDelay = random(5,15);
    }
}
