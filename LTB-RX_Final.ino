/*
  Transmitting Weather information from a "Weather LTB" build on 433Mhz OOP
  
  Uses Manchester Encoding, algorithm by delay rather than interrupt.
  The precision of the data chosen to be transmitted does not always reflect the capacity
  of the sensors eg one byte for temp, wind speed, humidity. However if users want to
  enhance the precision, this challenge is left to increase the packet length and
  the number of bytes transmitted/received and their format. This not difficult.

  This algorithm below has been updated and is now far easier to understand and should
  allow others to adapt to their own designs with more confidence. Highly recommended.
  See below for an explanation of the logic of the decoding.

  Data output can be displayed on a 20x4 LCD using "Adafruit LCD Backpack I2C/SPI" (white on blue),
  or displayed in the Arduino IDE Terminal, or sent as serial/USB to data to a Raspberry Pi for graphing etc
  A Raspberry Pi can provide real time clock via NNTP or an I2C version could be added to the console.

  As far as I know there are no other weather stations using this particular packet structure,
  so do not expect it to be compatible with any other system..

  The choice of 433Mh OOP is somewhat "old School" in this day and age of low power blue tooth, LoRA, WiFi
  etc etc, however on its side it does have range, low power and above all simplicity.

  Rob Ward (C) 2019

  For more detail on how Manchester protocol is structured, please refer to my GITHub site:
      https://github.com/robwlakes/ArduinoWeatherOS
  For an example of what can be done with a Raspberry Pi and CSV weather data, please refer to my website:
      http://www.laketyersbeach.net.au/weather.html
  For the 3-D printer parts (and a Arduino Uno setup) please refer to my Thingiverse account:
      https://www.thingiverse.com/thing:2849562 
*/

//A 3-D printer design for a weather station with anometer, wind direction, rain gauge, humidity and temperature
//Now includes Barometer, internal temperature, LCD, Serial reporting, Serial CSV.

#include <Wire.h>  //general access to I2C devices
#include <Adafruit_BMP085.h> //Atmospheric pressure, Barometer BMP180 (or BMP085).
#include <LiquidCrystal_PCF8574.h> //output to LCD display

// Connect via i2c, default address #0 (A0-A2 not jumpered)
LiquidCrystal_PCF8574 lcd(0x3f);  // set the LCD address to 0x3f for a 20 chars and 4 line display
Adafruit_BMP085 bmp; //create an instance of the barometer/temperature sensor (works with BMP180)

//Wind Direction
char* compass[] = {"N  ", "NNE", "NE ", "ENE", "E  ", "ESE", "SE ", "SSE", "S  ", "SSW", "SW ", "WSW", "W  ", "WNW", "NW ", "NNW", "???"};
byte wdirection = 0; //0ne 0f 16 directions
//Speed Rainfall Humidity Temperature (T2) are gathered from the external sensors
//See below for estimate of mm per click calculations.
float rainfall = 0.0; //raw number of bucket tips from rain gauge *1.374=mm.
//Barometer and Thermometer (these sensors are local on this console)
float  conTemp   = 0.0; //internal temperature (T1) measured at the console
float  barometer = 0.0; //internal air pressure, barometric pressure
// Interface Definitions
int RxPin  =  8; //incoming from 433MHz Rx
int ledPin = 13; //status LED
// Variables for Manchester Receiver Logic:
//Operating on 1mS data pulse,or bit-rate
word    sDelay     = 230;    //Small Delay about 1/4 of bit duration,  try like 220 to 250 (~1/4 bit rate)
word    lDelay     = 480;    //Long Delay about 1/2 of bit duration,  try like 460 to 500, (~1/2 bit rate)
boolean noErrors   = true;   //flags if signal does not follow Manchester conventions for bit waveform
//variables for Header detection
byte    minHeader = 15;     //The minimum number of ones expected to make a valid header (30 are sent from Tx)
//Variables for Byte storage
byte    maxBytes   = 10;     //Set the bytes collected after each header. NB if set too high, any end noise will cause an error
byte    nosBytes   = 0;      //Counter stays within 0 -> maxBytes
//Array for packet data
byte    manchester[12];      //Stores received manchester bits/bytes
//Status data
byte stationID =  0x51;     //Station ID must not exceed 127 as 0 in MSB is synch bit, ie must be a 0

