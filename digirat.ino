#include <bluefruit.h>
#include <Arduino.h>

uint32_t vbat_pin = PIN_VBAT;             // A7 for feather nRF52832, A6 for nRF52840

#define DEBUG_MODE  true                 // Enables or disable serial comunication
#define SAMPLE_PERIOD 10000               // Spacing between sample extraction in miliseconds (10 seconds)
#define TX_POWER (4)                      // Transmission power in dBm (check bluefruit.h)

// BATTERY CONSTANTS

#define VBAT_MV_PER_LSB   (0.73242188F)   // 3.0V ADC range and 12-bit ADC resolution = 3000mV/4096
#define VBAT_DIVIDER      (0.5F)          // 150K + 150K voltage divider on VBAT
#define VBAT_DIVIDER_COMP (2.0F)          // Compensation factor for the VBAT divider 2

#define REAL_VBAT_MV_PER_LSB (VBAT_DIVIDER_COMP * VBAT_MV_PER_LSB)

// BLE
#define DIGIPOT_RAT_SERVICE_UUID (0x100289b5b20c4ad9808baeb72b05404b) // 100289b5-b20c-4ad9-808b-aeb72b05404b
#define DIGIPOT_RAT_GAIN_UUID (0xd4afcbcd4b314ed7ba9828035d6ab6d8) // d4afcbcd-4b31-4ed7-ba98-28035d6ab6d8
#define DIGIPOT_RAT_TONE_UUID (0xbfb430b86d824918a1d76f17a2f39c92) // bfb430b8-6d82-4918-a1d7-6f17a2f39c92
#define DIGIPOT_RAT_VOLUME_UUID (0x7869beaeb0d34398a96d4a037aad947b) // 7869beae-b0d3-4398-a96d-4a037aad947b
#define DIGIPOT_RAT_POT3_UUID (0x66b0581111e74d498dffd22d788cf007) // 66b05811-11e7-4d49-8dff-d22d788cf007


BLEDis  bledis;  // device information
BLEBas  blebas;  // battery
BLEService ratBleService = BLEService(DIGIPOT_RAT_SERVICE_UUID);
BLECharacteristic gainCharacteristic = BLECharacteristic(DIGIPOT_RAT_GAIN_UUID);
BLECharacteristic toneCharacteristic = BLECharacteristic(DIGIPOT_RAT_TONE_UUID);
BLECharacteristic volumeCharacteristic = BLECharacteristic(DIGIPOT_RAT_VOLUME_UUID);
BLECharacteristic pot3Characteristic = BLECharacteristic(DIGIPOT_RAT_POT3_UUID);

void setup() {

  if (DEBUG_MODE) {
    Serial.begin(115200);
    while ( !Serial ) delay(10);   // for nrf52840 with native usb

    Serial.println("Digipot Rat");
    Serial.println("---------------------------\n");
  }

  Bluefruit.autoConnLed(true);
  Bluefruit.configPrphBandwidth(BANDWIDTH_MAX);
  Bluefruit.begin();
  Bluefruit.setTxPower(TX_POWER);    // Check bluefruit.h for supported values
  Bluefruit.setName("Digital RAT");
  Bluefruit.Periph.setConnectCallback(deviceConnectedCallback);
  Bluefruit.Periph.setDisconnectCallback(deviceDisconnectedCallback);

  // Configure and Start Device Information Service
  bledis.setManufacturer("Kikermo Industries");
  bledis.setModel("Bluefruit Feather52");
  bledis.begin();

  // Start BLE Battery Service
  blebas.begin();
  blebas.write(100);

  // Start Digipot Rat Service
  ratBleService.begin();

  // Gain Characteristic
  gainCharacteristic.setProperties(CHR_PROPS_READ|CHR_PROPS_WRITE);
  gainCharacteristic.setPermission(SECMODE_OPEN, SECMODE_OPEN);
  gainCharacteristic.setFixedLen(1);
  gainCharacteristic.setWriteCallback(gainCallback);
  gainCharacteristic.setUserDescriptor("GAIN");
  gainCharacteristic.begin();

  
  // Tone Characteristic
  toneCharacteristic.setProperties(CHR_PROPS_READ|CHR_PROPS_WRITE);
  toneCharacteristic.setPermission(SECMODE_NO_ACCESS, SECMODE_OPEN);
  toneCharacteristic.setFixedLen(1);
  toneCharacteristic.setWriteCallback(toneCallback);
  toneCharacteristic.setUserDescriptor("TONE");
  toneCharacteristic.begin();

  
  // Volume Characteristic
  volumeCharacteristic.setProperties(CHR_PROPS_READ|CHR_PROPS_WRITE);
  volumeCharacteristic.setPermission(SECMODE_NO_ACCESS, SECMODE_OPEN);
  volumeCharacteristic.setFixedLen(1);
  volumeCharacteristic.setWriteCallback(volumeCallback);
  volCharacteristic.setUserDescriptor("VOLUME");
  volumeCharacteristic.begin();

  
  // Pot 3 Characteristic
  pot3Characteristic.setProperties(CHR_PROPS_READ|CHR_PROPS_WRITE);
  pot3Characteristic.setPermission(SECMODE_NO_ACCESS, SECMODE_OPEN);
  pot3Characteristic.setFixedLen(1);
  pot3Characteristic.setWriteCallback(pot3Callback);
  pot3Characteristic.setUserDescriptor("POT 3");
  pot3Characteristic.begin();

  // Set up and start advertising
  startAdv();


  // TODO read digipots initial value
  // gainCharacteristic.write()
}

