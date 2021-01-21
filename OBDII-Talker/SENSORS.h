#define METRIC              0
#define IMPERIAL            1
#define NUMBER_OF_SENSORS   2

int system_of_measurements = METRIC;

////Functions' prototypes
bool validate_response(String, String);
String get_sensors_data(int);

//Sensors' formulas
String rpm_formula(String, String);
String speed_formula(String, String);

////Sensor type
typedef struct{
  String (*formula)(String data, String expected);
  String command;
  String expected;
  bool availability;
  String json_tag;
}SENSOR;

// Sensors' data
// For more info please refer to this wiki page: https://en.wikipedia.org/wiki/OBD-II_PIDs

    //////////////////////////////////////////////////////////////////
    ///    In order to add more sensors, all you gotta do is add   ///
    ///    the appropriate sensor's data you find in the wiki and  ///
    ///    you create its function following the two examples I    ///
    ///               provided for the rpm and the speed           ///
    //////////////////////////////////////////////////////////////////
    
SENSOR sensors[] = 
  {//formula         //command   //expected response  //availability  //json_tag
    {rpm_formula,      "01 0C",    "41 0C",             true,           "rpm"},
    {speed_formula,    "01 0D",    "41 0D",             true,           "speed"},
  };


////Functions' declarations
String get_sensors_data(int i){
  OBD_talk(sensors[i].command); // Send the command and receive a response
  if(validate_response(resp, sensors[i].expected)){ // If response is valid
    String return_val = sensors[i].formula(resp, sensors[i].expected);
    return return_val;
  }
  else return "?"; // We fill invalid or non existing data with a "?" char
}

bool validate_response(String data, String expected){
  return (data.indexOf(expected) >= 0) ? true : false;
}

String rpm_formula(String data, String expected){
  // data == "SEARCHING... 41 01 32 08 00 00" --> As == "32", Bs == "08"
  String As = data.substring(data.indexOf(expected) + 6, data.indexOf(expected) + 8);
  String Bs = data.substring(data.indexOf(expected) + 9, data.indexOf(expected) + 11);
  long A = strtol(As.c_str(), NULL, 16);
  long B = strtol(Bs.c_str(), NULL, 16);
  return String((256*A + B)/4); // https://en.wikipedia.org/wiki/OBD-II_PIDs
}

String speed_formula(String data, String expected){
  // data == "SEARCHING... 41 01 32 08 00 00" --> As == "32"
  String As = data.substring(data.indexOf(expected) + 6, data.indexOf(expected) + 8);
  long A = strtol(As.c_str(), NULL, 16);
  if (system_of_measurements == METRIC)
    return String(A);
  else   // if the system is IMPERIAL
    return String(int(A/1.609));
}
