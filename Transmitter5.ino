/* 
 Manchester Decoding, reading by delay rather than interrupt
 Rob Ward August 2014
 This example code is in the public domain.
 Use at your own risk, I will take no responsibility for any loss whatsoever its deployment.
 Visit https://github.com/robwlakes/433MHz_Tx_Rx 
 for the latest version and documentation. 
 Filename: Receiver1.ino  (receives the broadcast)
 Filename: Transmitter5.ino (makes the broadcast)
 */

const int TxPin = 8; // the TxPin applied to the 433MHz modulator pin
const int ledPin = 13; // the number of the onboard LED pin

byte thisBit; //reflects the current data bit polarity,

byte manchester[16]; //stores a bank of data to be manchester encoded on the fly, say first 8 bytes
byte bytePointer = 0; //increments through manchester[data]
byte maxBytes = 9; //number of bytes transmitted

word sDelay = 500;   //1/2 of bit pattern duration, begin with 500uS (a common choice, data bit length = 1ms)

void setup(){
  pinMode(TxPin, OUTPUT);
  pinMode(ledPin, OUTPUT);
  //The following format relates to the Weather stations, mainly because I wanted to incorporate
  //extra data from other remote RF sensors into my WWW server's weather pages.
  //This would work with the now legacy Bios or Thermor series (They repeat data 4 times to detect errors). 
  //Hence I need to be able to make the Tx use a compatible protocol to simplify the receiver program logic
  //that decodes the protocol.  As the Tx mimics a sensor it would need to alert the receiver to be able to 
  //distinguish multiple transmitters/sensors apart.  ie different ID number's etc so they don't clash with
  // the existing transmitters. 
  //Obviously other users may like to determine their own format or adapt it to their own weather station protocols.
  //
  //Dummy data in the Manchester Byte Array (just to test it out).
  manchester[0] = B01110001; //eg Brand/Sensor ID, can be from 0 to 255
  manchester[1] = B00000111; //eg Packet type (wind, rain, solar, UV etc)
  manchester[2] = 2; //dummy data
  manchester[3] = B10101010; //dummy data
  manchester[4] = B11110000; //dummy data
  manchester[5] = 5; //dummy data, incremented
  manchester[6] = 6; //dummy data, decremented
  manchester[7] = 7; //dummy data
  manchester[8] = B00110011; //dummy data
  Serial.begin(115200);
  Serial.println("Transmitter On Air....");//Just to check if you like
} 

void loop(){
  digitalWrite(ledPin,HIGH);
  manchester[5]++;//just tickle over the bits in the number to show it can change
  manchester[6]--;//as above
  //repeat 4 times over as a simple, but very effective, "checksum" system to validate data
  for (int packet=0;packet<4;packet++){ 
    digitalWrite(TxPin,0);   //begin on Signal Off
    for (int j=0;j<30;j++){  //header 30 of 1's, probably excessive, 20 could be enough
      Tx1(); //Send 1's as it stabilises the Rx's AGC the quickest
    }
    Tx0(); //put in a single 0 as the first "Start Bit", indicates end of Header bit stream of 1's
    bytePointer = 0; //Point to start of manchester array
    for (bytePointer =0;bytePointer<maxBytes;bytePointer++){
      byte mask = B10000000; //MSB -> LSB
      for (int k=0;k<8;k++){
        thisBit =  mask & manchester[bytePointer]; //thisBit!=0 means the bit in that spot is a 1
        if (thisBit == 0){
          Tx0(); //Send a 0
        }
        else{
          Tx1(); //Send a 1
        }
        mask = mask >> 1;//move to the next bit in manchester[bytePointer]
      }//end of checking a byte, 1 bit a a time MSB to LSB
    }//end of the machester[bytePointer]
    digitalWrite(TxPin,0);  //End packet on Signal Off
    delay(20); //break between repeating packets
  }//end of 4 repeats of packet data burst
  digitalWrite(ledPin,LOW); //just for debugging
  //Wait for 30 Seconds before next transmission
  for (int second=0;second<30;second++){
    delay(1000); //a Second
  }
}

//The definitions that follow are used by numerous authors (e.g., Stallings) 
//See www.wikipedia.com
void Tx1(){
  digitalWrite(TxPin,0);//put in a 'low' here so the next transition makes sense
  delayMicroseconds(sDelay);
  digitalWrite(TxPin,1);//transition from low to a high indicates a 1 sent
  delayMicroseconds(sDelay);
}

void Tx0(){
  digitalWrite(TxPin,1);//put in a 'high' here so the next transition makes sense
  delayMicroseconds(sDelay);
  digitalWrite(TxPin,0);//transition from high to a low indicates a 0 sent
  delayMicroseconds(sDelay);
}
