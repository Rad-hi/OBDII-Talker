#include <ArduinoJson.h> // https://github.com/bblanchon/ArduinoJson

#include "OBD.h"
#include "Sensors.h"

// Serial monitor baud rate, maybe you want to set it a little slower
// if you're using an arduino (i'm using an esp32)
#define SERIAL_MON_BAUD_RATE   115200  // 115.2K baud serial connection to computer
#define SENSOR_SEND_DELAY      50      // ms
#define JSON_TAG_SENSORS       "Sensors"
#define JSON_BUFFER_SIZE       200     // Calculate the right number using https://arduinojson.org/v5/assistant/

StaticJsonDocument<JSON_BUFFER_SIZE> DATA; //Json file that'll contain all data, and then be sent via mqtt
char buffer[JSON_BUFFER_SIZE];

unsigned long OBD_last_sent = 0; //last time an OBD setup cmd was sent
int  OBD_setup_step = 0; 

unsigned long sensor_last_sent = 0;
bool sensor_ready = false;
int  sensor = 0;

void setup() {
  Serial.begin(SERIAL_MON_BAUD_RATE);
  
  do{
    // Setup of the ELM327 module, refer to OBD.h
    OBD_setup();
    
  }while(OBD_setup_state != OBD_ELM_READY);
}

void loop() {
  //if we still have sensors' data to read and the delay period have passed
  //we read the next sensor data and we populate the Json file's sensors section
  if(!sensor_ready && sensor < NUMBER_OF_SENSORS 
    && (millis() - sensor_last_sent >= SENSOR_SEND_DELAY)){
      // Fill car sensors' data in the Json buffer
      DATA[JSON_TAG_SENSORS][sensors[sensor].json_tag] = get_sensors_data(sensor);
      sensor_last_sent = millis();
      sensor++;
  }
  if(!sensor_ready && sensor == NUMBER_OF_SENSORS){
    sensor = 0;
    sensor_ready = true;
  }

  // This specific sketch was part of a bigger project hence the use of
  // Json files (the data was sent using MQTT as a json file)
  if(sensor_ready){
    serializeJson(DATA, buffer);
    Serial.println(buffer);
  }
}