void setup() {
  Serial.begin(115200);
  pinMode(ledPin, OUTPUT);   //for debug purposes
  pinMode(RxPin, INPUT);     //433MHz RX input
  //Barometer
  bmp.begin();    //start the barometer and temp packages
  conTemp = bmp.readTemperature();   //internal temperature
  barometer = bmp.readPressure() / 100.0; //Pa reduced to mBar
  //comment out if using CSV format
  Serial.println("LTB Weather+Wireless Rx by Rob Ward (C) 2019");
  //Comment out if not using LCD
  lcd.begin(20, 4);
  lcd.setBacklight(255);//also controls LCD Back Light
  lcd.clear();
  lcd.print("LTB Weather Rob Ward");
}


// Main routines: Find header, then sync in with it, get a packet, and decode data in it, plus respond to any errors.

void loop() {
  noErrors = true;   // if any errors arise it aborts the packet and begins again
  digitalWrite(ledPin, 0);//Reset status LED, it turns on when a packet begins (not necessarily a valid one)
  byte nextBit = getHeader(minHeader); //get a header of min 10, "1"s
  //Serial.println(nextBit,DEC);
  if (nextBit > 1) {
    noErrors = false; //just check the header has not stopped because of an error
  }
  else {
    digitalWrite(ledPin, 1);
    //nextBit should be 0 here, if 1s in the header
    //Pack bits into Manchester[] bytes
    for (word j = 0; j < maxBytes; j++ ) {
      manchester[j] = 0;
      //OR in the bits to the bytes, MSB first
      for (int k = 7; k > -1; k-- ) {
        manchester[j] = manchester[j] | (nextBit << k);
        //each call gets a requested bit, and returns next request
        nextBit = getBit(nextBit);
        if (nextBit > 1) {
          //Serial.println("Packet Frame error.");
          noErrors = false;
        }
      }
    }
    digitalWrite(ledPin, 0);
    //Serial.println();
    if (noErrors) {
      //hexBinDump(); // Great for debugging
      if (manchester[0] == stationID) {
        if (ValidCS(maxBytes - 1)) {
          analyseData();
        }
        else {
          Serial.println("Checksum error");
        }
      }
      else {
        Serial.println("Wrong station ID");
      }
    }
  }
}

// check incoming signals for a stream of header bits
byte getHeader(byte nos) {
  byte hCounter = 0;
  byte a = 1; //assuming the header is a stream of 1's
  //This assumes 1->0 signal==1, 0->1 signal==0.
  //accumulate header bits while the stream stays the same
  byte b = getBit(a);
  //exit as soon as synch bit encountered b==0, or an error, eg b>1
  while (b == a) {
    hCounter++;
    b = getBit(a);
  }
  if (hCounter < nos) {
    //header stream has changed, if too early probably an error
    return 3;
  }
  else {
    //enough header, so return this value, probably first synch bit
    return b;
  }
}

//look for a nominated bit, return prediction for next bit
byte getBit(byte a) {
  while (digitalRead(RxPin) == a) {
    //stay in this loop until signal changes, and assume mid point in bit frame
  }
  delayMicroseconds(sDelay);// 1/4 delay after the data transition, @3/4 mark
  //check to see signal is steady
  if (digitalRead(RxPin) != a) {
    delayMicroseconds(lDelay);// 1/2 delay, landing at 1/4 mark into next bit frame
    //Check to see what signal transition to expect in the next bit frame
    if (digitalRead(RxPin) == a) {
      return (a); //expect more of the same
    }
    else {
      return (a ^ 1); //EOR bit, now expecting the inverse
    }
  }
  else {
    //the signal has not passed checks for Manchester bit frame shape, so rejected
    digitalWrite(ledPin, 0);
    //Serial.println("Bit sample error");
    return 2;
  }
}

// Very Simple CheckSum Routine, only 9 bytes involved
bool ValidCS(int CSPos) {
  boolean ok = false;
  int cs = 0;
  for (int x = 0; x < CSPos; x++) {
    cs += manchester[x];
  }
  if (manchester[CSPos] == cs % 256) {
    ok = true;
  }
  return ok;
}

