// Sender - V1.2.02
//  * Added Sender CPU temperature reading and transmission to Receiver on every cycle, displayed on 
//    OLED and sent to Receiver for display on web page and OLED)
//  * Added a wifi access point to allow OTA updates of the Sender firmware using the ESP2SOTA library. 
//    When the Debug Mode magnetic switch is made, the Sender will stay awake and start a WiFi access point for 
//    OTA updates. The access point is called "Gilboa_Sender" and has a password of "Gilboa_Sender".
//    The access point is configured to have a static IP address of 192.168.4.2.

// Sender - v1.2.01
// * Added Water detection - Water is detected between an open wire and the open ground wire next to it at the bottom of the sensor vinyl tube.
// * There are two water sensors, one at the bottom of the vinyl tube and one a few inches above it.
// * The sense of the water state is conveyed by two binary variables sent to the Receiver on the D, packet: water_bottom_detected and water_top_detected.

// Sender - v1.2.00
// * Send temps in string T,ROM address,temperature
// * aux_sleep_minutes,OLED_flag_from_receiver,debug_flag_from_receiver are stored in RTC memory and survive deep sleep
// * After data is sent, delay then wait for a packet from the Receiver (V,aux_sleep_minutes,OLED_flag_from_receiver,debug_flag_from_receiver)
// * Send data in string D,battery voltage,sender_version,SLEEP_MINUTES,aux_sleep_minutes,OLED_flag_from_receiver,debug_flag_from_receiver
// * Added flag in the Receiver to tell the Sender to enter debug mode (debug_flag_from_receiver)
// * Added the flag to tell the Sender to enter OLED active mode (OLED_flag_from_receiver)
// * Both flags are stored in NVS and survive deep sleep/power cycle
// * Added aux_sleep_minutes to allow changing of sleep time from Receiver
// * On first boot after power-on, default values are used and NVS is not read
// * After first boot, operator variables are loaded from NVS and saved on changes

// Sender - v1.1.01
// GPIO 2 = Debug mode - On a transition from low to high, stay awake, exit debug mode when signal is low
// Send version number to receiver 
// If a theralcouple data is missing, send -196.6 to indicate no data (future)

// Sender - v1.0.04
// GPIO 4 = OLED BLANK SWITCH
// Short GPIO 4 to GND = OLED OFF
// Open = OLED ON
// NEW: On wake-up, read ALL probes immediately (parallel conversion)
// Then send EVERY reading to receiver with NO delays
// ONLY enter sleep AFTER all readings have been sent
//

#define sender_version "v1.2.02"

#include <RadioLib.h>
#include <SPI.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <Adafruit_DS248x.h>
#include <Preferences.h>

// WiFi Server for OTA updates
#include <WiFi.h>
#include <WiFiAP.h>
#include <WebServer.h>
/* INCLUDE ESP2SOTA LIBRARY */
#include <ESP2SOTA.h>

const char* ssid = "Gilboa_Sender";
const char* password = "Gilboa_Sender";

WebServer server(80);

// These are the pins for the LoRa module. The LoRa module is an SX1262, and it is connected to the microcontroller using SPI. The CS pin is used to select the LoRa module, the DIO1 pin is used for interrupts, the RST pin is used to reset the module, and the BUSY pin is used to check if the module is busy.
#define LORA_MOSI 10
#define LORA_MISO 11
#define LORA_SCK  9
#define LORA_CS   8
#define LORA_RST  12
#define LORA_BUSY 13
#define LORA_DIO1 14
#define LORA_FREQ 915.0f

// Number of 1-wire sensors
#define NUMBER_1WIRE_TEMPERATURE_SENSORS 14 ; Number of Temperature probes
#define NUMBER_1WIRE_WATER_DETECTORS 1 ; Number of Water detection sensors (DS2413)

// These are the pins for the DS2484 1-Wire master. The VEXT_CTRL_PIN is used to control power to the DS2484, and the ADC_CTRL_PIN is used to control power to the battery voltage measurement circuit.
#define VEXT_CTRL_PIN 36
#define DS248X_ADDRESS 0x18
#define DS18B20_FAMILY_CODE 0x28
#define DS18B20_CMD_CONVERT_T 0x44
#define DS18B20_CMD_MATCH_ROM 0x55
#define DS18B20_CMD_READ_SCRATCHPAD 0xBE
#define DS18B20_CMD_SEARCH_ROM 0xF0

