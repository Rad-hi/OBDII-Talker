#include <ArduinoJson.h> // https://github.com/bblanchon/ArduinoJson

#include "OBD.h"
#include "SENSORS.h"

// Serial monitor baud rate, maybe you want to set it a little slower
// if you're using an arduino (i'm using an esp32)
#define SERIAL_MON_BAUD_RATE   115200  // 115.2K baud serial connection to computer

#define JSON_BUFFER_SIZE       200     // Calculate the right number using https://arduinojson.org/v5/assistant/

////General values
#define OBD_SETUP_STEPS        6       // How many steps the setup takes
#define OBD_SEND_DELAY         50      // How much time (ms) to wait between each 2 sendings 
#define SENSOR_SEND_DELAY      50      // ms

StaticJsonDocument<JSON_BUFFER_SIZE> DATA; //Json file that'll contain all data, and then be sent via mqtt
char buffer[JSON_BUFFER_SIZE];

unsigned long OBD_last_sent = 0; //last time an OBD setup cmd was sent
int  OBD_setup_step = 0; 

unsigned long sensor_last_sent = 0;
bool sensor_ready = false;
int  sensor = 0;

void setup() {
  Serial.begin(SERIAL_MON_BAUD_RATE);

  // Setup of the OBDII in steps without the use of delays
  do{
    if(OBD_setup_step < OBD_SETUP_STEPS
      && (millis() - OBD_last_sent >= OBD_SEND_DELAY)){
        Serial.printf("Setup step n°: %d", OBD_setup_step);
        OBD_init(OBD_setup_step++); //execute the step and increment the setup step, check the OBD.h file for OBD_init()
        OBD_last_sent = millis();
    }

    //////////////////////////////////////////////////////////////////
    ///    The use of millis allows you to put more setups here    ///
    ///    (Multitasking since we're not blocking the core with    ///
    ///               delays all over the place)                   ///
    //////////////////////////////////////////////////////////////////
    
  }while(OBD_setup_step < OBD_SETUP_STEPS);
}

void loop() {
  //if we still have sensors' data to read and the delay period have passed
  //we read the next sensor data and we populate the Json file's sensors section
  if(!sensor_ready && sensor < NUMBER_OF_SENSORS 
    && (millis() - sensor_last_sent >= SENSOR_SEND_DELAY)){
      String sensor_data = get_sensors_data(sensor); // Refer to the SENSORS.h file
      DATA["sensors"][sensors[sensor].json_tag] = sensor_data;
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
