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

//Interface Definitions
int     RxPin      = 8;   //The number of signal from the Rx
int     ledPin     = 13;  //The number of the onboard LED pin

// Variables for Manchester Receiver Logic:
word    sDelay     = 250;  //Small Delay about 1/4 of bit duration  try like 250 to 500
word    lDelay     = 500;  //Long Delay about 1/2 of bit duration  try like 500 to 1000, 1/4 + 1/2 = 3/4
byte    polarity   = 0;    //0 for lo->hi==1 or 1 for hi->lo==1 for Polarity, sets tempBit at start
byte    tempBit    = 1;    //Reflects the required transition polarity
byte    discards   = 1;    //how many leading "bits" need to be dumped, usually just a zero if anything eg discards=1
byte    discNos    = 0;    //counter of how many discarded
boolean firstZero  = false;//has it processed the first zero yet?  This a "sync" bit.
boolean noErrors   = true; //flags if signal does not follow Manchester conventions
//variables for Header detection
byte    headerBits = 10;   //The number of ones expected to make a valid header
byte    headerHits = 0;    //Counts the number of "1"s to determine a header
//Variables for Byte storage
byte    dataByte   = 0;    //Accumulates the bit information
byte    nosBits    = 0;    //Counts to 8 bits within a dataByte
byte    maxBytes   = 9;    //Set the bytes collected after each header. NB if set too high, any end noise will cause an error
byte    nosBytes   = 0;    //Counter stays within 0 -> maxBytes
//Variables for multiple packets
byte    bank       = 0;    //Points to the array of 0 to 3 banks of results from up to 4 last data downloads 
byte    nosRepeats = 0;    //Number of times the header/data is fetched at least once or up to 4 times
//Banks for multiple packets if required (at least one will be needed)
byte  manchester[4][20];   //Stores 4 banks of manchester pattern decoded on the fly

void setup() {
  Serial.begin(115200);
  pinMode(RxPin, INPUT);
  pinMode(ledPin, OUTPUT);
  Serial.println("Debug Manchester Rx+Tx");
  lDelay=2*sDelay;
  Serial.print("Using a delay of 1/4 bitWaveform ");
  Serial.print(sDelay,DEC);
  Serial.print(" uSecs 1/2 bitWaveform ");
  Serial.print(lDelay,DEC);
  Serial.println(" uSecs ");
  if (polarity){
    Serial.println("Negative Polarity hi->lo=1"); 
  }
  else{
    Serial.println("Positive Polarity lo->hi=1"); 
  }
  Serial.print(headerBits,DEC); 
  Serial.println(" bits expected for a valid header"); 
  if (discards){
    Serial.print(discards,DEC);
    Serial.println(" leading bits discarded from Packet"); 
  }
  else{
    Serial.println("All bits inside the Packet"); 
  }
  Serial.println("D 00 00001111 01 22223333 02 44445555 03 66667777 04 88889999 05 AAAABBBB 06 CCCCDDDD 07 EEEEFFFF 08 00001111 90 22223333"); 
  eraseManchester();
}

// Main routines, find header, then sync in with it, get a packet, and decode data in it, plus report any errors.
void loop(){
  tempBit     =polarity^1;
  noErrors    =true;
  firstZero   =false;
  headerHits  =0;
  nosBits     =0;
  nosBytes    =0;
  discNos     =discards;

  while (noErrors && (nosBytes<maxBytes)){
    while(digitalRead(RxPin)!=tempBit){
    }
    delayMicroseconds(sDelay);
    digitalWrite(ledPin,0); 
    if (digitalRead(RxPin)!=tempBit){
      noErrors=false;
    }
    else{
      byte bitState = tempBit ^ polarity;
      delayMicroseconds(lDelay);
      if(digitalRead(RxPin)==tempBit){ 
        tempBit = tempBit^1;
      }
      if(bitState==1){ 
        if(!firstZero){
          headerHits++;
          if (headerHits==headerBits){
            digitalWrite(ledPin,1);
          }
        }
        else{
          add(bitState);
        }
      }
      else{
        if(headerHits<headerBits){
          noErrors=false;
        }
        else{
          if ((!firstZero)&&(headerHits>=headerBits)){
            firstZero=true;
          }
          add(bitState);
        }
      }
    }
  }
  digitalWrite(ledPin,0);
}

void analyseData(){ 
}

void add(byte bitData){
  if (discNos>0){ 
    discNos--;
  }
  else{ //add bit into byte
    dataByte=(dataByte<<1)|bitData;
    nosBits++;
    if (nosBits==8){ //add byte into bank
      nosBits=0;
      manchester[bank][nosBytes]=dataByte;
      nosBytes++;
    }
    if(nosBytes==maxBytes){
      if (checkBanks()){
        hexBinDump();//show the data
        nosBytes=0;
        bank=0;
        eraseManchester(); //"jumble it up"
      }
      bank=(bank+1)%4;  //modulo 4 bank pointer
    }
  }
}

//Properly dump the banks in formatted hex and Binary with leading zeroes
void hexBinDump(){
  Serial.print("D ");
  for( int i=0; i < maxBytes; i++){ 
    byte mask = B10000000;
    if (manchester[bank][i]<16){
      Serial.print("0");
    }
    Serial.print(manchester[bank][i],HEX);
    Serial.print(" ");
    for (int k=0; k<8; k++){
      if (manchester[bank][i] & mask){
        Serial.print("1");
      }
      else{
        Serial.print("0");
      }
      mask = mask >> 1;
    }
    Serial.print(" ");
  }
  Serial.println();
}

void eraseManchester(){
  for( int j=0; j < 4; j++){ 
    for( int i=0; i < 20; i++){ 
      manchester[j][i]=j+i;//make each byte in each bank different
    }
  }
}

boolean checkBanks(){  //checks one bank against the other three
  //check the four versions are the same, best quality data, maybe too high?
  boolean valid = true;
  for( int i=1; i < 4; i++){  //Checking all four banks
    for (int j=0; j<maxBytes; j++){  //NB we are checking up to maxBytes
      if (manchester[0][j] != manchester[i][j]){
        valid = false;
      }
    }
  }
  return valid;
}