// These are the two water detection sensors. They work by having an open wire and an open ground wire next to each other at the bottom of the vinyl tube. When water is present, it creates a conductive path between the two wires, which can be detected by the microcontroller.
#define DS2413_FAMILY_CODE 0x3A
#define DS2413_CMD_READ 0xF5

// Battery voltage measurement
#define BATTERY_PIN 1
#define ADC_CTRL_PIN 37
#define BATTERY_SAMPLES 20

// <<< OLED ACTIVE SWITCH >>>
#define OLED_ACTIVE_PIN 4          // Pull to 3 volt = OLED ON
bool oledCurrentlyActive = false;

// <<< Debug Mode - GPIO 2 - high to stay awake >>>
#define Debug_Mode_PIN 2  // GPIO pin for Debug Mode Deep Sleep interupt
#define Debug_State_PIN 7 // GPIO pin to indicate Debug State HIGH=Debug Mode

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, 21);
SX1262 LoRa = new Module(LORA_CS, LORA_DIO1, LORA_RST, LORA_BUSY);
Adafruit_DS248x ds248x;

Preferences prefs;

uint8_t roms[14][8];
uint8_t ds2413Rom[8];                 // Single DS2413 water detect ROM
bool hasDS2413 = false;               // Flag: do we have a DS2413?

float Sender_temp_farenheit = 0.0f;   // Sender CPU temperature in Fahrenheit

float temperatures[14];
String water_bottom_detected = "N/A"; // "N/A", "Y" or "N" to indicate if water is detected at the bottom sensor
String water_top_detected = "N/A"; // "N/A", "Y" or "N" to indicate if water is detected at the top sensor
int deviceCount = 0;

bool allReadingsSent = false;     // Track if all temps transmitted
int delayBetweenPackets = 200; // Delay time between sending of packets (ms) 200 old
unsigned long cycleStartTime = 0;
const unsigned long MAX_WAIT_MS = 2000; // Max wait time for receiver responce

const int SLEEP_MINUTES = 30; //Default Sleep time between readings

// === RTC MEMORY VARIABLES (survive deep sleep) ===
RTC_DATA_ATTR int     aux_sleep_minutes       = 30;   // default only on first boot
RTC_DATA_ATTR bool    OLED_flag_from_receiver  = false;
RTC_DATA_ATTR bool    debug_flag_from_receiver = false;

// Flag to detect first boot after power-on (survives deep sleep too)
RTC_DATA_ATTR bool    vars_initialized = false;

bool inSleepMode = false;
bool justWokeUp = true;
unsigned long elapsedSeconds;

String lastRssiStr = "N/A";

// === NVS: Load/Save Operator Variables ===
void loadOperatorVars() {
  prefs.begin("operator", true);
  // Only load from NVS if not first boot after reset
  if (vars_initialized) {
    aux_sleep_minutes       = prefs.getInt("aux_sleep", aux_sleep_minutes);
    OLED_flag_from_receiver  = prefs.getBool("oled_flag", OLED_flag_from_receiver);
    debug_flag_from_receiver = prefs.getBool("debug_flag", debug_flag_from_receiver);
  }
  prefs.end();
  Serial.printf("Loaded operator vars: aux_sleep=%d, OLED_flag=%d, debug_flag=%d\n",
                aux_sleep_minutes, OLED_flag_from_receiver, debug_flag_from_receiver);
}

void saveOperatorVars() {
  prefs.begin("operator", false);
  prefs.putInt("aux_sleep", aux_sleep_minutes);
  prefs.putBool("oled_flag", OLED_flag_from_receiver);
  prefs.putBool("debug_flag", debug_flag_from_receiver);
  prefs.end();
  Serial.println("Operator variables saved to NVS");
}

void ds2484_power_on() {
  pinMode(VEXT_CTRL_PIN, OUTPUT);
  digitalWrite(VEXT_CTRL_PIN, LOW);
  delay(10);
}

void ds2484_power_off() {
  digitalWrite(VEXT_CTRL_PIN, HIGH);
}

float readBatteryVoltage() {
  #define ADC_READ_STABILIZE 10
  digitalWrite(ADC_CTRL_PIN, HIGH);
  delay(ADC_READ_STABILIZE);
  uint32_t raw = 0;
  for (int i = 0; i < BATTERY_SAMPLES; i++) raw += analogRead(BATTERY_PIN);
  raw /= BATTERY_SAMPLES;
  digitalWrite(ADC_CTRL_PIN, LOW);
  return 5.42 * (3.3 / 4096.0) * raw;
}

