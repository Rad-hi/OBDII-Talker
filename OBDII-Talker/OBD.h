// Used a local SoftwareSerial lib because SS for arduino and SS for esp
// both have the same name and functions so there was a confusion in paths.
// This could be solved with some paths settings but I was lazy and just put
// the esp-SS lib in this folder and included it locally to save time
#include "SoftwareSerial.h" 

#define OBD_SER_RX             32
#define OBD_SER_TX             33      
#define OBD_BAUD_RATE          38400   // 38.4 Kb/s baud serial to ELM327

////OBD communication states
#define OBD_READY              0
#define OBD_SEND               1
#define OBD_READ               2

//time after which the ELM is considered 
//not responding to the command 
#define OBD_READ_TIMEOUT       1000   // ms
#define OBD_TALK_TIMEOUT       2000   // ms

////OBD init states
#define OBD_START_SERIAL       0
#define OBD_RESET              1
#define OBD_ECHO_OFF           2
#define OBD_LINEFEED_OFF       3
#define OBD_SELECT_PROTOCOL    4
#define OBD_HEADERS_OFF        5

int OBD_state = 0;

byte inData;
char inChar;
String resp = "";
unsigned long timeout_read = 0;
unsigned long timeout_talk = 0;

SoftwareSerial OBD_Serial(OBD_SER_RX, OBD_SER_TX); // RX, TX (CarTX -> Green, CarRX -> White)

////Functions' prototypes
void OBD_init(int);
void OBD_talk(String);


////Functions' declarations
void OBD_init(int init_step){
  switch(init_step){
    case OBD_START_SERIAL:{
      OBD_Serial.begin(OBD_BAUD_RATE); 
      break;
    }
    case OBD_RESET:{
      OBD_talk("atz");
      break;
    }
    case OBD_ECHO_OFF:{
      OBD_talk("ate0");
      break;
    }
    case OBD_LINEFEED_OFF:{
      OBD_talk("atl0");
      break;
    }
    case OBD_SELECT_PROTOCOL:{
      OBD_talk("atsp0");
      break;
    }
    case OBD_HEADERS_OFF:{
      OBD_talk("ath0");
      break;
    }
    default:{
      break;
    }
  }  
}

void OBD_talk(String cmd){
  bool data_received = false;
  timeout_talk = millis();
  resp = "";
  do{
    switch(OBD_state){
      case OBD_READY:
        OBD_state = OBD_SEND;
        break;
      case OBD_SEND:
        OBD_Serial.print(cmd + '\r'); // All ELM327 commands shall end with a carriage return
        
        // We start a timer since we don't want to allow our MCU
        // to keep waiting forever for a response
        timeout_read = millis(); 
        OBD_state = OBD_READ;
        break;
      case OBD_READ:
        do{
          if(OBD_Serial.available() > 0){
            inData = 0;
            inChar = 0;
            inData = OBD_Serial.read(); // Read upcoming byte
            inChar = char(inData); // Convert the byte into a char
            resp += inChar; // Add the read char to the response string
          }
        // The ELM327 finishes all its responses by the '>' char
        }while(inChar != '>'
              && ((millis() - timeout_read) < OBD_READ_TIMEOUT));
        if(inChar == '>'){
          OBD_state = OBD_READY;
          data_received = true;
          break;
        }
        else{
          // If we timedout and we didn't read a valid response,
          // we send again
          OBD_state = OBD_SEND; 
        }
        break;
      default:
        break;
    }
  }while(!data_received && ((millis() - timeout_talk) < OBD_TALK_TIMEOUT));
}
