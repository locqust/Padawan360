// =======================================================================================
// /////////////////////////Padawan Servo Expander Code///////////////////////////////////
// =======================================================================================
//                                 BY Gus (GUCABE)
//                        Revised  Date: 01/03/17
//         Designed to be used with  the Padawan Body code by Danf
//              EasyTransfer and PS2X_lib libraries by Bill Porter
//                         Contributions added by MABC
//
//         You will need an Adafruit 16-ch PWM/Servo driver connected via I2C
//         https://www.adafruit.com/products/815
//
//         This program is free software: you can redistribute it and/or modify it .
//         This program is distributed in the hope that it will be useful,
//         but WITHOUT ANY WARRANTY; without even the implied warranty of
//         MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.


///////include libs and declare variables////////////////////////////
#include <Sabertooth.h>
#include <SyRenSimplified.h>
#include <SoftwareSerial.h>
#include <MP3Trigger.h>
MP3Trigger mp3Trigger;
#include <SoftEasyTransfer.h>
//Adafruit servo driver
#include <Wire.h>
#include <XBOXRECV.h>
#include <Servos.h>
#include <Adafruit_PWMServoDriver.h>


//set these 3 to whatever speeds work for you. 0-stop, 127-full speed.
const byte DRIVESPEED1 = 50;
//Recommend beginner: 50 to 75, experienced: 100 to 127, I like 100.
const byte DRIVESPEED2 = 100;
//Set to 0 if you only want 2 speeds.
const byte DRIVESPEED3 = 127;

byte drivespeed = DRIVESPEED1;

// the higher this number the faster the droid will spin in place, lower - easier to control.
// Recommend beginner: 40 to 50, experienced: 50 $ up, I like 70
const byte TURNSPEED = 70;
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



//Adafruit servo driver---------------------------
//it uses the default address 0x40
//you can also call it with a different address you want example:
//Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver(0x41);
//Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver();
Servos BodyServos (0x40);
/*   For Arduino 1.0 and newer, do this:   */
#include <SoftwareSerial.h>
//Adafruit servo driver
SoftwareSerial ServoSerial(2, 18); //For the adafruit 16 servo controller, so it does not interfeare with the standard serial port.

Sabertooth Sabertooth2x(128, Serial1);
Sabertooth Syren10(128, Serial2);

USB Usb;
XBOXRECV Xbox(&Usb);
byte vol = 20; // 0 = full volume, 255 off
//int servomove = 0;
//int servomove2 = 0;

//Servo expander servo # (position on the expander board)
// You can change the value from 0 to 15 (servo 1 through 16)
uint8_t util1servo = 2;
uint8_t util2servo = 4;



//Servo limits definition.
//You will have to figure out which numbers match your servo be carefull with these,
//it can break your servo if you go beyond the limits. The ones listed here are for my own setup
//boolean UTPowerState = true;
//boolean UBPowerState = true;
//unsigned long UTPowerTime;
//unsigned long UBPowerTime;
//
//
//
//int DPowerDelay=700;
//int UPowerDelay=1200;
//
//int UArmTopSpot[3] = {930, 1539, 1052};     //  Open/Close/Mid points
//int UArmBottomSpot[3] = {1582, 2127, 1827};
//
//
//int UArmTop=2;                              //  Servo number on controller
boolean UArmTopCheck = false;             //  Check value, open = true, closed = false
//int UArmBottom=4;
boolean UArmBottomCheck = false;
//int UArmSpeed = 0;
int UTIL1MIN = 1539; // Close position of the Top Utility arm
int UTIL1MAX = 930; // Open position of the Top Utility arm
int UTIL2MIN = 1582; // Close position of the Bottom Utility arm
int UTIL2MAX = 892; // Open position of the Bottom Utility arm



boolean isDriveEnabled = false;

// Automated function variables
// Used as a boolean to turn on/off automated functions like periodic random sounds and periodic dome turns
boolean isInAutomationMode = false;
unsigned long automateMillis = 0;
byte automateDelay = random(5, 20); // set this to min and max seconds between sounds
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