bool readDS2413PIO(uint8_t *rom, uint8_t &pioLogic) {
  if (!ds248x.OneWireReset()) {
    Serial.println("1-Wire reset failed before reading DS2413");
    return false;
  }
  // Match ROM
  ds248x.OneWireWriteByte(0x55);  // Match ROM command
  for (int j = 0; j < 8; j++) {
    ds248x.OneWireWriteByte(rom[j]);
  }
  // Send Read PIO Registers command
  ds248x.OneWireWriteByte(0xF5);
  ds248x.OneWireReadByte(&pioLogic);  // PIO Logic State
  Serial.printf("DS2413 PIO Data: 0x%02X  \n", pioLogic);
  // PIO logic bits: bit 4 = PIOA, bit 6 = PIOB, bits 7-4 should be complement of bits 3-0
  // PIO = 0 B 0 A  1 ~B 1 ~A 
  // Check if we got a valid responce, bits 7-4 should be the complement of bits 3-0, if not, we likely have a read error
  if (((pioLogic & 0xF0) >> 4) != (~pioLogic & 0x0F)) {
    Serial.println("DS2413 PIO read error - invalid data");
    return false;
  }
  return true;
}


  bool initDS2484() {
  if (!ds248x.begin()) return false;
  Wire1.setClock(400000);
  return true;
}

float readTemperature(uint8_t *rom) {
  if (!ds248x.OneWireReset()) return -127.0;
  ds248x.OneWireWriteByte(DS18B20_CMD_MATCH_ROM);
  for (int i = 0; i < 8; i++) ds248x.OneWireWriteByte(rom[i]);
  ds248x.OneWireWriteByte(DS18B20_CMD_CONVERT_T);
  delay(750);

  if (!ds248x.OneWireReset()) return -127.0;
  ds248x.OneWireWriteByte(DS18B20_CMD_MATCH_ROM);
  for (int i = 0; i < 8; i++) ds248x.OneWireWriteByte(rom[i]);
  ds248x.OneWireWriteByte(DS18B20_CMD_READ_SCRATCHPAD);

  uint8_t data[9];
  for (int i = 0; i < 9; i++) ds248x.OneWireReadByte(&data[i]);
  int16_t raw = (data[1] << 8) | data[0];
  return raw / 16.0;
}

// <<< OLED ACTIVE CONTROL >>>
void updateOledActiveState() {
  bool switchState = (digitalRead(OLED_ACTIVE_PIN) == HIGH || OLED_flag_from_receiver == true);

  Serial.printf("OLED ACTIVE SWITCH STATE: %s \n oledCurrentlyActive: %s\n inSleepMode: %s\n", switchState ? "HIGH (ON)" : "LOW (OFF)", oledCurrentlyActive ? "TRUE" : "FALSE", inSleepMode ? "TRUE" : "FALSE");

  if (!switchState && oledCurrentlyActive && !inSleepMode) {
    Serial.println("OLED ACTIVE SWITCH: False, Turning OLED OFF");
    u8g2.setPowerSave(1);
    oledCurrentlyActive = false;
  }
  //else if (switchState && !oledCurrentlyActive && !inSleepMode) {
  else if (switchState &&  !inSleepMode) {
    Serial.println("OLED ACTIVE SWITCH: True, Turning OLED ON");
    u8g2.setPowerSave(0);
    oledCurrentlyActive = true;
  }
}