//Enable debug dumps if formatted data required
void analyseData() {
  conTemp = bmp.readTemperature();   //console/base station temperature
  barometer = bmp.readPressure() / 100.0; //Pa expresed in mBar
  rainfall = (manchester[4] * 256 + manchester[5]) * 1.37; //See calibration below
  wdirection = manchester[1] & 15; //bottom four bits are one of 16 wind directions
  //hexBinDump();
  send2LCD(); //LCD printout of readings
  send2screen();//send to IDE Terminal
  send2CSV();//send to serial for use by Python data base and graphing results
}

void send2LCD() {
  //first line
  lcd.clear();
  lcd.print("LTB Weather T1:");
  lcd.print(conTemp, 1); //temperature at the base station
  lcd.print("C");
  //second line LCD
  lcd.setCursor(0, 1);
  lcd.print("WSp:");
  lcd.print(float(manchester[2]) / float(32), 0); //Revs per second
  lcd.print("km/h WDir:");
  lcd.print(compass[wdirection]); //lower 4 bits are 16 directions
  //third line LCD
  lcd.setCursor(0, 2);
  lcd.print("Rain:");
  lcd.print(rainfall, 1);
  lcd.print("mm Hum:");
  lcd.print(manchester[6]);
  lcd.print("%");
  //fourth line LCD
  lcd.setCursor(0, 3);
  int temp = manchester[3] - 20; //offset to allow easy -temps
  lcd.print("T2:");
  lcd.print(temp, DEC);//temperature at external sensors
  lcd.print("C Bar:");
  lcd.print(barometer, 0);
  lcd.print("mB ");
}

void send2screen() {
  Serial.print("Station ID: ");
  Serial.println(manchester[0], DEC);
  //Serial.print(manchester[1],DEC);
  Serial.print("Wind Direction = ");
  Serial.print(compass[wdirection]);
  Serial.print(" Av Wind Speed = ");
  Serial.print(float(manchester[2]) / 32, 2); //Shown as RPS here
  Serial.print("  Wind gust = ");
  Serial.println(float(manchester[7]) / 32, 2); //add your own calibration
  Serial.print("Barometer = ");
  Serial.print(barometer, 0);
  Serial.print("  Rainfall = ");
  Serial.print(rainfall, 2);
  Serial.print("  Humidity = ");
  Serial.println(manchester[6], DEC);
  Serial.print("Internal Temperature = ");
  Serial.print(conTemp, 1);
  Serial.print(" External Temperature = ");
  Serial.println(manchester[3] - 20, DEC);
  Serial.println("->->->");
}

void send2CSV() {
  Serial.print(manchester[0], DEC); //station ID
  Serial.print(", ");
  Serial.print(manchester[1] & 15, DEC); //Wind direction
  Serial.print(", ");
  Serial.print(manchester[2], DEC); //Wind speed (ad in your own calibration here)
  Serial.print(", ");
  Serial.print(manchester[3] - 20, DEC); //External temperature
  Serial.print(", ");
  Serial.print(rainfall, 2); //Cumulative rainfall
  Serial.print(", ");
  Serial.print(manchester[6], DEC); //Humidity
  Serial.print(", ");
  Serial.print(manchester[7], DEC); //Wind gust
  Serial.print(", ");
  Serial.print(manchester[1] >> 4, DEC); //Wind direction
  Serial.print(", ");
  Serial.print(manchester[8], DEC);//just send the spare anyway
  Serial.print(", ");
  Serial.print(barometer, 0); //Barometer in mBar
  Serial.print(", ");
  Serial.println(conTemp, 1); //Internal temperature
}