void setup() {
  // set servos to the intitial closed position
  ServoSerial.begin(9600);//Adafruit servo driver
#ifdef ESP8266
  Wire.pins(2, 14);   // ESP8266 can use any two pins, such as SDA to #2 and SCL to #14
#endif

//  pwm.begin();
//
//  pwm.setPWMFreq(60);  // Analog servos run at ~60 Hz updates
//  yield(); //Adafruit servo driver


  mp3Trigger.setup();
  mp3Trigger.setVolume(vol);
//BodyServos.moveTo(UArmTop, UArmSpeed, UArmTopSpot[0], UArmTopSpot[0]); //  Close all servos/doors
//BodyServos.moveTo(UArmBottom, UArmSpeed, UArmBottomSpot[0], UArmBottomSpot[0]);
 //ServosOff();
//  UArmTopCheck = false;
//  UArmBottomCheck = false;
//BodyServos.moveTo(util1servo,0,UTIL1MIN);//Top utility arm set point
//BodyServos.moveTo(util2servo,0,UTIL2MIN);//Bottom utility arm set point


  while (!Serial);
  if (Usb.Init() == -1) {
    //Serial.print(F("\r\nOSC did not start"));
    while (1); //halt
  }

}

void loop() {
  Usb.Task();
  // if we're not connected, return so we don't bother doing anything else.
  // set all movement to 0 so if we lose connection we don't have a runaway droid!
  // a restraining bolt and jawa droid caller won't save us here!
  if (!Xbox.XboxReceiverConnected || !Xbox.Xbox360Connected[0]) {
    Sabertooth2x.drive(0);
    Sabertooth2x.turn(0);
    Syren10.motor(1, 0);
    firstLoadOnConnect = false;
    // If controller is disconnected, but was in automation mode, then droid will continue
    // to play random sounds and dome movements

  }

  // After the controller connects, Blink all the LEDs so we know drives are disengaged at start
  if (!firstLoadOnConnect) {
    firstLoadOnConnect = true;
    mp3Trigger.play(21);
    Xbox.setLedMode(ROTATING, 0);
    manuallyDisabledController = false;
    //triggerI2C(10, 0);
    //domeData.ctl = 1; domeData.dsp = 0; ET.sendData();
    //Tell the dome that the controller is now connected
  }

  if (Xbox.getButtonClick(XBOX, 0)) {
    if (Xbox.getButtonPress(L1, 0) && Xbox.getButtonPress(R1, 0)) {
      manuallyDisabledController = true;
      Xbox.disconnect(0);
    }
  }

  // Volume Control of MP3 Trigger
  // Hold R1 and Press Up/down on D-pad to increase/decrease volume
  if (Xbox.getButtonClick(UP, 0)) {
    // volume up
    if (Xbox.getButtonPress(L1, 0)) {
      if (vol > 0) {
        vol--;
        mp3Trigger.setVolume(vol);
      }
    }
        if (Xbox.getButtonPress(R1, 0)){
//          if (UArmTopCheck = false) {
//          UTPowerState=true;
//          BodyServos.moveTo(UArmTop, UArmSpeed, UArmTopSpot[0], UArmTopSpot[0]);
//          UArmTopCheck = true;
           //ServosOff();
//BodyServos.moveTo(util1servo, 1, UTIL1MAX);
BodyServos.moveTo(util1servo, 1, 930);
//UArmTopCheck = true;
BodyServos.setPWM(2,0,0);
//pwm.setPWM(util1servo, 0, UTIL1MAX);
// }
        }
    if (Xbox.getButtonPress(R2, 0)) {
// if (UArmBottomCheck = false) {
//      UBPowerState=true;
//      BodyServos.moveTo(UArmBottom, UArmSpeed, UArmBottomSpot[0], UArmBottomSpot[0]);
//      UArmBottomCheck = true;
//       //ServosOff();
//   BodyServos.moveTo(util2servo, 1, UTIL2MAX);
     BodyServos.moveTo(util2servo, 1,892);
//   UArmBottomCheck = true;
   BodyServos.setPWM(4,0,0);
//pwm.setPWM(util2servo, 0, UTIL2MAX);
// }
    }
  }

  
  if (Xbox.getButtonClick(DOWN, 0)) {
    //volume down
    if (Xbox.getButtonPress(L1, 0)) {
      if (vol < 255) {
        vol++;
        mp3Trigger.setVolume(vol);
      }
    }
   if (Xbox.getButtonPress(R1, 0)) {
// if (UArmTopCheck = true) {
//        UTPowerState=true;
//        BodyServos.moveTo(UArmTop, UArmSpeed, UArmTopSpot[1], UArmTopSpot[1]);
//        UArmTopCheck = false;
//         //ServosOff();
   //  BodyServos.moveTo(util1servo, 0, UTIL1MIN);
          BodyServos.moveTo(util1servo, 0, 1539);
//     UArmTopCheck = false;
     BodyServos.setPWM(2,0,0);
//pwm.setPWM(util1servo, 0, UTIL1MIN);
//     }
   }
    if (Xbox.getButtonPress(R2, 0)) {
//    if (UArmBottomCheck = true) {
//      UBPowerState=true;
//      BodyServos.moveTo(UArmBottom, UArmSpeed, UArmBottomSpot[0], UArmBottomSpot[0]);
//      UArmBottomCheck = false;
//       //ServosOff();
  //     BodyServos.moveTo(util2servo, 0, UTIL2MIN);
        BodyServos.moveTo(util2servo, 0, 1582);
//       UArmBottomCheck = false;
       BodyServos.setPWM(4,0,0);
//pwm.setPWM(util2servo, 0, UTIL2MIN);
//  }
    }
  }
  
  // Logic display brightness.
  // Hold L1 and press up/down on dpad to increase/decrease brightness
  if (Xbox.getButtonClick(RIGHT, 0)) {
    if (Xbox.getButtonPress(L1, 0)) {

    }
  }
  if (Xbox.getButtonClick(LEFT, 0)) {
    if (Xbox.getButtonPress(L1, 0)) {

    }
  }
  

 


  ////// Utility arms--------------------------------------------------------------------------------------

  //////Top Utility arm

//  if (Xbox.getButtonPress(UP, 0)) && (Xbox.getButtonPress(R1, 0)){
//    // volume up
//      //  BodyServos.moveTo(UArmTop, UArmSpeed, UArmTopSpot[0], UArmTopSpot[0]);
//      //  UArmTopCheck = true;
//      BodyServos.moveTo(util1servo, 2500, UTIL1MAX);
//      //pwm.setPWM(util1servo, 0, UTIL1MAX);
//  }
//  if (Xbox.getButtonPress(DOWN, 0)) && (Xbox.getButtonPress(R1, 0)) {
//    // volume up
//      //        BodyServos.moveTo(UArmTop, UArmSpeed, UArmTopSpot[1], UArmTopSpot[1]);
//      //  UArmTopCheck = false;
//      BodyServos.moveTo(util1servo, 2500, UTIL1MIN);
//      //pwm.setPWM(util1servo, 0, UTIL1MIN);
//  }

  /// //Bottom Utility arm
//
//  if (Xbox.getButtonPress(UP, 0)) && (Xbox.getButtonPress(R2, 0)) {
//    // volume up
////      BodyServos.moveTo(UArmBottom, UArmSpeed, UArmBottomSpot[0], UArmBottomSpot[0]);
////      UArmBottomCheck = true;
//      BodyServos.moveTo(util2servo, 2500, UTIL2MAX);
//      //pwm.setPWM(util2servo, 0, UTIL2MAX);
//  }
//  if (Xbox.getButtonPress(DOWN, 0)) && (Xbox.getButtonPress(R2, 0)) {
////      BodyServos.moveTo(UArmBottom, UArmSpeed, UArmBottomSpot[0], UArmBottomSpot[0]);
////      UArmBottomCheck = true;
//      BodyServos.moveTo(util2servo, 2500, UTIL2MIN);
//      //pwm.setPWM(util2servo, 0, UTIL2MIN);
//  }

 

}

//void ServosOff(){
//  // if delay passed, power on, and both closed, then turn off utility arm supply.
//  if((millis()>UTPowerTime)&&(UTPowerState==true)&&(UArmTopCheck == false)){
//    UTPowerState=false;
//    BodyServos.setPWM(2,0,0);
//  }
//  if((millis()>UBPowerTime)&&(UBPowerState==true)&&(UArmBottomCheck == false)){
//    UBPowerState=false;
//    BodyServos.setPWM(4,0,0);
//  }
//}  