// Read and transmit ALL probes immediately on wake
void readAndTransmitAllProbes() {
  if (allReadingsSent) return;

  Serial.println(">>> READING AND TRANSMITTING ALL PROBES NOW <<<");

  // Start conversion on ALL probes
  if (ds248x.OneWireReset()) {
    ds248x.OneWireWriteByte(0xCC);  // Skip ROM
    ds248x.OneWireWriteByte(DS18B20_CMD_CONVERT_T);
  }

  float batteryV = readBatteryVoltage();    // Read battery voltage
  // Read CPU temperature
  float temp_celsius = temperatureRead();
  Sender_temp_farenheit = (temp_celsius * 9.0/5.0) + 32.0;
  Serial.printf("Sender CPU Temp: %.1f °F\n", Sender_temp_farenheit);
  delay(750);  // Wait for all to convert

// Handle OLED display
  if (oledCurrentlyActive) {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_ncenB08_tr);


    u8g2.drawStr(0, 10, ("Sender: " + String(sender_version)).c_str());
    u8g2.drawStr(0, 20, ("Sensors: " + String(deviceCount)).c_str());
    u8g2.drawStr(0, 30, ("Batt: " + String(batteryV, 2) + "V").c_str());
    u8g2.drawStr(0, 40, ("LORA RSSI: " + lastRssiStr).c_str());
    u8g2.drawStr(0, 50, ("ROM Sleep: " + String(SLEEP_MINUTES) + " min").c_str());
    u8g2.drawStr(0, 60, ("Aux Sleep: " + String(aux_sleep_minutes) + " min").c_str());

    u8g2.sendBuffer();
    }


  // Read and transmit each probe immediately
  // Send Temperature packet
  for (int i = 0; i < deviceCount; i++) {
    temperatures[i] = readTemperature(roms[i]);

    String romStr = "T,";
    for (int j = 0; j < 8; j++) {
      if (roms[i][j] < 16) romStr += "0";
      romStr += String(roms[i][j], HEX);
      if (j < 7) romStr += " ";
    }
    romStr.toUpperCase();
    String sendPacket = romStr + "," + String(temperatures[i], 2);
    LoRa.startReceive();
    int state = LoRa.transmit(sendPacket);
    if (state == RADIOLIB_ERR_NONE) {
    Serial.println(sendPacket);
    } else {
      Serial.printf("TX failed on probe %d (error %d)\n", i, state);
    }
    delay(delayBetweenPackets); // Short delay between packets
  }
  
// Read the water detector (only one DS2413)
if (hasDS2413) {
  uint8_t pioLogic;
  if (readDS2413PIO(ds2413Rom, pioLogic)) {
    // Assuming low = water detected (conductive path pulls to ground)
    // PIOA (bit 4) = top sensor, PIOB (bit 6) = bottom sensor
    water_top_detected    = (pioLogic & 0x10) ? "N" : "Y";  // bit 4
    water_bottom_detected = (pioLogic & 0x40) ? "N" : "Y";  // bit 6
    Serial.printf("DS2413 detected - Water Bottom: %s, Water Top: %s\n", 
                  water_bottom_detected.c_str(), water_top_detected.c_str());
  } else {
    water_top_detected    = "ERR";
    water_bottom_detected = "ERR";
    Serial.println("DS2413 read failed");
  }
} else {
  water_top_detected    = "N/A";
  water_bottom_detected = "N/A";
  Serial.println("No DS2413 water detector found");
}

  // Finally, send Data packet
  String sendPacket = "D," + String(batteryV, 2) + "," + String(sender_version) + "," + String(SLEEP_MINUTES) 
          + "," + String(aux_sleep_minutes) + "," + String(OLED_flag_from_receiver) + "," + String(debug_flag_from_receiver)
          + "," + String(water_bottom_detected) + "," + String(water_top_detected) + "," + String(Sender_temp_farenheit, 1);
  sendPacket.toUpperCase();
//  LoRa.startReceive();
  int state = LoRa.transmit(sendPacket);
  if (state == RADIOLIB_ERR_NONE) {
    Serial.println(sendPacket);
  } else {
    Serial.printf("TX failed on Data packet (error: %d)\n", state);
  }
  delay(delayBetweenPackets); // Short delay between packets

// Send packet to denote all readings sent  
  sendPacket =  "C"; // Complete packet indicator;
  state = LoRa.transmit(sendPacket);
  if (state == RADIOLIB_ERR_NONE) {
    Serial.println(sendPacket);
  } else {
    Serial.printf("TX failed on C packet (error: %d)\n", state);
  }
  delay(delayBetweenPackets); // Short delay between packets

  allReadingsSent = true;
  Serial.println(">>> ALL READINGS SENT - READY FOR SLEEP <<<");
}

