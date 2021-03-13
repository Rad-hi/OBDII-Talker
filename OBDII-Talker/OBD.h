#include <HardwareSerial.h>  

#define OBD_SER_RX             32
#define OBD_SER_TX             33      
#define OBD_BAUD_RATE          38400   // 38.4 Kb/s baud serial to ELM327

// OBD communication states
#define OBD_READY              0
#define OBD_SEND               1
#define OBD_READ               2

// Time after which the ELM is considered 
// not responding to the command 
#define OBD_READ_TIMEOUT       1000   // ms
#define OBD_TALK_TIMEOUT       2000   // ms

// OBD init states
#define OBD_RESET              0
#define OBD_START_SERIAL       1
#define OBD_ELM_RESET          2
#define OBD_ECHO_OFF           3
#define OBD_LINEFEED_OFF       4
#define OBD_SELECT_PROTOCOL    5
#define OBD_HEADERS_OFF        6
#define OBD_ELM_READY          7

// How much time (ms) to wait between each 2 setup steps 
#define OBD_SETUP_DELAY        50      

// Find all "41 01", and read the next byte in the response string 
// as the number of DTCs for that ECU.
#define READ_DTC__             0
#define FOUND_4_               1
#define FOUND_1                2
#define FOUND_SPACE_           3
#define FOUND_0_               4
#define FOUND_1_               5

// https://www.elmelectronics.com/wp-content/uploads/2016/07/ELM327DS.pdf (page 34) 
const String conversion_ [16] = {"P0","P1","P2","P3",
                                 "C0","C1","C2","C3",
                                 "B0","B1","B2","B3",
                                 "U0","U1","U2","U3"};

// Setup variables
unsigned long OBD_last_setup = 0;      // last time an OBD setup cmd was sent
int  OBD_setup_state = 0;  

// Talking varibales
int OBD_state = 0;
char inChar;
String resp = "";
unsigned long timeout_read = 0;
unsigned long timeout_talk = 0;
uint8_t num_ecu = 0;

// RX, TX (CarTX -> Green, CarRX -> White)
HardwareSerial OBD_Serial(1);

// Functions' prototypes
void OBD_setup();
void OBD_talk(String);
long* get_dtc_number();
void get_dtc_s(String&, long, uint8_t);
String read_dtc_();

// Functions' declarations
void OBD_setup(){
  
  switch(OBD_setup_state){
    case OBD_RESET:{
      OBD_setup_state = OBD_START_SERIAL;
      OBD_last_setup = millis();
      break;
    }
    case OBD_START_SERIAL:{
      if(millis() - OBD_last_setup >= OBD_SETUP_DELAY){
        OBD_Serial.begin(OBD_BAUD_RATE, SERIAL_8N1, OBD_SER_RX, OBD_SER_TX); 
        OBD_setup_state = OBD_ELM_RESET;
        OBD_last_setup = millis();
      }
      break;
    }
    case OBD_ELM_RESET:{
      if(millis() - OBD_last_setup >= OBD_SETUP_DELAY){
        OBD_talk("atz");
        OBD_setup_state = OBD_ECHO_OFF;
        OBD_last_setup = millis();
      }
      break;
    }
    case OBD_ECHO_OFF:{
      if(millis() - OBD_last_setup >= OBD_SETUP_DELAY){
        OBD_talk("ate0");
        OBD_setup_state = OBD_LINEFEED_OFF;
        OBD_last_setup = millis();
      }
      break;
    }
    case OBD_LINEFEED_OFF:{
      if(millis() - OBD_last_setup >= OBD_SETUP_DELAY){
        OBD_talk("atl0");
        OBD_setup_state = OBD_SELECT_PROTOCOL;
        OBD_last_setup = millis();
      }
      break;
    }
    case OBD_SELECT_PROTOCOL:{
      if(millis() - OBD_last_setup >= OBD_SETUP_DELAY){
        OBD_talk("atsp0"); // Auto select protocol
        OBD_setup_state = OBD_HEADERS_OFF;
        OBD_last_setup = millis();
      }
      break;
    }
    case OBD_HEADERS_OFF:{
      if(millis() - OBD_last_setup >= OBD_SETUP_DELAY){
        OBD_talk("ath0");
        OBD_setup_state = OBD_ELM_READY;
        OBD_last_setup = millis();
      }
      break;
    }
    case OBD_ELM_READY:{
      break;
    }
    default: break;
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
        OBD_Serial.print(cmd + '\r');
        timeout_read = millis();
        OBD_state = OBD_READ;
        break;
      case OBD_READ:
        do{
          if(OBD_Serial.available() > 0){
            inChar = char(OBD_Serial.read()); // Convert the byte into a char
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
          OBD_state = OBD_SEND;
        }
        break;
      default: break;
    }
  }while(!data_received && ((millis() - timeout_talk) < OBD_TALK_TIMEOUT));
}

