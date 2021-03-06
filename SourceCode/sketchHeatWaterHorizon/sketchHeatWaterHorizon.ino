//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//WAFLOCON FIRMWARE
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//----------------------------------------------------------------------------------------------------------------
//https://learn.adafruit.com/adafruit-1-wire-gpio-breakout-ds2413/onewire-library
//https://github.com/adafruit/Adafruit_DS2413
//----------------------------------------------------------------------------------------------------------------

#include <OneWire.h>
#include <DallasTemperature.h>
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//----------------------------------------------------------------------------------------------------------------
//UserSettings
//#define TEMPERATURE_MIN 25           //[Grad Celsius] MinTemperature at CC for Access to Barrel
//!!!! TEMPERATURE_DIFF_TRIGGER > TEMPERATURE_HYSTERESIS !!!!!
#define TEMPERATURE_DIFF_TRIGGER 4   //[Grad Celsius] TriggerdifferenceTemperature
#define TEMPERATURE_HYSTERESIS 2     //[K] Temperaturedifference beetween Pump ON/OFF
#define TEMPERATURE_MAX_VALID 70     //[Grad Celsius]

//Stuff
#define NilTemperature -127

//B = BHSF, A = BHSE
#define LEVEL_LOW 0x0A  //BIN: 1010 // B + A == GND
#define LEVEL_MID 0x0E  //BIN: 1110 // B (BHSE) open == HIGH(PullUp), A (BHSF) shorten to GND == LOW
#define LEVEL_FULL 0x0F //BIN: 1111 // B+A open == HIGH
#define LEVEL_UNKNOWN 0x00 //BIN: 0000 // Default value
#define LEVEL_LOW_MAX_CYCLECOUNT 600
#define LEVEL_LOW_RESET_CYCLECOUNT 1000

#define PUMP_MODE_NONE 0
#define PUMP_MODE_HOTNCOLD 1
#define PUMP_MODE_HOT 2
#define PUMP_MODE_COLD 3
#define PUMP_MODE_ALL 4



//Hardware
#define PinOutputLED 13 // Optics
#define PinOutputPumpHot 7 // To MOSFET-Gate of Pump 3
#define PinOutputPumpCold 6 // To MOSFET-Gate of Pump 4
#define PinExternalTrigger 2 // InterruptPin (Int0)PD2

//DeviceMapping
#define T0 0
#define CC 1
//#define CH 2
#define T2 2
#define BC 3
#define T4 4
#define T5 5
#define T6 6
#define BH 7
#define CH 8
#define T9 9
#define E0 10

//ROMAddresses
#define T0ROM 0x28,0xFF,0x84,0x63,0x90,0x15,0x01,0xE1
#define T1ROM 0x28,0xFF,0x1E,0x29,0x90,0x15,0x04,0x2F
#define T2ROM 0x28,0xFF,0x4c,0x77,0x90,0x15,0x01,0xCB
#define T3ROM 0x28,0xFF,0x85,0x19,0x90,0x15,0x04,0xA0
#define T4ROM 0x28,0xFF,0xA8,0x5E,0x90,0x15,0x01,0xED
#define T5ROM 0x28,0xFF,0xAE,0x1A,0x90,0x15,0x04,0xA0
#define T6ROM 0x28,0xFF,0x45,0x18,0x90,0x15,0x04,0x0D
#define T7ROM 0x28,0xFF,0xD4,0x84,0x90,0x15,0x01,0xBB
#define T8ROM 0x28,0xFF,0x83,0x65,0x90,0x15,0x01,0xB9
#define T9ROM 0x28,0xFF,0x50,0x63,0x90,0x15,0x01,0xA0
#define E0ROM 0x3A,0x4D,0xFF,0x2B,0x00,0x00,0x00,0x8D

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#define ONE_WIRE_BUS 3 // Data wire is plugged into digital pin 3 on the Arduino
#define NumberOfDevices 11 // Set maximum number of devices in order to dimension
#define NameMaxCharLength 7 // extra char for string terminator