// Listen and wait a set time for a "V,xx,yy,zz" packet from receiver
void receiveVariablesFromReceiver() {
  unsigned long startWait = millis();
  while (millis() - startWait < MAX_WAIT_MS) {
    String rxPacket;
    int rxState = LoRa.receive(rxPacket);
    if (rxState == RADIOLIB_ERR_NONE && rxPacket.length() > 0) {
      Serial.print("Received during wait: "); Serial.println(rxPacket);
      if (rxPacket.startsWith("V,")) {
        // Split: V,aux_sleep,OLED_flag,debug_flag
        int c1 = rxPacket.indexOf(',');
        int c2 = rxPacket.indexOf(',', c1 + 1);
        int c3 = rxPacket.indexOf(',', c2 + 1);

        if (c1 > 0 && c2 > c1 && c3 > c2) {
          String auxStr   = rxPacket.substring(c1 + 1, c2);
          String oledStr  = rxPacket.substring(c2 + 1, c3);
          String debugStr = rxPacket.substring(c3 + 1);

          int newSleep = auxStr.toInt();
          bool newOled  = (oledStr == "1" || oledStr == "On" || oledStr == "on");
          bool newDebug = (debugStr == "1" || debugStr == "On" || debugStr == "on");
          if ((newSleep == aux_sleep_minutes) &&
              (newOled == OLED_flag_from_receiver) &&
              (newDebug == debug_flag_from_receiver)) {
            Serial.println("Received variables are the same as current settings. No changes made.");
            return; // No changes needed
          }
          if (newSleep >= 0 && newSleep <= 30) {
            aux_sleep_minutes = newSleep;
            Serial.printf("Updated aux_sleep_minutes from receiver: %d\n", aux_sleep_minutes);
          }
          OLED_flag_from_receiver = newOled;
          Serial.printf("Updated OLED_flag_from_receiver: %d (%s)\n", 
                        OLED_flag_from_receiver, OLED_flag_from_receiver ? "On" : "Off");
          debug_flag_from_receiver = newDebug;
          Serial.printf("Updated debug_flag_from_receiver: %d (%s)\n", 
                        debug_flag_from_receiver, debug_flag_from_receiver ? "On" : "Off");
          saveOperatorVars();
          return;  // Success - stop waiting
        }
      }
    }

    delay(10);  // small yield
  }

  Serial.println("No valid V,xx,yy,zz packet received within 2000 ms");
}

// Enter sleep mode for a specified number of minutes, or 1 minute if debug mode is active
void enterSleep() {
  int sleepTime;
  if (inSleepMode) return;
  inSleepMode = true;
    Serial.printf ("aux_sleep_minutes" " = %d\n", aux_sleep_minutes);
  sleepTime = SLEEP_MINUTES ;

  if (aux_sleep_minutes > 0) {
    sleepTime = aux_sleep_minutes;
  } else {
    if (digitalRead(Debug_State_PIN) == HIGH || debug_flag_from_receiver == true) {
      sleepTime = 1;
    }
  }
  Serial.printf(">>> ENTERING %d MIN SLEEP  <<<\n", sleepTime);

  u8g2.setPowerSave(1);
  oledCurrentlyActive = false;

  ds2484_power_off();
  delay(10);

  justWokeUp = true;
  allReadingsSent = false;

  esp_sleep_enable_ext0_wakeup(GPIO_NUM_2, 1);  // Configure GPIO 2 for EXT0 wake-up on RISING edge
  esp_sleep_enable_timer_wakeup(sleepTime * 60ULL * 1000000);
  esp_deep_sleep_start();
}

// Wake up from sleep mode, power up 1-wire board, and print the wake-up reason
void exitSleep() {
  inSleepMode = false;
  Serial.println(">>> WAKING UP <<<");
  
  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();

  switch (wakeup_reason) {
    case ESP_SLEEP_WAKEUP_EXT0:
      Serial.println("Woke up from deep sleep due to GPIO 2 RISING EDGE!");
      break;
    case ESP_SLEEP_WAKEUP_TIMER:
      Serial.println("Woke up from timer");
      break;
    default:
      Serial.println("Woke up from other source");
      break;
  }

  ds2484_power_on();
  delay(10);

  cycleStartTime = millis();
  oledCurrentlyActive = true;  // Force check

}

// Debug OTA mode - start WiFi access point for OTA updates
void OTA_debug_mode() { 
  Serial.println("Debug mode active - starting WiFi access point for OTA updates");
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password);
  delay(1000);
  IPAddress IP = IPAddress(192, 168, 4, 2);
  IPAddress NMask = IPAddress(255, 255, 255, 0);
  WiFi.softAPConfig(IP, IP, NMask);
  IPAddress myIP = WiFi.softAPIP();
  Serial.print("Access Point IP address: ");
  Serial.println(myIP);
  // SETUP YOUR WEB OWN ENTRY POINTS 
  server.on("/myurl", HTTP_GET,[]() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/plain", "Hello there!"); 
  });
  // INITIALIZE ESP2SOTA LIBRARY
  ESP2SOTA.begin(&server);  
    server.begin();
}