/*******************************************************************
 * NOTE:                                                           *
 *  We're operating under the assumption that there're only 5 ECUs *
 *  reporting trouble codes, that the first ECU's data (DTCs)      *
 *  is reported before the second's,and that each ECU would report *
 *  a maximum of 20 DTCs --> Max N°DTCs == 100.                    *
 *******************************************************************/

long* get_dtc_number(){
  static long n_dtc[5]= {0, 0, 0, 0, 0};
  uint8_t  dtc_num_state = READ_DTC__;
  uint16_t s_idx = 0;
  num_ecu = 0;
  do{
    switch(dtc_num_state){
      case READ_DTC__:{
        if(resp[s_idx++] == '4')dtc_num_state = FOUND_4_;
        break;
      }
      case FOUND_4_:{
        if(resp[s_idx++] == '1')dtc_num_state = FOUND_1;
        else dtc_num_state = READ_DTC__;
        break;
      }
      case FOUND_1:{
        if(resp[s_idx++] == ' ')dtc_num_state = FOUND_SPACE_;
        else dtc_num_state = READ_DTC__;
        break;
      }
      case FOUND_SPACE_:{
        if(resp[s_idx++] == '0')dtc_num_state = FOUND_0_;
        else dtc_num_state = READ_DTC__;
        break;
      }
      case FOUND_0_:{
        if(resp[s_idx++] == '1')dtc_num_state = FOUND_1_;
        else dtc_num_state = READ_DTC__;
        break;
      }
      case FOUND_1_:{
        String s  = resp.substring(s_idx+1, s_idx+3);
        if((s[0] - '0') >= 8) n_dtc[num_ecu++] = strtol(s.c_str(), NULL, 16) - 128;
        else n_dtc[num_ecu++] = strtol(s.c_str(), NULL, 16);
        dtc_num_state = READ_DTC__;
        break;
      }
    }
  }while(num_ecu < 5 && s_idx < resp.length());
  return n_dtc;
}

// Consult the ELM327 datasheet for further understanding of the next procedure
// https://www.elmelectronics.com/wp-content/uploads/2016/07/ELM327DS.pdf (page 34)
void get_dtc_s(String& data, long n_dtc, uint8_t ecu_idx){
  uint8_t ecus = 0;
  for(uint16_t i = 0; i < resp.length()-1; i++){ // Search for responses
    if(resp[i] == '4' && resp[i+1] == '3' && ecus++ == ecu_idx){ // found desired response
      for(long j = 0; j < n_dtc ; j++){ // Read all DTCs available for that specific ECU
        // resp :: "43 01 33 00 00 00 00"
        String sub = resp.substring(i+3+j*6, i+8+j*6);
        // sub  :: "01 33"
        if(isDigit(sub[0]))data += conversion_[sub[0]-'0'];
        else data += conversion_[10+(sub[0]-'A')]; // If sub[0] == 'F' --> (sub[0]-'F')+10 == 15
        data += sub[1];
        data += sub[3];
        data += sub[4];
        data += ',';
        // data :: "0P133," 
      }
    }
  }
}

String read_dtc_(){
  OBD_talk("0101"); // Request number of DTCs (Diagnostic Trouble Codes)
  long *num_dtcs = get_dtc_number();
  delay(50); // OBD is slow and will need some time between commands
  OBD_talk("03");   // Request actual DTCs
  String dtc_string = ""; // String that'll be filled with DTCs in the format
                          // "P0133,C2122, ..."
  for(uint8_t i = 0; i < num_ecu; i++){
    if(*(num_dtcs + i)){ // There exists TCs
      get_dtc_s(dtc_string, *(num_dtcs + i), i);
    }
  }
  return dtc_string;
}