#define DS18B20_FAMILY_ID    0x28
#define DS2413_FAMILY_ID    0x3A
#define DS2413_ACCESS_READ  0xF5
#define DS2413_ACCESS_WRITE 0x5A
#define DS2413_ACK_SUCCESS  0xAA
#define DS2413_ACK_ERROR    0xFF
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
OneWire oneWire(ONE_WIRE_BUS); // Setup a oneWire instance to communicate with any OneWire devices
DallasTemperature sensors(&oneWire); // Pass our oneWire reference to Dallas Temperature.
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
byte allAddress [NumberOfDevices][8]; // Device Addresses are 8-element byte arrays.
// we need one for each of our DS18B20 sensors.
byte totalDevices; // Declare variable to store number of One Wire devices
// that are actually discovered.
//char allNames[NumberOfDevices][NameMaxCharLength]={"T0","BC","BH","CC","CH","T5","T6","T7","T8","T9"};
//char allNames[NumberOfDevices][NameMaxCharLength] = {"T0", "CC", "CH", "BC", "T4", "T5", "T6", "BH", "T8", "T9", "E0"};
//byte ROMMapping[NumberOfDevices] = {T0, CC, CH, BC, T4, T5, T6, BH, T8, T9, E0};
char allNames[NumberOfDevices][NameMaxCharLength] = {"T0", "CC", "T2", "BC", "T4", "T5", "T6", "BH", "CH", "T9", "E0"};
byte ROMMapping[NumberOfDevices] = {T0, CC, T2, BC, T4, T5, T6, BH, CH, T9, E0};
int USBPlugged = 0; // me: 0 = external Power, 1 = USB
int loopCount = 0;
int BarrelLowLoopCount = 0;
struct DeviceRecord {
  uint8_t address[8];
  bool active = LOW;
  //float temperature[2] = {NilTemperature, NilTemperature};
  float temperature = NilTemperature;
  byte IOData = LEVEL_UNKNOWN;
};
typedef struct DeviceRecord DeviceData;
DeviceData Devices[NumberOfDevices];