void startAdv(void) {
  // Advertising packet
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();

  Bluefruit.Advertising.addService(ratBleService);

  // Secondary Scan Response packet (optional)
  // Since there is no room for 'Name' in Advertising packet
  Bluefruit.ScanResponse.addName();

  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.setInterval(32, 244);    // in unit of 0.625 ms
  Bluefruit.Advertising.setFastTimeout(30);      // number of seconds in fast mode
  Bluefruit.Advertising.start(0);                // 0 = Don't stop advertising after n seconds
}

void loop() {
 
}


// callback invoked when the connection is established
void deviceConnectedCallback(uint16_t connectionHandle) {
  // Get the reference to current connection
  BLEConnection* connection = Bluefruit.Connection(connectionHandle);

  char deviceName[32] = { 0 };
  connection->getPeerName(deviceName, sizeof(deviceName));

  if (!DEBUG_MODE) {
    return;
  }

  Serial.print("Connected to ");
  Serial.println(deviceName);
}

/**
   Callback invoked when a connection is dropped
*/
void deviceDisconnectedCallback(uint16_t connectionHandle, uint8_t reason) {
  (void) connectionHandle;
  (void) reason;

  if (!DEBUG_MODE) {
    return;
  }

  Serial.println();
  Serial.println("Client disconnected");
}

float readVBAT(void) {
  float raw;

  // Set the analog reference to 3.0V (default = 3.6V)
  analogReference(AR_INTERNAL_3_0);

  // Set the resolution to 12-bit (0..4095)
  analogReadResolution(12); // Can be 8, 10, 12 or 14

  // Let the ADC settle
  delay(1);

  // Get the raw 12-bit, 0..3000mV ADC value
  raw = analogRead(vbat_pin);

  // Set the ADC back to the default settings
  analogReference(AR_DEFAULT);
  analogReadResolution(10);

  // Convert the raw value to compensated mv, taking the resistor-
  // divider into account (providing the actual LIPO voltage)
  // ADC range is 0..3000mV and resolution is 12-bit (0..4095)
  return raw * REAL_VBAT_MV_PER_LSB;
}

uint8_t mvToPercent(float mvolts) {
  if (mvolts < 3300)
    return 0;

  if (mvolts < 3600) {
    mvolts -= 3300;
    return mvolts / 30;
  }

  mvolts -= 3600;
  return 10 + (mvolts * 0.15F );  // thats mvolts /6.66666666
}

void logBatteryResult(float vbatMv, uint8_t vbatPer) {
  if (!DEBUG_MODE) {
    return;
  }
  Serial.print("LIPO = ");
  Serial.print(vbatMv);
  Serial.print(" mV (");
  Serial.print(vbatPer);
  Serial.println("%)");
}

void logMoistureSensor(uint8_t moistureVal) {
  if (!DEBUG_MODE) {
    return;
  }

  uint8_t moisturePer = moistureVal * 100 / 256;
  Serial.print("Soil moisture = ");
  Serial.print(moisturePer);
  Serial.println("%");
}

void gainCallback(uint16_t conn_hdl, BLECharacteristic* chr, uint8_t* data, uint16_t len) {
  //boolean lightOn = data[0] > 0;
  // TODO read data

   // TODO Set pot value

  if (!DEBUG_MODE) {
    return;
  }
  Serial.print("Gain Value = ");
  Serial.println(data[0]);
}

void toneCallback(uint16_t conn_hdl, BLECharacteristic* chr, uint8_t* data, uint16_t len) {
  // TODO read data

   // TODO Set pot value

  if (!DEBUG_MODE) {
    return;
  }
  Serial.print("Tone Value = ");
  Serial.println(data[0]);
}

void volumeCallback(uint16_t conn_hdl, BLECharacteristic* chr, uint8_t* data, uint16_t len) {
  // TODO read data

   // TODO Set pot value

  if (!DEBUG_MODE) {
    return;
  }
  Serial.print("Volume Value = ");
  Serial.println(data[0]);
}

void pot3Callback(uint16_t conn_hdl, BLECharacteristic* chr, uint8_t* data, uint16_t len) {
  // TODO read data

   // TODO Set pot value

  if (!DEBUG_MODE) {
    return;
  }
  Serial.print("Pot 3 Value = ");
  Serial.println(data[0]);
}