void setup() {
  esp_log_level_set("Preferences", ESP_LOG_NONE); 
  Serial.begin(115200);
  delay(1000);
  Serial.println("=== SENDER " + String(sender_version) + " ===");
  Serial.println("On wake: read + transmit ALL probes ");

  pinMode(BATTERY_PIN, INPUT);
  pinMode(ADC_CTRL_PIN, OUTPUT);
  digitalWrite(ADC_CTRL_PIN, HIGH);

  pinMode(OLED_ACTIVE_PIN, INPUT_PULLDOWN); // Set OLED Active pin as input with pull-down
  pinMode (Debug_State_PIN,INPUT_PULLDOWN); // Set Debug State pin as input with pull-down

  ds2484_power_on();

  loadOperatorVars(); // Load operator variables from NVS

  Wire.begin(17, 18);
  Wire.setClock(100000);
  u8g2.begin();
  Wire1.begin(41, 42);
  if (!ds248x.begin(&Wire1, DS248X_ADDRESS)) {
    Serial.println("DS2484 init failed!");
    while (true);
  }
  Wire1.setClock(400000);

  int state = LoRa.begin(LORA_FREQ, 125.0, 7, 5, 0x34);

  if (state != RADIOLIB_ERR_NONE) {
    Serial.printf("LoRa init failed: %d\n", state);
    while (true);
  }

  // Handle the WiFi access point for OTA updates
  if (digitalRead(Debug_State_PIN) == HIGH ) {
    OTA_debug_mode();
  }

  LoRa.startReceive();

  // Count the number of devices on the 1-wire bus and identify them. Store the ROMs of the sensors in the roms array and count them in
  deviceCount = 0;
  hasDS2413 = false;

for (int attempt = 0; attempt < 3; ++attempt) {
    if (ds248x.OneWireReset()) {
      ds248x.OneWireWriteByte(DS18B20_CMD_SEARCH_ROM);
      while (ds248x.OneWireSearch(roms[deviceCount]) && deviceCount < 15) {

        Serial.print("Found device with ROM: ");
        for (int j = 0; j < 8; j++) {
          Serial.printf("%02X ", roms[deviceCount][j]);
        }
        Serial.println();


 if (roms[deviceCount][0] == DS18B20_FAMILY_CODE) {
        deviceCount++;  // only count temp sensors
      }
      else if (roms[deviceCount][0] == DS2413_FAMILY_CODE) {
        // Store the DS2413 ROM (only one expected)
        memcpy(ds2413Rom, roms[deviceCount], 8);
        hasDS2413 = true;
        Serial.println("DS2413 water detector found and stored");
        // Do NOT increment deviceCount — we don't count it as a temp sensor
      }
    }
    if (deviceCount > 0 || hasDS2413) break;
  }
  delay(100);
  }
  Serial.printf("Found %d DS18B20 sensors\n", deviceCount);
  if (hasDS2413) {
    Serial.print("DS2413 ROM: ");
    for (int j = 0; j < 8; j++) Serial.printf("%02X ", ds2413Rom[j]);
    Serial.println();
  }

  lastRssiStr = String(LoRa.getRSSI());

  cycleStartTime = millis();
  
  // Mark that we've initialized (survives deep sleep)
  vars_initialized = true;
}

void loop() {
  bool hardwareDebugMode; 

  if (justWokeUp) {
    if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER) {
      exitSleep();
    }
    if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT0) {
      exitSleep();
    }
    justWokeUp = false;
    return;
  }
  unsigned long now = millis();

  // Handle the WiFi access point for OTA updates
  if (digitalRead(Debug_State_PIN) == HIGH ) {
    hardwareDebugMode = true;
  /* HANDLE UPDATE REQUESTS */
    server.handleClient();  
  }else {
    hardwareDebugMode = false;
  }

  updateOledActiveState(); // Handle the OLDE Active Switch


  if (inSleepMode) { delay(100); return; }

  // Read and transmit all probes
  readAndTransmitAllProbes();
  receiveVariablesFromReceiver();

  // If hardware debug switch is made then skip sleep and stay awake for OTA updates
  if (!hardwareDebugMode) {
    // OK to sleep?
    if (allReadingsSent) {
      if(oledCurrentlyActive == true) {
        delay(5000);  // Wait 5 seconds before sleeping if OLED is ON
      }
      enterSleep();
      return;
    }
  } else {
    allReadingsSent = false; // Reset flag to allow re-reading and transmitting probes when debug mode is exited
  }
  delay(500);
}