uint8_t OutputStates = PUMP_MODE_NONE;
bool ExternalTrigger = LOW;
bool InterruptTaskFinshed = LOW;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void printLine(char sign[1]) {
  for (byte i = 0; i < 70; i++) {
    Serial.print(sign);
  }
}
void printSpace(int LFCount) {
  for (int i = 0; i < LFCount; i++) {
    Serial.print("\r\n");
  }
}
//----------------------------------------------------------------------------------------------------------------
void mapDevices() {
  byte allROMs[NumberOfDevices][8] = {T0ROM, T1ROM, T2ROM, T3ROM, T4ROM, T5ROM, T6ROM, T7ROM, T8ROM, T9ROM, E0ROM};
  Serial.println("mapDevices()");
  for (byte d = 0; d < NumberOfDevices; d++) {
    //each device, starting with dd = "BC"
    byte r = ROMMapping[d];
    Serial.print("\"");
    Serial.print(allNames[d]);
    Serial.print("\" ");
    Serial.print("(Hardware");
    Serial.print(r);
    Serial.print(") Address: ");
    for (byte ab = 0; ab < 8; ab++) {
      Devices[d].address[ab] = allROMs[r][ab];
      Serial.print("0x");
      Serial.print(Devices[d]. address[ab], HEX);
      Serial.print(" ");
    }
    Serial.println("");
  }
  Serial.println("");
}
//----------------------------------------------------------------------------------------------------------------
bool byteArrayMatch(byte byte1[8], byte byte2[8]) {
  byte goil = 0;
  for (byte i = 0; i < 8; i++) {
    if (byte1[i] == byte2[i]) {
      goil++;
    }
  }
  if (goil == 8) {
    return HIGH;
  }
  else {
    return LOW;
  }
}
//----------------------------------------------------------------------------------------------------------------
void setDeviceStatus() {
  // Serial.print("setDeviceStatus()");
  // Serial.println("");
  for (byte d = 0; d < NumberOfDevices; d++) {
    byte tempByteArray[8];
    for (byte ab = 0; ab < 8; ab++) {
      tempByteArray[ab] = Devices[d].address[ab];
    }
    //Serial.print(allNames[d]);
    int hit = -1;
    byte a = 0;
    while (a < NumberOfDevices) {
      if (byteArrayMatch(allAddress[a], tempByteArray)) {
        hit = a;
        Devices[d].active = HIGH;
        //Serial.print(" = ");
        //Serial.print("Device ");
        //Serial.print(a);
        //Serial.print(" active");
        //Serial.println("");
        break;
      }
      else {
        Devices[d].active = LOW;
      }
      a++;
    }
    if (hit == -1) {
      // Serial.print(" not found");
      // Serial.println("");
    }
  }
  // Serial.println("");
}
//----------------------------------------------------------------------------------------------------------------
uint8_t readDS2413(byte DS2413Address[8]) {
  bool ok = LOW;
  uint8_t results;

  oneWire.reset();
  oneWire.select(DS2413Address);
  oneWire.write(DS2413_ACCESS_READ);

  results = oneWire.read();                 /* Get the register results   */
  //Serial.println(results, BIN);
  //Serial.println(results, HEX);
  //ok = (!results & 0x0F) == (results >> 4); /* Compare nibbles            */
  results &= 0x0F;                          /* Clear inverted values      */
  //Serial.println(results, BIN);
  //Serial.println(results, HEX);
  oneWire.reset();

  //return ok ? results : -1;
  return results;
}
//----------------------------------------------------------------------------------------------------------------
void getDataFromDevices() {
  //Serial.println("getDataFromDevices()");
  sensors.requestTemperatures(); // Initiate temperature request to all devices
  for (byte d = 0; d < NumberOfDevices; d++) {
    if (Devices[d].address[0] ==  DS18B20_FAMILY_ID) {
      //Write to DevicesArray
      //Devices[d].temperature[1] = Devices[d].temperature[0]; //?
      if (Devices[d].active) {
        //Devices[d].temperature[0] = sensors.getTempC(Devices[d].address);
        Devices[d].temperature = sensors.getTempC(Devices[d].address);
        Serial.print(allNames[d]);
        Serial.print(" -> ");
        //Serial.println(Devices[d].temperature[0]);
        Serial.println(Devices[d].temperature);
        if (Devices[d].temperature > TEMPERATURE_MAX_VALID) {
          Devices[d].temperature = NilTemperature;
          Serial.print(F("Temperaturvalue > TEMPERATURE_MAX_VALID"));
        }
        //Serial.print("C\r\n");
      }
      else {
        //Devices[d].temperature[0] = NilTemperature;
        Devices[d].temperature = NilTemperature;
      }
    }
    if (Devices[d].address[0] == DS2413_FAMILY_ID) {
      uint8_t state = readDS2413(Devices[d].address); //read DS2413
      if (Devices[d].active) {
        Devices[d].IOData = state;                      //safe
        Serial.print(allNames[d]);
        Serial.print(" -> ");
        //Serial.print(state, BIN);
        //Serial.print(" (");
        switch (state) {
          case -1:
            Serial.print(F("Failed reading the DS2413"));
            break;
          case LEVEL_LOW:
            Serial.print(F("BarrelLevel LOW"));
            break;
          case LEVEL_MID:
            Serial.print(F("BarrelLevel MID"));
            break;
          case LEVEL_FULL:
            Serial.print(F("BarrelLevel FULL"));
            break;
          default:
            Serial.print(F("Unknown State for SensorPhalanx"));
            break;
            //Serial.print(")");
        }
      }
      else {
        Devices[d].IOData = LEVEL_UNKNOWN;
        Serial.print(F("Barrel offline!"));
      }
    }
  }
  Serial.println("");
}
//----------------------------------------------------------------------------------------------------------------
void printAddress(DeviceAddress addr) {
  byte i;
  for ( i = 0; i < 8; i++) { // prefix the printout with 0x
    Serial.print("0x");
    if (addr[i] < 16) {
      Serial.print('0'); // add a leading '0' if required.
    }
    Serial.print(addr[i], HEX); // print the actual value in HEX
    Serial.print(" ");
  }
  Serial.println("");
}
//----------------------------------------------------------------------------------------------------------------
byte discoverOneWireDevices() {
  byte j = 0; // search for one wire devices and
  // copy to device address arrays.
  // Serial.print("discoverOneWireDevices()");
  // Serial.print("\r\n");
  for (byte i = 0; i < NumberOfDevices; i++) {
    //clearAllAddresses
    for (byte ab = 0; ab < 8; ab++) {
      allAddress[i][ab] = 0xFF;
    }
  }
  while ((j < NumberOfDevices) && (oneWire.search(allAddress[j]))) {
    j++;
  }
  for (byte i = 0; i < j; i++) {
    // Serial.print("Device ");
    // Serial.print(i);
    // Serial.print(": ");
    // printAddress(allAddress[i]); // print address from each device address array.
  }
  // Serial.print("\r\n");
  return j ; // return total number of devices found.
}
//----------------------------------------------------------------------------------------------------------------
byte arrayPos(byte definition) {
  byte i = 0;
  byte thePosition = NumberOfDevices + 1;
  while (i < NumberOfDevices) {
    //find position of definition in sensor array
    if (definition == ROMMapping[i]) {
      thePosition = i;
      break;
    }
    i++;
  }
  if (thePosition == NumberOfDevices + 1) {
    Serial.println ("arrayPos: Can't find definition in ROMMappingArray");
  }
  return thePosition;
}
//----------------------------------------------------------------------------------------------------------------
bool checkValidTemp(float TempCold, float TempHot) {
  //  Serial.println("checkValidTemp()");
  if (TempCold != NilTemperature && TempCold < TEMPERATURE_MAX_VALID && TempHot != NilTemperature && TempHot < TEMPERATURE_MAX_VALID) {
    return HIGH;
  }
  else {
    if (TempCold == NilTemperature) {
      Serial.println("TempCold invalid");
    }
    if (TempCold >= TEMPERATURE_MAX_VALID) {
      Serial.println("TempCold > TEMPERATURE_MAX_VALID");
    }
    if (TempHot == NilTemperature) {
      Serial.println("TempHot invalid");
    }
    if (TempHot >= TEMPERATURE_MAX_VALID) {
      Serial.println("TempHot > TEMPERATURE_MAX_VALID");
    }
    return LOW;
  }
}
bool getCollectorPumpRequest(float TempCold, float TempHot, uint8_t OutputStates) {
  //  Serial.println("getCollectorPumpRequest()");
  //  Serial.print("TempHot = ");
  //  Serial.println(TempHot);
  //  Serial.print("TempCold = ");
  //  Serial.println(TempCold);

  bool result = LOW;
  if (OutputStates == PUMP_MODE_NONE) {
    //Pump(s) ON or OFF regarding temperature difference
    //Examples: // 25 - 20 > 4 = HIGH
    result = (TempHot - TempCold > TEMPERATURE_DIFF_TRIGGER);
    Serial.print("TempHot - TempCold > TEMPERATURE_DIFF_TRIGGER -> ");
    Serial.print(TempHot - TempCold);
    Serial.print(" > ");
    Serial.println(TEMPERATURE_DIFF_TRIGGER);
  }
  else {
    //Pump(s) ON or OFF regarding temperature difference including Hysteresis
    //Examples: 25 + 2 - 20 > 4 = HIGH, 23 + 2 - 21 > 4 = LOW
    result = (TempHot + TEMPERATURE_HYSTERESIS - TempCold > TEMPERATURE_DIFF_TRIGGER);
    Serial.print("TempHot + TEMPERATURE_HYSTERESIS - TempCold > TEMPERATURE_DIFF_TRIGGER -> ");
    Serial.print(TempHot + TEMPERATURE_HYSTERESIS - TempCold);
    Serial.print(" > ");
    Serial.println(TEMPERATURE_DIFF_TRIGGER);
  }
  return result;
}
uint8_t getNominalOutputValues(bool CollectorPumpRequest, uint8_t BarrelState) {
  //  Serial.println("getNominalOutputValues()");
  uint8_t result = PUMP_MODE_NONE;
  if (BarrelState == LEVEL_LOW) {
    result = PUMP_MODE_COLD;
  }
  else if (BarrelState == LEVEL_MID && CollectorPumpRequest) {
    result = PUMP_MODE_HOTNCOLD;
  }
  else if (BarrelState == LEVEL_FULL && CollectorPumpRequest) {
    result = PUMP_MODE_HOT;
  }
  return result;
}
uint8_t setOutputs(uint8_t NominalOutputValues) {
  //  Serial.println("setOutputs()");
  bool valPumpHot;
  bool valPumpCold;
  switch (NominalOutputValues) {
    case PUMP_MODE_ALL:
      //Switch all
      valPumpHot = HIGH;
      valPumpCold = HIGH;
      break;
    case PUMP_MODE_HOTNCOLD:
      //Switch all
      valPumpHot = HIGH;
      valPumpCold = HIGH;
      break;
    case PUMP_MODE_HOT:
      //Switch Hot, switch off Cold
      valPumpHot = HIGH;
      valPumpCold = LOW;
      break;
    case PUMP_MODE_COLD:
      //Switch Cold, switch off Hot
      valPumpHot = LOW;
      valPumpCold = HIGH;
      break;
    case PUMP_MODE_NONE:
      //Switch off all
      valPumpHot = LOW;
      valPumpCold = LOW;
      break;
    default:
      //Switch off all
      valPumpHot = LOW;
      valPumpCold = LOW;
      break;
  }
  digitalWrite(PinOutputPumpHot, valPumpHot);
  digitalWrite(PinOutputPumpCold, valPumpCold);
  Serial.print("PumpHot -> ");
  Serial.println(valPumpHot);
  Serial.print("PumpCold -> ");
  Serial.println(valPumpCold);
  return NominalOutputValues;
}
//Interrupt
void enableInterruptForExternalTrigger(uint8_t pin, bool enable) {
  //Serial.println("enableInterruptForExternalTrigger()");
  if (enable) {
    //enable interrupt for going to LOW at inputpin
    attachInterrupt(digitalPinToInterrupt(pin), externalTriggerResponse, FALLING);
  }
  else {
    //disable
    //TODO: doesn't work, make it work!
    detachInterrupt(pin);
  }
  Serial.println("");
  if (enable) {
    Serial.print("Enable");
  }
  else {
    Serial.print("Disable");
  }
  Serial.println(" interrupt for ExternalTrigger");
  Serial.println("");
}
//ISR
void externalTriggerResponse() {
  //disable interrupt
  enableInterruptForExternalTrigger(PinExternalTrigger, LOW);
  //set flag for external trigger
  ExternalTrigger = HIGH;
  //  Serial.println("externalTriggerResponse()");
  Serial.println("Set ExternalTriggerFlag");
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void setup() {
  pinMode(PinOutputLED, OUTPUT);
  pinMode(PinOutputPumpHot, OUTPUT);
  digitalWrite(PinOutputPumpHot, LOW);
  Serial.print("PinOutputPumpHot -> ");
  Serial.println(PinOutputPumpHot);
  pinMode(PinOutputPumpCold, OUTPUT);
  digitalWrite(PinOutputPumpCold, LOW);
  Serial.print("PinOutputPumpCold -> ");
  Serial.println(PinOutputPumpCold);

  //Use interrupt for External Trigger
  pinMode(PinExternalTrigger, INPUT_PULLUP);
  enableInterruptForExternalTrigger(PinExternalTrigger, HIGH);

  //Serial.begin(38400);
  Serial.begin(9600);
  sensors.begin();
  delay(1000);
  printLine("-");
  Serial.println("");
  mapDevices();
}
//----------------------------------------------------------------------------------------------------------------
void loop() {
  printLine(".");
  //printSpace(20);
  Serial.println(loopCount);
  totalDevices = discoverOneWireDevices(); // get addresses of our one wire devices into allAddress array
  setDeviceStatus();
  for (byte i = 0; i < totalDevices; i++) {
    if (allAddress[i][0] ==  DS18B20_FAMILY_ID) {
      sensors.setResolution(allAddress[i], 10); // and set the a to d conversion resolution of each.
    }
  }
  //DATA---------------------------------------------------------------------
  getDataFromDevices();

  //ACTION-------------------------------------------------------------------
  bool PumpRequest = LOW;
  uint8_t NominalOutputValues = 0; // disable all
  float BHTemperature = Devices[arrayPos(BH)].temperature;
  float CHTemperature = Devices[arrayPos(CH)].temperature;
  byte BarrelState = Devices[arrayPos(E0)].IOData;
  if (BarrelState == LEVEL_LOW) {
    BarrelLowLoopCount++;
  }
  else {
    BarrelLowLoopCount = 0;
  }
  if (checkValidTemp(BHTemperature, CHTemperature)) {
    if (ExternalTrigger) {
      //clear flag
      ExternalTrigger = LOW;
      Serial.println("Clear ExternalTriggerFlag");
      //for external trigger: set testcase with nice values to activate pump(s) for sure
      BHTemperature = 20;
      CHTemperature = 50;
      InterruptTaskFinshed = HIGH;
    }
    PumpRequest = getCollectorPumpRequest(BHTemperature, CHTemperature, OutputStates);
    NominalOutputValues = getNominalOutputValues(PumpRequest, BarrelState);
  }

  if (BarrelState == LEVEL_LOW && BarrelLowLoopCount >= LEVEL_LOW_MAX_CYCLECOUNT) {
    // switch off pump after x cycles
    Serial.print("BarrelState == LOW for more than ");
    Serial.print(BarrelLowLoopCount);
    Serial.print(" cycles. Reset in (");
    Serial.print(LEVEL_LOW_RESET_CYCLECOUNT - BarrelLowLoopCount);
    Serial.println(").");
    OutputStates = setOutputs(PUMP_MODE_NONE);
    if (BarrelLowLoopCount >= LEVEL_LOW_RESET_CYCLECOUNT) {
      // reset pump after x cycles
      BarrelLowLoopCount = 0;
    }
  }
  else {
    OutputStates = setOutputs(NominalOutputValues);
  }

  delay(1000);
  if (InterruptTaskFinshed) {
    //reactivate interrupt
    enableInterruptForExternalTrigger(PinExternalTrigger, HIGH);
    InterruptTaskFinshed = LOW;
  }
  loopCount++;
}