//Useful to invoke to debug the byte Array
// for showing/debuging data array contents
void hexBinDump() {
  //Serial.println("T A3 10100011 07 00000111 02 00000010 AA 10101010 F0 11110000 06 00000110 FF 11111111 07 00000111 33 00110011 60 01100000");
  Serial.print("D ");
  for ( int i = 0; i < maxBytes; i++) {
    byte mask = B10000000;
    if (manchester[i] < 16) { //Formats Hex<16 to 2 digits, adds the leading 0
      Serial.print("0");
    }
    Serial.print(manchester[i], HEX);
    Serial.print(" ");
    for (int k = 0; k < 8; k++) {
      if (manchester[i] & mask) {
        Serial.print("1");
      }
      else {
        Serial.print("0");
      }
      mask = mask >> 1;
    }
    Serial.print(" ");
  }
  Serial.println();
}

/* Rain Gauge Calibration
  3 lots of 300 ml, produced 35,35,33 clicks.
  Area of top (diam~9cm) of catchment = 3.1414*4.5cm)^2=63.61cm^2
  Average number of clicks per 300ml = 34.33clicks
  Depth of water across the ~47ml, or
  47.16mm/34.33=1.37 mm depth per click
  This is a guide only, please conduct your own calibrations to
  determine your particular settings.
*/


/*Explanation of Manchester data bit structure
 
           ON     OFF      ON     OFF   OFF     ON      OFF    ON      OFF
  Sig   1111111D0000000T1111111D0000000000000D1111111T0000000D1111111T0000000
  Data  WWWWWWWWWWWWWWW|XXXXXXXXXXXXXX|YYYYYYYYYYYYYY|ZZZZZZZZZZZZZZZ| boundaries
        _______         _______              ._______        ________
       |       |       |       |             |       |       |       |
       |       v       |       v             ^       |       ^       |
    ___|       |_______|       |______.______|       |_______|       |_______
  Data         1               1             0               0            Timeline
  Label    A   B   C      D    E   F     G   H   I      J    K   L      M
  Program LLLTdddqDDDDDDQLLLLTdddqDDDDDQLLLTdddqDDDDDDQLLLLTdddqDDDDDDQLLLT

  Above is a diagram illustrating the main points about Manchester encoding/decoding.
  The signal has a fairly regular signal pattern. The data stream certainly arrives at regular rate
  but the on/off ratio of the signal can vary when the transmitted bits change value.

  A long stream of even square waves could be either all 1's or all 0's, it just depends on
  which edge is interpretted as the data edge. Only the interpretation of the transitions
  determines whether the signal is conveying 1s or 0s. Notice above the Data signal transitions have
  "D" above them and transitions not carrying data are labelled "T". Between label E&H the data is about
  to change from two data 1s, into two data 0s, but the signal level does not change. Most importantly
  though using this encoding, the average signal time at ON and OFF remains constant. This stabilizes
  the RX's gain and bias to a signal midpoint, and that allows easy differentiation into digital pulses.

  It is convenient that the signal value in the first half of the data bit, in this setup, matches
  the value of the bit as data bit of 1 is high to low, and a data bit of 0 is low to high.

  The stream of Header Bits that precedes the data stabilizes the Rx bias before the packet of
  data is sent. A synchronising bit of the opposite level is required to mark the end of the
  header and the beginning of the packet data. The above example could be the end of the header
  data 1's and the arrival of the first, data 0, synchronising bit. The packet data would follow
  after that. In this system the first 0 data bit is included in the start of the packet data.

  In this system I am using the convention- signal transition 0->1 is 0data, and 1->0 is 1data
  Program Labels:
    WXYZ- Four data bits
    LLL - looping for an expected transition (eg 1->0) (AB, DE) or 0->1 (GH, JK)
    T   - Found a transition (assumed to be middle of data bit) (B,E,H,K)
    ddd - a short wait 1/4 of data bit rate (BC,EF,HI,KL)
    q   - check the signal is stable at 3/4 of data bit (C,F,I,L), if not = error
    DDD - a longer wait, 1/2 bit rate (CD,FG,IJ,LM), extends into 1/4 of next data bit
    Q   - if signal has changed the next data bit is the same as before(D,I,L)
          if the signal is the same, the next data bit will be inverted (G)
    begin again, looping to detect the next data transition.
  NB the program ignores the non-data carrying transitions, and always locks onto the
  the data carrying transitions. This makes this system quite stable and tolerant of
  other varying CPU tasks that could make small changes to timing, eg calculations.
*/
