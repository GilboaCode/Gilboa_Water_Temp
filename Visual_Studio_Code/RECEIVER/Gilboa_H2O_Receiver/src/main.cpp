// Receiver -- V1.3.01
//  * Modified the location of the firmware.bin to be in the "latest" folder of the GitHub repository to allow for future versioning of the firmware.
//
// Receiver -- V1.3.00
//  * Added a new command (/@UPDATE@)to the Telegram bot to update the Receiver remotely. It will download and install the latest firmware from the GitHub repository 
//
// Receiver -- V1.2.05
//  * Added a new command (/@RESET@)to the Telegram bot to reset the Receiver remotely. The command is /reset and it will restart the Receiver when issued.*
//  * Moved all the LoRa xmitt/rcve interrupts to a single core.
//
// Receiver -- V1.2.04
//  * Changed the reception of LORA commnication from polling to interrupt based.
//  * Hide the "debug-on" and "debug-off" commands from the Telegram help display, commands are still active.
//
// Receiver -- V1.2.03
//  * Changed text on web site display from "Battery" to "Sender Battery" to clarify that this is the battery voltage of the Sender, not the Receiver. 
//
// Receiver -- V1.2.02
// * Added Telegram Bot integration for:
//  /status command
//      Receiver IP address
//      Sender Temperature data (Air Temp + every 10 ft if detected)
//      Sender Battery Voltage
//      Sender Version
//      Receiver Version
//      Water detection status at top and bottom sensors
//      HTTP Status Code of last POST request to postUrl
//      Sleep timer status (running time / set sleep time for sender)
//      Display Mode of Sender (OLED Active)
//      Debug Mode of Sender
//  /rom  Display the thermocouple ROM addresses
//  /debug-on command to enable debug mode on the Sender 
//  /debug-off command to disable debug mode on the Sender
//  /help command to show available commands
//  * Added Sender and Receiver CPU temperature reading
//  * Added a Debug Telegram bot tolken when a second receiver is being used to debug  new code on the Sender without interfering with the main Receiver bot. 

// Receiver - v1.2.01
// * Added water detection status to the OLED display and the local web page.
// * Added email alert functionality to notify the operator when water is detected at the top or bottom sensor. The email recipient can be configured on the web page.
// *    Gilboa Water Temp project Gmail account
// *    GilboaWaterTemp@gmail.com
// *    Login Password: 87Gil2026 (only for manual login)
// *    App Password: efocbnzvxhcfwngx (app password for this project)
// * Added NTP client to get real time for email alerts and web page display. Time is displayed in UTC.
// * Added ability to configure multiple recipient emails (up to 6) on the web page. These are saved in NVS and loaded on startup.
// * Added ability to configure the sender email and password (app password) on the web page. These are saved in NVS and loaded on startup.

// *

//Receiver - v1.2.00
// * Receive temps from string T,ROM address,temperature
// * Receive data from string D,battery voltage,sender_version,SLEEP_MINUTES
// * After data received from Sender send the string  V,aux_sleep_minutes,OLED_flag_from_receiver,debug_flag_from_receiver
// * Added flag in the Receiver to tell the Sender to enter debug mode.
// * Added the flag to tell the Sender to enter OLED active mode.
// * Added a color change to RED in the "Position" field on the config screen when a thermocouple is not detected.
// * Added red color change to the submit buttons on the config page when there are unsaved changes.
// * Added Sleep time counter and setpoint on the main web page.


// Receiver - v1.1.01 
// • Display "Looking for WiFi" on power up
// * On the local web page display the version number of the Receiver
// * Added HTTP POST for link to web server
// * Added http status code to the OLED display and on the Config web page
// * Removed the 15 seceond refresh rate on the config web page to prevent interference with form submission
// * On the local web page display the version number of the Sender (FUTURE)
// * Added flag to tell the Sender to update every10 seconds for testing (FUTURE)

// Receiver - v1.0.07 (FINAL – GRAPH FIXED + EVERYTHING ELSE PRESERVED)
// • Physical WiFi reset: GPIO45 → GND only
// • No web reset page
// • All temperatures show correctly (including Air Temp)
// • Perfect working temperature vs depth graph

#define receiver_version "v1.3.01b"

#include <RadioLib.h>
#include <SPI.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <WebServer.h>
#include <Preferences.h>
#include <HTTPClient.h>
#include <ESP_Mail_Client.h>
#include <ctype.h>
#include <string.h>

#include <NTPClient.h>
#include <WiFiUdp.h>

#include <HTTPUpdate.h>
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include <esp_image_format.h>


// Telegram Bot
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
// Gilboa_WaterTemp_Debug_bot - "8328269756:AAGSF-JlY4pAeiHQWRxxzP-bReZdUqOJZxY"
// Gilboa_WaterTemp_bot - "8820302613:AAHUGMsmcHJNaG1YZnqkC-uzV6Y8-I1HwvA"
#define Telegram_Debug_Mode_Pin 4 // Short to ground to enable the Telegram tolken of Gilboa_WaterTemp_Debug_bot tolken
#define BOT_TOKEN "8820302613:AAHUGMsmcHJNaG1YZnqkC-uzV6Y8-I1HwvA"
#define BOT_TOKEN_Debug "8328269756:AAGSF-JlY4pAeiHQWRxxzP-bReZdUqOJZxY"
WiFiClientSecure secured_client;
UniversalTelegramBot bot(BOT_TOKEN, secured_client);

// Gmail SMTP Server Settings
#define SMTP_server "smtp.gmail.com"
#define SMTP_Port 465
String sender_email = "GilboaWaterTemp@gmail.com";
String sender_password = "efocbnzvxhcfwngx"; // App password for this project, not the actual email password
#define Recipient_name ""
SMTPSession smtp;


#define LORA_MOSI 10
#define LORA_MISO 11
#define LORA_SCK  9
#define LORA_CS   8
#define LORA_RST  12
#define LORA_BUSY 13
#define LORA_DIO1 14    // Interupt pin for LoRa module, used to detect when a packet is received
#define LORA_FREQ 915.0f


#define RESET_PIN 45
#define Display_On_Pin 36 //Set GPIO36 LOW before initializing the OLED display (display.begin())
bool resetTriggered = false;

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, 21);
WebServer server(80);
Preferences prefs;

// Graph Y-axis (saved in NVS)
float graphMinY = 32.0f;
float graphMaxY = 90.0f;

String  Recipient_email = "jeffklopping@gmail.com"; // Default recipient email, can be updated on the web page and saved in NVS

String postUrl = "http://jsonplaceholder.typicode.com/test"; // Default POST URL for testing, can be updated on the web page and saved in NVS
int httpCode=0;

// Setup the handle for the two code tasks
TaskHandle_t task0Handle;
TaskHandle_t task1Handle;
// Create the radio object
SX1262 LoRa = new Module(LORA_CS, LORA_DIO1, LORA_RST, LORA_BUSY);
// Global variables for interupt flag
bool shutUpImTransmitting = false;
bool newPacketReceived = false; 
struct LoRaPacket // Buffer for the incomming LoRa data
{
    char text[128];
};
QueueHandle_t loraQueue;
String sendPacket;
int delayTimeBeforeVPacket = 225; // Delay between packets in milliseconds

// Global state
String SenderVersion = "N/A";
String SenderSleepTime = "N/A";
String ReceiverVersion = receiver_version;

String lastSenderBatt = "N/A";
String lastSenderState = "Awake";
String lastRssiStr = "N/A";
unsigned long lastPacketTime = 0;
#define SLEEP_TIMEOUT_MS 20000
int sleep_time_for_sender ;

// Array for up to 6 recipient emails
String recipient_emails[6] = {
  "jeffklopping@gmail.com", "", "", "", "", ""
};

// Real time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 0, 60000);  // UTC offset 0, update every 60s

// Sender Water Detector 
String water_top_detected = "N/A";
String water_bottom_detected = "N/A";
bool topWaterAlertSent = false;    // true = alert already sent for top sensor
bool bottomWaterAlertSent = false; // true = alert already sent for bottom sensor

// Timer to track running time since last packet
unsigned long runningTimer = 0;
int minCounter = 0;

// Flag to send variables even on no change (startup)
bool resetVariablesFlag = false;
// Operator Variables to send to Sender
static int aux_sleep_minutes=0;
static boolean OLED_flag_from_receiver = false;
static boolean debug_flag_from_receiver = false;
// Operator variables old values for change detection
int old_aux_sleep_minutes=0;
boolean old_OLED_flag_from_receiver = false;
boolean old_debug_flag_from_receiver = false;

String uniqueRoms[14];
int uniqueRomCount = 0;

String romTable[14];
float temperatures[14];
bool romTableLoaded = false;
bool temperatureDetected[14] = {false};
bool temperatureDetectedHistory[14] = {false};
float defaultNoThermocoupleTemp = -196.6f; // Default temp when no thermocouple is connected

const char* depthLabels[14] = {
  "Air   ", "10  ft", "20  ft", "30  ft", "40  ft", "50  ft", "60  ft",
  "70  ft", "80  ft", "90  ft", "100 ft", "110 ft", "120 ft", "130 ft"
};

const int w = 600;
const int h = 300;
const int margin = 50;

// Processor temperatures
float Receiver_temp_farenheit = 0.0f;
String Sender_temp_farenheit ;

// Telegram Bot variables
bool telegramResetFlag = false; // Flag to indicate if the /reset command was received from the Telegram bot
bool telegramUpdateFlag = false; // Flag to indicate if the /update command was received from the Telegram bot
bool telegramUpdateFailedFlag = false; // Flag to indicate if the /update command failed
const char* firmwareURL ="https://github.com/GilboaCode/Gilboa_Water_Temp/releases/latest/download/firmware.bin";
bool updateStarted = false;


// === NVS ===
void loadGraphLimits() {
  prefs.begin("graph", true);
  graphMinY = prefs.getFloat("minY", 32.0f);
  graphMaxY = prefs.getFloat("maxY", 90.0f);
  prefs.end();
}

void saveGraphLimits() {
  prefs.begin("graph", false);
  prefs.putFloat("minY", graphMinY);
  prefs.putFloat("maxY", graphMaxY);
  prefs.end();
}

// === NVS: Load/Save Email Config ===
void loadEmailConfig() {
  prefs.begin("email", true);
  Recipient_email = prefs.getString("recipient_email", "jeffklopping@gmail.com");
  prefs.end();
  Serial.print("Loaded recipient email: "); Serial.println(Recipient_email);
}

void saveEmailConfig() {
  prefs.begin("email", false);
  prefs.putString("recipient_email", Recipient_email);
  prefs.end();
  Serial.print("Saved recipient email: "); Serial.println(Recipient_email);
}

void loadRecipients() {
  prefs.begin("recipients", true);
  for (int i = 0; i < 6; i++) {
    String key = "email" ;
    key += String(i);
    recipient_emails[i] = prefs.getString(key.c_str(), (i == 0 ? "jeffklopping@gmail.com" : ""));
  }
  prefs.end();
  String tempS = "Loaded ";
  tempS  += String(6);
  tempS  += " recipient emails from NVS";
  Serial.println(tempS);
}

void saveRecipients() {
  prefs.begin("recipients", false);
  for (int i = 0; i < 6; i++) {
    String key = "email";
    key += String(i);
    prefs.putString(key.c_str(), recipient_emails[i]);
  }
  prefs.end();
  Serial.println("Saved recipient emails to NVS");
}


void loadSenderCreds() {
  prefs.begin("sender", true);
  sender_email = prefs.getString("email", "GilboaWaterTemp@gmail.com");
  sender_password = prefs.getString("password", "efocbnzvxhcfwngx");
  prefs.end();
  Serial.println("Loaded sender credentials from NVS");
}

void saveSenderCreds() {
  prefs.begin("sender", false);
  prefs.putString("email", sender_email);
  prefs.putString("password", sender_password);
  prefs.end();
  Serial.println("Saved sender credentials to NVS");
}

// === NVS: Load/Save POST URL ===
void loadPostUrl() {
  prefs.begin("post", true);  // read-only mode first
  if (prefs.isKey("url")) {
    postUrl = prefs.getString("url", "http://jsonplaceholder.typicode.com/test");
  } else {
    Serial.println("POST URL namespace not found yet - using default");
  }
  prefs.end();
  Serial.print("Loaded POST URL: "); Serial.println(postUrl);
}

void savePostUrl() {
  prefs.begin("post", false);
  prefs.putString("url", postUrl);
  prefs.end();
  Serial.print("Saved POST URL: "); Serial.println(postUrl);
}

// === NVS: Load/Save Operator Variables ===
void loadOperatorVars() {
  prefs.begin("operator", true);
  aux_sleep_minutes = prefs.getInt("aux_sleep", 0);
  OLED_flag_from_receiver = prefs.getBool("oled_flag", false);
  debug_flag_from_receiver = prefs.getBool("debug_flag", false);
  prefs.end();

  // Clamp aux_sleep_minutes
  if (aux_sleep_minutes < 0) aux_sleep_minutes = 0;
  if (aux_sleep_minutes > 30) aux_sleep_minutes = 30;

  old_aux_sleep_minutes = aux_sleep_minutes;
  old_OLED_flag_from_receiver = OLED_flag_from_receiver;
  old_debug_flag_from_receiver = debug_flag_from_receiver;

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
// === NVS: Load/Save ROM Table ===
void loadRomTable() {
  if (romTableLoaded) return;
  prefs.begin("rom_table", true);
  for (int i = 0; i < 14; i++) {
    romTable[i] = prefs.getString(String(i).c_str(), "");
    romTable[i].toUpperCase(); romTable[i].trim();
  }
  prefs.end();
  romTableLoaded = true;
}

void saveRomTable() {
  prefs.begin("rom_table", false);
  for (int i = 0; i < 14; i++) prefs.putString(String(i).c_str(), romTable[i]);
  prefs.end();
}

bool isValidRom(String rom) {
  rom.trim();
  if (rom.length() < 23) return false;
  char *token = strtok((char*)rom.c_str(), " ");
  int count = 0;
  while (token != nullptr) {
    if (strlen(token) != 2) return false;
    for (int i = 0; i < 2; i++) if (!isHexadecimalDigit(token[i])) return false;
    count++;
    token = strtok(nullptr, " ");
  }
  return count == 8;
}


// === No WiFi - Display "Looking for WiFi" ===
void displayLookingForWiFi() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tr);
  u8g2.drawStr(0, 10, "Looking for WiFi");
  u8g2.drawStr(0, 20, "SSID: WaterTempRec");
  u8g2.drawStr(0, 30, "psw: password123");
  u8g2.drawStr(0, 40, "IP Address: 192.168.4.1");
  u8g2.sendBuffer();
}

// === GPIO45 RESET ===
void checkHardResetPin() {
  if (digitalRead(RESET_PIN) == LOW && !resetTriggered) {
    resetTriggered = true;
    Serial.println("\nGPIO45 → GND → WiFi RESET ONLY");
    WiFiManager wm; wm.resetSettings();
    u8g2.clearBuffer(); u8g2.setFont(u8g2_font_ncenB08_tr);
    u8g2.drawStr(0,20,"WiFi RESET"); u8g2.drawStr(0,40,"Rebooting...");
    u8g2.sendBuffer(); delay(2000); ESP.restart();
  }
  if (digitalRead(RESET_PIN) == HIGH) resetTriggered = false;
}

// === ROM HELPERS ===
bool isRomUnique(String rom) {
  rom.toUpperCase();
  for (int i = 0; i < uniqueRomCount; i++) if (uniqueRoms[i] == rom) return false;
  return true;
}

void addUniqueRom(String rom) {
  rom.toUpperCase(); rom.trim();
  if (uniqueRomCount < 14 && isRomUnique(rom)) uniqueRoms[uniqueRomCount++] = rom;
}

// === MAIN PAGE  ===
void handleRoot() {
  float minY = graphMinY;
  float maxY = graphMaxY;

  server.sendHeader("Content-Type", "text/html; charset=utf-8");
  unsigned long elapsed = (millis() - lastPacketTime) / 1000;
  char timeStr[9]; sprintf(timeStr, "%02d:%02d:%02d", elapsed/3600, (elapsed%3600)/60, elapsed%60);

  String html = "<!DOCTYPE html><html><head><meta charset='utf-8'><title>Water Gauge</title>";
  html += "<meta http-equiv='refresh' content='15'>";
  html += "<style>body{font-family:Arial;margin:20px;}table{border-collapse:collapse;width:70%;}";
  html += "th,td{border:1px solid #ddd;padding:8px;}th{background:#f0f0f0;}</style></head><body>";
  html += "<h1>Water Temperature Monitor</h1><p><a href='/romconfig'>Config</a></p>";
  html += "<table><tr><th>Pos</th><th>Depth</th><th>ROM</th><th>Temp (°F)</th></tr>";

for (int i = 0; i < 14; i++) {
 // String tempStr = (!temperatures[i] > -100.0) ? String(temperatures[i], 1) : "N/A";
  String tempStr = (temperatures[i] > -100.0) ? String(temperatures[i], 1) : "N/A";

  String posStyle = temperatureDetectedHistory[i] 
                    ? "font-weight:bold;" 
                    : "color:#d32f2f; font-weight:normal;"; // Red color if not detected

  html += "<tr>";
  html += String("<td style='");
  html += posStyle;
  html += "'>";
  html += String(i);
  html += "</td>";
  html += String("<td>") ;
  html += depthLabels[i] ;
  html += "</td>";
  html += String("<td>") ;
  html +=(romTable[i].length() ? romTable[i] : "—") ;
  html += "</td>";
  if (temperatureDetectedHistory[i] == false){
  html += String("<td>") ;
  html += tempStr ;
  html += "</td></tr>";
  }else {
  html += String("<td><b>") ;
  html += tempStr ;
  html += "</b></td></tr>";
  }
}
  html += "</table>";
  html += "<b>Sender Battery:</b> " ;
  html += lastSenderBatt ;
  html += " V";
  html += "<br>";
  html += "<b>SenderVersion:</b> " ;
  html += SenderVersion ;
  html += "<br>";
  html += "<b>Sender CPU Temperature:</b> " ;
  html += Sender_temp_farenheit;
  html +=  " °F";
  html +=  "<br>";
  html += "<b>ReceiverVersion:</b> " ;
  html +=  ReceiverVersion ;
  html +=  "<br>";
  html += "<b>Receiver CPU Temperature:</b> " ;
  html += String(Receiver_temp_farenheit, 1) ;
  html +=  " °F";
  html +=  "<br>";
  html += "<b>Water detected at top sensor:</b> " ;
  html +=  water_top_detected ;
  html += "<br>";
  html += "<b>Water detected at bottom sensor:</b> " ;
  html +=  water_bottom_detected ;
  html +=  "<br>";

// HTTP Status Code
  html += "<b>http Status Code:</b> " ;
  html +=  String(httpCode) ;
  html +=  "</br>";
// Sleep Timer
  html += "<b>Sleep Timer:</b> " ;
  html +=  String(runningTimer) ;
  html +=  " /" ;
  html +=  String(sleep_time_for_sender) ;
  html += "</p>";

  // === GRAPH ===
  html += "<canvas id='tempGraph' width='600' height='300' style='border:1px solid #ccc;margin-top:20px;display:block;'></canvas>";
  html += "<script>";
  html += "const canvas = document.getElementById('tempGraph');";
  html += "const ctx = canvas.getContext('2d');";
  html += "const w = 600, h = 300, margin = 50;";
  html += "const minY = " ;
  html +=  String(graphMinY, 1) ;
  html +=  ";";
  html += "const maxY = " ;
  html +=  String(graphMaxY, 1) ;
  html +=  ";";
  html += "const points = [];";

  for (int i = 1; i < 14; i++) {
    if (romTable[i].length() > 0 && temperatures[i] > -100.0) {
      float depth = i * 10.0f;
      float temp = temperatures[i];
      html += "points.push({x:" ;
      html +=  String(depth) ;
      html += ", y:" ;
      html +=  String(temp, 2) ;
      html +=  "});";
    }
  }

  html += "ctx.clearRect(0,0,w,h);";
  html += "ctx.strokeStyle = '#ddd'; ctx.lineWidth = 1;";
  html += "ctx.beginPath(); ctx.moveTo(margin, margin); ctx.lineTo(margin, h-margin);";
  html += "ctx.lineTo(w-margin, h-margin); ctx.stroke();";

  // Y-axis labels
  html += "ctx.fillStyle = '#000'; ctx.font = '12px Arial'; ctx.textAlign = 'right'; ctx.textBaseline = 'middle';";
  for (float t = floor(minY/10)*10; t <= maxY + 5; t += 10) {
    if (t > maxY) t = maxY;
    float y = margin + (maxY - t) / (maxY - minY) * (h - 2*margin);
    html += "ctx.fillText('" ;
    html +=  String((int)t) ;
    html +=  "', " ;
    html +=  String(margin-8) ;
    html +=  ", " ;
    html +=  String(y) ;
    html +=  ");";
    html += "ctx.beginPath(); ctx.moveTo(margin-5, " ;
    html +=  String(y) ;
    html +=  "); ctx.lineTo(margin, " ;
    html +=  String(y) ;
    html +=  "); ctx.stroke();";
  }

  // X-axis labels
  html += "ctx.textAlign = 'center';";
  for (int d = 10; d <= 130; d += 20) {
    float x = margin + (d/130.0f)*(w - 2*margin);
    html += "ctx.fillText('" ;
    html +=  String(d) ;
    html +=  " ft', " ;
    html += String(x) ;
    html +=  ", " ;
    html +=  String(h-margin+15) ;
    html +=  ");";
  }

  // Titles
  html += "ctx.save(); ctx.translate(18, h/2); ctx.rotate(-Math.PI/2); ctx.fillText('°F', 0, 0); ctx.restore();";
  html += "ctx.fillText('Temperature vs Depth', w/2, 25);";

  // Plot
  html += "if(points.length > 0){";
  html += "  ctx.strokeStyle = '#0066cc'; ctx.lineWidth = 3; ctx.beginPath();";
  html += "  ctx.fillStyle = '#0066cc';";
  html += "  points.forEach((p,i)=>{";
  html += "    let px = margin + (p.x/130)*(w-2*margin);";
  html += "    let py = margin + (maxY - p.y)/(maxY-minY)*(h-2*margin);";
  html += "    if(i===0) ctx.moveTo(px,py); else ctx.lineTo(px,py);";
  html += "    ctx.beginPath(); ctx.arc(px, py, 5, 0, Math.PI*2); ctx.fill();";
  html += "  }); ctx.stroke();";
  html += "}";
  html += "</script>";

  html += "</body></html>";
  server.send(200, "text/html", html);
}

// === CONFIG PAGE (Updated layout: side-by-side + buttons on right + HTTP boxed below) ===
void handleRomConfig() {
  float newMin ;
  float newMax;

  server.sendHeader("Content-Type", "text/html; charset=utf-8");

  if (server.method() == HTTP_POST) {
    // ROM assignment
    if (server.hasArg("position") && server.hasArg("rom")) {
      int pos = server.arg("position").toInt();
      String rom = server.arg("rom");
      Serial.printf("ROM Config POST: Pos=%d, ROM=%s\n", pos, rom.c_str());
      if (pos >= 0 && pos < 14) {
        if (rom == "Not Assigned") {
          romTable[pos] = "";
          temperatures[pos] = -196.6;
          Serial.printf("  Cleared position %d\n", pos);
        } else if (isValidRom(rom)) {
          romTable[pos] = rom;
          Serial.printf("  Set position %d to %s\n", pos, rom.c_str());
        } else {
          Serial.println("  Invalid ROM format");
        }
        saveRomTable();
      }
    }

    // Graph min/max
    if (server.hasArg("miny") && server.hasArg("maxy")) {
      newMin = server.arg("miny").toFloat();
      newMax = server.arg("maxy").toFloat();
      if (newMin < newMax && newMin >= 0 && newMax <= 200) {
        graphMinY = newMin;
        graphMaxY = newMax;
        saveGraphLimits();
      }
    }

    // POST URL
    if (server.hasArg("posturl")) {
      String newUrl = server.arg("posturl");
      if (newUrl.length() > 5 && newUrl.startsWith("http")) {
        postUrl = newUrl;
        savePostUrl();
        Serial.println("POST URL updated via web config");
      } else {
        Serial.println("Invalid POST URL - not saved");
      }
    }

    // Multiple Recipient Emails (up to 6)
    bool recipientsChanged = false;
    for (int i = 0; i < 6; i++) {
      String argName = "recipient_email";
      argName += String(i);
      if (server.hasArg(argName)) {
        String newEmail = server.arg(argName);
        newEmail.trim();
        if (newEmail.length() > 0 && newEmail.indexOf("@") > 0) {
          if (newEmail != recipient_emails[i]) {
            recipient_emails[i] = newEmail;
            recipientsChanged = true;
          }
        } else if (newEmail.length() == 0) {
          recipient_emails[i] = "";
          recipientsChanged = true;
        }
      }
    }
    if (recipientsChanged) {
      saveRecipients();
      Serial.println("Recipient emails updated via web config");
    }

    // Sender Email & Password
    bool senderCredsChanged = false;
    if (server.hasArg("sender_email")) {
      String newSenderEmail = server.arg("sender_email");
        newSenderEmail.trim();
      if (newSenderEmail.length() > 5 && newSenderEmail.indexOf("@") > 0) {
        if (newSenderEmail != sender_email) {
          sender_email = newSenderEmail;
          senderCredsChanged = true;
        }
      }
    }
    if (server.hasArg("sender_password")) {
      String newPassword = server.arg("sender_password");
      if (newPassword.length() > 0 && newPassword != sender_password) {
        sender_password = newPassword;
        senderCredsChanged = true;
      }
    }
    if (senderCredsChanged) {
      saveSenderCreds();
      Serial.println("Sender email/password updated via web config");
    }

    // Operator variables
    if (server.hasArg("aux_sleep")) {
      int newVal = server.arg("aux_sleep").toInt();
      if (newVal >= 0 && newVal <= 30) {
        aux_sleep_minutes = newVal;
        Serial.printf("aux_sleep_minutes updated to %d\n", aux_sleep_minutes);
      } else {
        Serial.println("aux_sleep_minutes out of range (0–30) - ignored");
      }
    }

    if (server.hasArg("oled_flag")) {
      OLED_flag_from_receiver = (server.arg("oled_flag") == "On");
      Serial.printf("OLED_flag_from_receiver updated to %d\n", OLED_flag_from_receiver);
    }

    if (server.hasArg("debug_flag")) {
      debug_flag_from_receiver = (server.arg("debug_flag") == "On");
      Serial.printf("debug_flag_from_receiver updated to %d\n", debug_flag_from_receiver);
    }

    saveOperatorVars();

    server.sendHeader("Location", "/romconfig");
    server.send(303);
    return;
  }

  // === Build Config Page HTML ===
  Serial.println("Serving /romconfig page");
  String html = "<!DOCTYPE html><html><head><title>Config</title>";
  html += "<style>body{font-family:Arial;margin:20px;}table{border-collapse:collapse;width:70%;}";
  html += "th,td{border:1px solid #ddd;padding:6px;text-align:left;font-size:14px;}";
  html += "th{background:#f2f2f2;}select,input{padding:4px; margin:5px 0;}";
  html += "fieldset{margin:20px 0; padding:15px; border:2px solid #ccc; border-radius:8px;}";
  html += "legend{font-weight:bold; padding:0 10px;}";
  html += ".side-by-side{display:flex; gap:20px; flex-wrap:wrap;}";
  html += ".side-by-side fieldset{flex:1; min-width:300px;}";
  html += ".input-row{display:flex; align-items:center; gap:10px; flex-wrap:wrap;}";
  html += ".input-row label{margin-right:10px;}";
  html += ".changed-submit { background:#d32f2f !important; color:white !important; border:1px solid #b71c1c !important; cursor:pointer; }</style></head><body>";
  html += "<h1>Config</h1>";
  html += "<p><a href='/'>Back</a></p>";

  // ROM table (full width, no box)
  html += "<table><tr><th>Pos</th><th>Depth</th><th>Current</th><th>New ROM</th><th>Set</th></tr>";
  for (int i = 0; i < 14; i++) {
    String currentRom = romTable[i].length() > 0 ? romTable[i] : "Not Assigned";

    html += "<tr>";
    html += "<td>";
    html += String(i);
    html += "</td>";
    html += "<td>";
    html += depthLabels[i];
    html += "</td>";
    html += "<td>";
    html += currentRom;
    html += "</td>";
    html += "<td>";
    html += "<form method='post' style='display:inline;' id='romForm_";
    html += String(i);
    html += "'>";
    html += "<input type='hidden' name='position' value='";
    html += String(i);
    html += "'>";
    html += "<select name='rom' id='romSelect_";
    html += String(i);
    html += "' onchange='highlightSetButton(";
    html += String(i);
    html += ")'>";
    html += "<option value='Not Assigned'>Not Assigned</option>";
    for (int j = 0; j < uniqueRomCount; j++) {
      String romOption = uniqueRoms[j];
      html += "<option value='";
      html += romOption;
      html += "'";
      if (currentRom == romOption) html += " selected";
      html += ">";
      html += romOption;
      html += "</option>";
    }
    html += "</select>";
    html += "</td><td>";
    html += "<input type='submit' id='setButton_";
    html += String(i);
    html += "' value='Set'>";
    html += "</form>";
    html += "</td></tr>";
  }
  html += "</table>";

  // Side-by-side: Graph + Operator
  html += "<div class='side-by-side'>";

  // Left box: Graph Y-Axis Settings
  html += "<fieldset id='graphFieldset'>";
  html += "<legend>Graph Y-Axis Settings</legend>";
  html += "<form method='post' id='graphForm'>";
  html += "<div class='input-row'>";
  html += "<label>Min (°F): </label>";
  html += "<input type='number' step='0.1' name='miny' id='minYInput' value='";
  html += String(graphMinY, 1);
  html += "' style='width:80px;' onchange='checkGraphChanges()'>";
  html += "<label>  Max (°F): </label>";
  html += "<input type='number' step='0.1' name='maxy' id='maxYInput' value='";
  html += String(graphMaxY, 1);
  html += "' style='width:80px;' onchange='checkGraphChanges()'>";
  html += "<input type='submit' id='graphSubmit' value='Update Graph Scale' style='margin-left:15px;'>";
  html += "</div>";
  html += "</form>";
  html += "</fieldset>";

  // Right box: Operator Variables
  html += "<fieldset id='operatorFieldset'>";
  html += "<legend>Operator Variables (Sent to Sender)</legend>";
  html += "<form method='post' id='operatorForm'>";

  html += "<div class='input-row'>";
  html += "<label>OLED Active: </label>";
  html += "<select name='oled_flag' id='oledSelect' onchange='checkOperatorChanges()'>";
  html += "<option value='On'";
  html += String(OLED_flag_from_receiver ? " selected" : "");
  html += ">On</option>";
  html += "<option value='Off'";
  html += String(!OLED_flag_from_receiver ? " selected" : "");
  html += ">Off</option>";
  html += "</select>";
  html += "<label>  Debug Mode: </label>";
  html += "<select name='debug_flag' id='debugSelect' onchange='checkOperatorChanges()'>";
  html += "<option value='On'";
  html += String(debug_flag_from_receiver ? " selected" : "");
  html += ">On</option>";
  html += "<option value='Off'";
  html += String(!debug_flag_from_receiver ? " selected" : "");
  html += ">Off</option>";
  html += "</select>";
  html += "</div>";

  html += "<div class='input-row'>";
  html += "<label>Aux Sleep (0–30): </label>";
  html += "<input type='number' name='aux_sleep' id='auxSleepInput' min='0' max='30' value='";
  html += String(aux_sleep_minutes);
  html += "' style='width:70px;' onchange='checkOperatorChanges()'>";
  html += "<input type='submit' id='operatorSubmit' value='Update Operator Settings' style='margin-left:15px;'>";
  html += "</div>";

  html += "</form>";
  html += "</fieldset>";

  html += "</div>";

  // New row: Sender Credentials (left) + HTTP POST Settings (right)
  html += "<div class='side-by-side'>";

  // Sender Credentials box (left)
  html += "<fieldset id='senderCredsFieldset'>";
  html += "<legend>Sender Email Credentials</legend>";
  html += "<form method='post' id='senderForm'>";
  html += "<div class='input-row'>";
  html += "<label>Sender Email: </label>";
  html += "<input type='text' name='sender_email' id='senderEmailInput' value='";
  html += sender_email;
  html += "' style='width:300px;' onchange='checkSenderChanges()'>";
  html += "</div>";
  html += "<div class='input-row' style='margin-top:10px;'>";
  html += "<label>App Password: </label>";
  html += "<input type='text' name='sender_password' id='senderPassInput' value='";
  html += sender_password;
  html += "' style='width:300px;' onchange='checkSenderChanges()'>";
  html += "</div>";
  html += "<div class='input-row' style='margin-top:15px;'>";
  html += "<input type='submit' id='senderSubmit' value='Update Sender Credentials' style='margin-left:15px;'>";
  html += "</div>";
  html += "</form>";
  html += "</fieldset>";

  // HTTP POST Settings box (right – restored)
  html += "<fieldset id='httpFieldset'>";
  html += "<legend>HTTP POST Settings</legend>";
  html += "<form method='post' id='httpForm'>";
  html += "<div class='input-row'>";
  html += "<label>HTTP POST URL: </label>";
  html += "<input type='text' name='posturl' id='postUrlInput' value='";
  html += postUrl;
  html += "' style='width:300px;' onchange='checkHttpChanges()'>";
  html += "<input type='submit' id='httpSubmit' value='Update POST URL' style='margin-left:15px;'>";
  html += "</div>";
  html += "</form>";
  html += "</fieldset>";

  html += "</div>";  // end side-by-side for Sender + HTTP

  // New separate row below: Water Detection Recipients
  html += "<fieldset id='recipientsFieldset' style='margin-top:20px;'>";
  html += "<legend>Water Detection Email Recipients (up to 6)</legend>";
  html += "<form method='post' id='recipientsForm'>";
  for (int i = 0; i < 6; i++) {
    html += "<div class='input-row' style='margin-bottom:8px;'>";
    html += "<label>Recipient ";
    html += String(i+1);
    html += ": </label>";
    html += "<input type='text' name='recipient_email";
    html += String(i);
    html += "' id='recipientEmailInput";
    html += String(i);
    html += "' value='";
    html += recipient_emails[i];
    html += "' style='width:300px;' onchange='checkRecipientsChanges()'>";
    html += "</div>";
  }
  html += "<div class='input-row' style='margin-top:15px;'>";
  html += "<input type='submit' id='recipientsSubmit' value='Update Recipient Emails' style='margin-left:15px;'>";
  html += "</div>";
  html += "</form>";
  html += "</fieldset>";

  // === JavaScript for all forms ===
  html += "<script>";
  // Graph changes
  html += "function checkGraphChanges() {";
  html += "  const submit = document.getElementById('graphSubmit');";
  html += "  const minVal = document.getElementById('minYInput').value;";
  html += "  const maxVal = document.getElementById('maxYInput').value;";
  html += "  const origMin = '";
  html += String(graphMinY, 1);
  html += "';";
  html += "  const origMax = '";
  html += String(graphMaxY, 1);
  html += "';";
  html += "  const changed = (minVal != origMin) || (maxVal != origMax);";
  html += "  submit.className = changed ? 'changed-submit' : '';";
  html += "}";

  // Operator changes
  html += "function checkOperatorChanges() {";
  html += "  const submit = document.getElementById('operatorSubmit');";
  html += "  const auxVal = document.getElementById('auxSleepInput').value;";
  html += "  const oledVal = document.getElementById('oledSelect').value;";
  html += "  const debugVal = document.getElementById('debugSelect').value;";
  html += "  const origAux = '";
  html += String(aux_sleep_minutes);
  html += "';";
  html += "  const origOled = '";
  html += (OLED_flag_from_receiver ? "On" : "Off");
  html += "';";
  html += "  const origDebug = '";
  html += (debug_flag_from_receiver ? "On" : "Off");
  html += "';";
  html += "  const changed = (auxVal != origAux) || (oledVal != origOled) || (debugVal != origDebug);";
  html += "  submit.className = changed ? 'changed-submit' : '';";
  html += "}";

  // HTTP changes
  html += "function checkHttpChanges() {";
  html += "  const submit = document.getElementById('httpSubmit');";
  html += "  const urlVal = document.getElementById('postUrlInput').value.trim();";
  html += "  const origUrl = '";
  html += postUrl;
  html += "';";
  html += "  const changed = (urlVal != origUrl);";
  html += "  submit.className = changed ? 'changed-submit' : '';";
  html += "}";

  // Sender credentials changes
  html += "function checkSenderChanges() {";
  html += "  const submit = document.getElementById('senderSubmit');";
  html += "  const emailVal = document.getElementById('senderEmailInput').value.trim();";
  html += "  const passVal = document.getElementById('senderPassInput').value.trim();";
  html += "  const origEmail = '";
  html += sender_email;
  html += "';";
  html += "  const origPass = '";
  html += sender_password;
  html += "';";
  html += "  const changed = (emailVal !== origEmail) || (passVal !== origPass);";
  html += "  submit.className = changed ? 'changed-submit' : '';";
  html += "}";

// Recipients changes – check all 6 fields
  html += "function checkRecipientsChanges() {";
  html += "  const submit = document.getElementById('recipientsSubmit');";
  html += "  let changed = false;";
  html += "  const originals = [";
  html += "'";
  html +=  recipient_emails[0] ;
  html +=  "',";
  html += "'" ;
  html +=  recipient_emails[1] ;
  html +=  "',";
  html += "'" ;
  html +=  recipient_emails[2];
  html +=  "',";
  html += "'" ;
  html +=  recipient_emails[3] ;
  html +=  "',";
  html += "'" ;
  html +=  recipient_emails[4] ;
  html +=  "',";
  html += "'" ;
  html +=  recipient_emails[5];
  html += "'";
  html += "];";

  // Loop through each input and compare
  html += "  for (let i = 0; i < 6; i++) {";
  html += "    const input = document.getElementById('recipientEmailInput' + i);";
  html += "    if (input) {";
  html += "      const val = input.value.trim();";
  html += "      const orig = originals[i];";
  html += "      if (val !== orig) {";
  html += "        changed = true;";
  html += "        break;";
  html += "      }";
  html += "    }";
  html += "  }";

  html += "  submit.className = changed ? 'changed-submit' : '';";
  html += "}";

  // Reset buttons on submit
  html += "document.getElementById('graphForm').addEventListener('submit', () => { document.getElementById('graphSubmit').className = ''; });";
  html += "document.getElementById('operatorForm').addEventListener('submit', () => { document.getElementById('operatorSubmit').className = ''; });";
  html += "document.getElementById('httpForm').addEventListener('submit', () => { document.getElementById('httpSubmit').className = ''; });";
  html += "document.getElementById('senderForm').addEventListener('submit', () => { document.getElementById('senderSubmit').className = ''; });";
  html += "document.getElementById('recipientsForm').addEventListener('submit', () => { document.getElementById('recipientsSubmit').className = ''; });";

  // Initial checks on load
  html += "checkGraphChanges();";
  html += "checkOperatorChanges();";
  html += "checkHttpChanges();";
  html += "checkSenderChanges();";
  html += "checkRecipientsChanges();";
  html += "</script>";

  // JavaScript for ROM row submit buttons (unchanged)
  html += "<script>";
  html += "function checkRomChange(pos) {";
  html += "  const select = document.getElementById('romSelect_' + pos);";
  html += "  const button = document.getElementById('setButton_' + pos);";
  html += "  const current = select.value;";
  html += "  const orig = select.getAttribute('data-original') || 'Not Assigned';";
  html += "  const changed = (current !== orig);";
  html += "  if (changed) {";
  html += "    button.classList.add('changed-submit');";
  html += "  } else {";
  html += "    button.classList.remove('changed-submit');";
  html += "  }";
  html += "}";
  html += "for (let i = 0; i < 14; i++) {";
  html += "  const form = document.getElementById('romForm_' + i);";
  html += "  if (form) {";
  html += "    form.addEventListener('submit', () => {";
  html += "      document.getElementById('setButton_' + i).classList.remove('changed-submit');";
  html += "    });";
  html += "  }";
  html += "}";
  html += "document.querySelectorAll('select[id^=\"romSelect_\"]').forEach(select => {";
  html += "  const current = select.value;";
  html += "  select.setAttribute('data-original', current);";
  html += "  checkRomChange(select.id.split('_')[1]);";
  html += "});";
  html += "function highlightSetButton(pos) {";
  html += "  document.getElementById('setButton_' + pos).classList.add('changed-submit');";
  html += "}";
  html += "document.querySelectorAll('form[id^=\"romForm_\"]').forEach(form => {";
  html += "  form.addEventListener('submit', function() {";
  html += "    const pos = form.id.split('_')[1];";
  html += "    const button = document.getElementById('setButton_' + pos);";
  html += "    if (button) button.classList.remove('changed-submit');";
  html += "  });";
  html += "});";
  html += "</script>";

  html += "</body></html>";
  server.send(200, "text/html", html);
}



// === HTTP POST depth & temperature data ===
void postTemperaturesToJsonPlaceholder() {
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  http.begin(postUrl);  // ← uses the configurable URL
  http.addHeader("Content-Type", "application/json");
  //http.addHeader("Content-Type", "text/plain");


  String payload = "[";
  bool first = true;

  for (int i = 0; i < 14; i++) {
    if (temperatures[i] > -100.0) {  // only send valid readings
      if (!first) payload += ",";
      payload += "{\"depth\":\"" ;
      payload +=  String(depthLabels[i]) ;
      payload += "\",";
      payload += "\"temp\":" ;
      payload += String(temperatures[i], 1) ;
      payload += "}";
      first = false;
    }
  }
  payload += "]";

  // If first=true, it means no valid temperatures were added, so payload will be "[]"
  
  Serial.print("POST to "); Serial.print(postUrl); Serial.print(" payload: "); Serial.println(payload);

  httpCode = http.POST(payload);
  if (httpCode > 0) {
    Serial.printf("POST returned code: %d\n", httpCode);
    if (httpCode == 201 || httpCode == 200) {
      Serial.println("Data successfully sent to jsonplaceholder");
    }
  } else {
    Serial.println("POST failed");
  }

  http.end();
}

void updateOLED() {
  // OLED CODE
  String debugStr ;
  sleep_time_for_sender ;
  sleep_time_for_sender = atoi(SenderSleepTime.c_str());
  if (aux_sleep_minutes != 0) {
    sleep_time_for_sender = aux_sleep_minutes;
  } else {
    if (debug_flag_from_receiver == true) {
      sleep_time_for_sender = 1; // Use the 1 min update time
    } 
  }
  String tempS = "";
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tr);
  tempS = "Water L: " ;
  String tempstr = String(water_bottom_detected) ;
  tempstr.toLowerCase();
  tempS += tempstr ;
  tempS += " U: " ;
  tempstr =String(water_top_detected);
  tempstr.toLowerCase();
  tempS += tempstr ;
  u8g2.drawStr(0, 10, (tempS).c_str());
  tempS="";
  tempS += "t/c : " ;
  tempS += String(uniqueRomCount) ;
  tempS += "       Sleep: " ;
  tempS +=  String(runningTimer) ;
  tempS +=  "/" ;
  tempS +=  String(sleep_time_for_sender);
  u8g2.drawStr(0, 20, (tempS).c_str());
  tempS = "";
  tempS += "Batt : " ;
  tempstr = lastSenderBatt;
  tempstr.toLowerCase();
  tempS += tempstr ;
  tempS +=  " v  post: ";
  tempS +=  String(httpCode);
  u8g2.drawStr(0, 30, (tempS).c_str());
  if ( debug_flag_from_receiver == true ){
    debugStr = "Debug";
  } else {
    debugStr = "Normal";
  }
  tempS = "";
  tempS += "Mode: " ;
  tempS += debugStr;
  u8g2.drawStr(0, 40, (tempS).c_str());
  tempS = "";
  tempS +="LORA RSSI: " ;
  tempstr = lastRssiStr;
  tempstr.toLowerCase();
  tempS +=  tempstr;
  u8g2.drawStr(0, 50, (tempS).c_str());
  tempS = "" ;
  tempS += "IP:" ;
  tempS +=  WiFi.localIP().toString() ;
  tempS += "  " ;
  
  u8g2.drawStr(0, 60, (tempS).c_str());
  u8g2.sendBuffer();
}

/* Callback function to get the Email sending status */
void smtpCallback(SMTP_Status status){
  /* Print the current status */
  Serial.println(status.info());

  /* Print the sending result */
  if (status.success()){
    // ESP_MAIL_PRINTF used in the examples is for format printing via debug Serial port
    // that works for all supported Arduino platform SDKs e.g. AVR, SAMD, ESP32 and ESP8266.
    // In ESP8266 and ESP32, you can use Serial.printf directly.

    Serial.println("----------------");
    ESP_MAIL_PRINTF("Message sent success: %d\n", status.completedCount());
    ESP_MAIL_PRINTF("Message sent failed: %d\n", status.failedCount());
    Serial.println("----------------\n");

    for (size_t i = 0; i < smtp.sendingResult.size(); i++)
    {
      /* Get the result item */
      SMTP_Result result = smtp.sendingResult.getItem(i);

      // In case, ESP32, ESP8266 and SAMD device, the timestamp get from result.timestamp should be valid if
      // your device time was synched with NTP server.
      // Other devices may show invalid timestamp as the device time was not set i.e. it will show Jan 1, 1970.
      // You can call smtp.setSystemTime(xxx) to set device time manually. Where xxx is timestamp (seconds since Jan 1, 1970)
      
      ESP_MAIL_PRINTF("Message No: %d\n", i + 1);
      ESP_MAIL_PRINTF("Status: %s\n", result.completed ? "success" : "failed");
      ESP_MAIL_PRINTF("Date/Time: %s\n", MailClient.Time.getDateTimeString(result.timestamp, "%B %d, %Y %H:%M:%S").c_str());
      ESP_MAIL_PRINTF("Recipient: %s\n", result.recipients.c_str());
      ESP_MAIL_PRINTF("Subject: %s\n", result.subject.c_str());
    }
    Serial.println("----------------\n");

    // You need to clear sending result as the memory usage will grow up.
    smtp.sendingResult.clear();
  }
}



// Email handler
void sendEmailAlert(String emailsubject, String emailmessage) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Cannot send email - WiFi not connected");
    return;
  }

  for (int i = 0; i < 6; i++) {
    if (recipient_emails[i].length() > 5 && recipient_emails[i].indexOf("@") > 0) {
      Serial.printf("Added email recipient: %s\n", recipient_emails[i].c_str());

      /*  Set the network reconnection option */
      MailClient.networkReconnect(true);
      /** Enable the debug via Serial port
       * 0 for no debugging
       * 1 for basic level debugging
       *
       * Debug port can be changed via ESP_MAIL_DEFAULT_DEBUG_PORT in ESP_Mail_FS.h
       */
      smtp.debug(1);
      /* Set the callback function to get the sending results */
      smtp.callback(smtpCallback);
    
      ESP_Mail_Session session;
      session.server.host_name = SMTP_server ;
      session.server.port = SMTP_Port;
      session.login.email = sender_email;
      session.login.password = sender_password;
      session.login.user_domain = "";

      // Set the session NTP config time
      session.time.ntp_server = F("pool.ntp.org,time.nist.gov");
      session.time.gmt_offset = 5;
      session.time.day_light_offset = 0;


      SMTP_Message message;
      message.sender.name = "Gilboa Water Temp Receiver";
      message.sender.email = sender_email;
      message.subject = emailsubject;
      message.addRecipient("Recipient ", recipient_emails[i]);

      //Send HTML message
      String htmlMsg = "<div style=\"color: #000000;\"><h1> " ;
      htmlMsg += emailmessage;
      htmlMsg += "</h1><p> Mail Generated from  Gilboa Water Temperature Receiver</p></div>";
      message.html.content = htmlMsg.c_str();
      message.html.content = htmlMsg.c_str();
      message.text.charSet = "us-ascii";
      message.html.transfer_encoding = Content_Transfer_Encoding::enc_7bit;

      /*Send raw text message
      String textMsg = "Hello World! - Sent from ESP board";
      message.text.content = textMsg.c_str();
      message.text.charSet = "us-ascii";
      message.text.transfer_encoding = Content_Transfer_Encoding::enc_7bit;
      */
      message.priority = esp_mail_smtp_priority::esp_mail_smtp_priority_low;
      message.response.notify = esp_mail_smtp_notify_success | esp_mail_smtp_notify_failure | esp_mail_smtp_notify_delay;

      if (!smtp.connect(&session)){
        return;
      }
      if (!MailClient.sendMail(&smtp, &message)){
        Serial.print("Error sending Email, " );
        Serial.println(smtp.errorReason());
      }
    }
  }
}

//
// /help or /start command - show available commands
void command_help (String chat_id, String text){
  String runningText  = "Commands:";
  runningText += "\n";
  runningText += " /status - Status of Receiver and Sender";
  runningText += "\n";
  runningText += " /rom - ROM assignments";
  runningText += "\n";
  runningText += " /help - Available commands";
  bot.sendMessage(chat_id,runningText,"");
}

// /help or /start command - show available commands
void command_superhelp (String chat_id, String text){
  String runningText  = "Commands:";
  runningText += "\n";
  runningText += " /status - Status of Receiver and Sender";
  runningText += "\n";
  runningText += " /rom - ROM assignments";
  runningText += "\n";
  runningText += " /debug-on - Sender Debug mode on(1 min update time)";
  runningText += "\n";
  runningText += " /debug-off - Sender Debug mode off";
  runningText += "\n";
  runningText += " /ota - Receiver OTA information";  
  runningText += "\n";
  runningText += " /_UPDATE - Update Receiver firmware";
  runningText += "\n";
  runningText += " /_RESET - Receiver Reset";
  runningText += "\n";
  runningText += " /help - Available commands";
  runningText += "\n";
  runningText += " /superhelp - Super User commands";
  bot.sendMessage(chat_id,runningText,"");
}


//  /status command
//      Receiver IP address
//      Sender Temperature data (Air Temp + every 10 ft if detected)
//      Sender Battery Voltage
//      Sender Version
//      Receiver Version
//      Water detection status at top and bottom sensors
//      HTTP Status Code of last POST request to postUrl
//      Sleep timer status (running time / set sleep time for sender)
//      Display Mode of Sender (OLED Active)
//      Debug Mode of Sender
void command_status (String chat_id,String text) {
  String runningText = "Receiver IP Address: " ; 
  runningText +=  WiFi.localIP().toString();
  
  for (int i = 0; i < 14; i++) {
  runningText += "\n";
  runningText += "depth - " ;
  runningText +=  String(depthLabels[i]) ;
  runningText += "  temp - " ;
  if (temperatures[i] < -100.0) {
    runningText += "N/A" ;
    } else {
    runningText += String(temperatures[i], 1) ;
    }
  }
  runningText += "\nSender Batt: " ;
  runningText += lastSenderBatt ;
  runningText += " v\nSender Ver: " ;
  runningText += SenderVersion ;
  runningText += "\nSender CPU Temp: " ;
  runningText += Sender_temp_farenheit ;
  runningText += " °F" ;  
  runningText += "\nReceiver Ver: " ;
  runningText += receiver_version ;
  runningText += "\nReceiver CPU Temp: " ;
  runningText += String(Receiver_temp_farenheit, 1) ;
  runningText += " °F" ;
  runningText += "\nWater L: " ;
  String tempstr = String(water_bottom_detected) ;
  tempstr.toLowerCase();
  runningText += tempstr ;
  runningText += " U: " ;          
  tempstr =String(water_top_detected);
  tempstr.toLowerCase();
  runningText += tempstr ;
  runningText += "\nHTTP POST: " ;
  runningText += String(httpCode);
  runningText += "\nSleep Timer: " ;
  runningText += String(runningTimer) ;
  runningText += "/" ;
  runningText += String(sleep_time_for_sender);
  runningText += "\nOLED Active: " ;
  runningText += (OLED_flag_from_receiver ? "On" : "Off");
  runningText += "\nDebug Mode: " ;
  runningText += (debug_flag_from_receiver ? "On" : "Off");
  bot.sendMessage(chat_id, runningText, "");
}

//  /OTA command
//      Firmware version of Receiver
//      Build date of Receiver
//      Running Partition of Receiver
//      Boot Partition of Receiver
//      Next OTA target partition of Receiver
//      OTA Slot 0 partition of Receiver
//      OTA Slot 1 partition of Receiver
void command_ota (String chat_id,String text) {
  String runningText = "Firmware Version: " ; 
  runningText +=  receiver_version;
  runningText += "\nBuild date       : " ;
  runningText += String(__DATE__) ;
  runningText += " " ;
  runningText += String(__TIME__);
  runningText += "\nRunning partition: " ;
  runningText +=  esp_ota_get_running_partition()->label;
  runningText += "\nBoot partition   : " ;
  runningText +=  esp_ota_get_boot_partition()->label;    
  runningText += "\nNext OTA target  : " ;
  runningText +=  esp_ota_get_next_update_partition(NULL)->label;
  runningText += "\nOTA Slot 0       : " ;
  runningText +=  esp_partition_find_first(ESP_PARTITION_TYPE_APP,ESP_PARTITION_SUBTYPE_APP_OTA_0,NULL)->label;
  runningText += "\nOTA Slot 0 Offset: " ;
  runningText +=  String(esp_partition_find_first(ESP_PARTITION_TYPE_APP,ESP_PARTITION_SUBTYPE_APP_OTA_0,NULL)->address, HEX);
  runningText += "\nOTA Slot 0 Size  : " ;
  runningText +=  String(esp_partition_find_first(ESP_PARTITION_TYPE_APP,ESP_PARTITION_SUBTYPE_APP_OTA_0,NULL)->size);
  runningText += "\nOTA Slot 1       : " ; 
  runningText +=  esp_partition_find_first(ESP_PARTITION_TYPE_APP,ESP_PARTITION_SUBTYPE_APP_OTA_1,NULL)->label;   
  runningText += "\nOTA Slot 1 Offset: " ;
  runningText +=  String(esp_partition_find_first(ESP_PARTITION_TYPE_APP,ESP_PARTITION_SUBTYPE_APP_OTA_1,NULL)->address, HEX);  
  runningText += "\nOTA Slot 1 Size  : " ;
  runningText +=  String(esp_partition_find_first(ESP_PARTITION_TYPE_APP,ESP_PARTITION_SUBTYPE_APP_OTA_1,NULL)->size);
  runningText += "\nOTA Update URL: " ;
  runningText +=  firmwareURL;
 
  bot.sendMessage(chat_id, runningText, "");
}

// Command debug-on, Turn on the debug mode of the Sender
void command_debug_on (String chat_id, String text) {
  debug_flag_from_receiver = true;
  bot.sendMessage(chat_id, "Debug mode turned ON", "");
}

// Command debug-off, Turn off the debug mode of the Sender
void command_debug_off (String chat_id, String text) {
  debug_flag_from_receiver = false;
  bot.sendMessage(chat_id, "Debug mode turned OFF", "");
}

// Command *RESET* ,Reset the Receiver 
void command_reset (String chat_id, String text) {
  telegramResetFlag = true;
  bot.sendMessage(chat_id, "Receiver is resetting...", "");
}

// Command update, update the Receiver firmware
void command_update (String chat_id, String text) {
  bot.sendMessage(chat_id, "Updating Receiver firmware...", "");
  telegramUpdateFlag = true;
}

// Command rom, display the rom addresses of the thermocouple rom addresses
void command_rom( String chat_id, String text){
  String runningText = "";
  for (int i = 0; i < 14; i++) {
    runningText += (depthLabels[i]);
    String currentRom = romTable[i].length() > 0 ? romTable[i] : "Not Assigned";
    runningText += ("  ROM - ");
    runningText += (currentRom);
    runningText += "\n";
  }
  bot.sendMessage(chat_id, runningText, "");
}


// Print partition information to Serial Monitor
void printOTAInfo()
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    const esp_partition_t *boot    = esp_ota_get_boot_partition();
    const esp_partition_t *next    = esp_ota_get_next_update_partition(NULL);

    const esp_partition_t *app0 =
        esp_partition_find_first(ESP_PARTITION_TYPE_APP,
                                 ESP_PARTITION_SUBTYPE_APP_OTA_0,
                                 NULL);

    const esp_partition_t *app1 =
        esp_partition_find_first(ESP_PARTITION_TYPE_APP,
                                 ESP_PARTITION_SUBTYPE_APP_OTA_1,
                                 NULL);

    Serial.println();
    Serial.println("========== OTA Information ==========");

    Serial.printf("Firmware version : %s\n", receiver_version);
    Serial.printf("Build date       : %s %s\n", __DATE__, __TIME__);

    if (running)
        Serial.printf("Running partition: %s\n", running->label);

    if (boot)
        Serial.printf("Boot partition   : %s\n", boot->label);

    if (next)
        Serial.printf("Next OTA target  : %s\n", next->label);

    Serial.println();

    if (app0)
    {
        Serial.printf("OTA Slot 0 (%s)\n", app0->label);
        Serial.printf("    Offset : 0x%06X\n", app0->address);
        Serial.printf("    Size   : %u bytes (%.1f KB)\n",
                      app0->size,
                      app0->size / 1024.0);
    }

    if (app1)
    {
        Serial.printf("OTA Slot 1 (%s)\n", app1->label);
        Serial.printf("    Offset : 0x%06X\n", app1->address);
        Serial.printf("    Size   : %u bytes (%.1f KB)\n",
                      app1->size,
                      app1->size / 1024.0);
    }

    Serial.println("=====================================");
    Serial.println();
}

// Resolve HTTP redirects and return the final URL
String resolveRedirect(const char *url)
{
    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;

    // Don't automatically follow redirects
    http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);

    if (!http.begin(client, url))
    {
        Serial.println("HTTP begin failed");
        return "";
    }

    int code = http.GET();

    Serial.printf("HTTP Response = %d\n", code);

    if (code == HTTP_CODE_MOVED_PERMANENTLY ||
        code == HTTP_CODE_FOUND ||
        code == HTTP_CODE_TEMPORARY_REDIRECT ||
        code == HTTP_CODE_PERMANENT_REDIRECT)
    {
        String newURL = http.header("Location");

        Serial.println("Redirect URL:");
        Serial.println(newURL);

        http.end();
        return newURL;
    }

    http.end();

    return String(url);
}


// OTA update function
void performOTA()
{
  Serial.println("\nStarting OTA update...");
  telegramUpdateFailedFlag=false; // Clear update failed flag before starting OTA
  WiFiClientSecure client;

  client.setInsecure();
  httpUpdate.rebootOnUpdate(true);
  httpUpdate.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);

  Serial.printf("Downloading: %s\n", firmwareURL);
  t_httpUpdate_return result =
    httpUpdate.update(client, firmwareURL);

  switch (result)
  {
  case HTTP_UPDATE_FAILED:
    Serial.printf("Update failed. Error (%d): %s\n",
    httpUpdate.getLastError(),
    httpUpdate.getLastErrorString().c_str());
    telegramUpdateFailedFlag = true;
    break;

  case HTTP_UPDATE_NO_UPDATES:
    Serial.println("No update available.");
    break;

  case HTTP_UPDATE_OK:
    // Never reached.
    // The ESP automatically reboots.
    Serial.println("Update successful.");
    break;
  }
}

// Telegram Bot - Handle incoming messages
void handleNewMessages(int numNewMessages)
{
  for (int i = 0; i < numNewMessages; i++)
  {
    String chat_id = bot.messages[i].chat_id;
    String text = bot.messages[i].text;

    // Diag print
    String runningText = text;
    runningText +=  "Received message: ";
    runningText += text;
    runningText += " from chat ID: ";
    runningText += chat_id;
    Serial.println(runningText);

    if (text == "/start" || text == "/help" || text == "?") command_help (chat_id,text);
    if (text == "/status") command_status(chat_id,text);
    if (text == "/rom") command_rom(chat_id,text);
    if (text == "/ota") command_ota(chat_id,text);
    if (text == "/debug-on") command_debug_on (chat_id,text);
    if (text == "/debug-off") command_debug_off (chat_id,text);
    if (text == "/_UPDATE") command_update(chat_id,text);
    if (text == "/_RESET") command_reset(chat_id,text);
    if (text == "/superhelp") command_superhelp(chat_id,text);
  }
}

//
// Telegram Bot - Check for new messages
void checkTelegram() {
  if (WiFi.status() != WL_CONNECTED) return;

  int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
  while (numNewMessages) {
    handleNewMessages(numNewMessages);
    numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    if (telegramResetFlag==true) {
      ESP.restart(); // reset the ESP32 if the reset command was received
    }
    if (telegramUpdateFlag==true) {
      telegramUpdateFlag = false;
      performOTA();
      if (telegramUpdateFailedFlag==true) {
        bot.sendMessage(bot.messages[0].chat_id, "OTA update failed. Check Serial Monitor for details.", "");
      } else {
        bot.sendMessage(bot.messages[0].chat_id, "OTA update successful. ESP32 will reboot.", "");
      }
    }
  }
}



// ====================== INTERUPT CALLBACK (ISR) ======================
// Get actual packet length first (recommended for SX1262)
// Absolute minimum work
void IRAM_ATTR onPacketReceived()
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    vTaskNotifyGiveFromISR(task0Handle, &xHigherPriorityTaskWoken);

    if (xHigherPriorityTaskWoken)
        portYIELD_FROM_ISR();
}


// LoRa SETUP 
void setupLoRa() {
  Serial.println("Initializing SX1262...");
  delay(1000);

  int state = LoRa.begin(LORA_FREQ, 125.0, 7, 5, 0x34);
  if (state != RADIOLIB_ERR_NONE) {
    Serial.printf("Radio init failed: %d\n", state);
    while (true);
  } 

 // Attach interrupt callback
  LoRa.setPacketReceivedAction(onPacketReceived);

  // Start continuous receive
  state = LoRa.startReceive();
  if (state != RADIOLIB_ERR_NONE) {
    Serial.printf("Start receive failed: %d\n", state);
  }
  Serial.println("LoRa SX1262 receiver with interrupt ready (DIO1 on GPIO14)");
}



// Read the packet from the LoRa hardware and send it to the queue for processing
void LoRaTask(void *pvParameters)
{
  LoRaPacket packet;
  char buffer[128];
  while (true){
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    int16_t state = LoRa.readData((uint8_t*)packet.text,sizeof(packet.text) - 1);
    if(state == RADIOLIB_ERR_NONE){
      packet.text[LoRa.getPacketLength()] = '\0';


      if (strcmp(packet.text, "C") == 0){
        Serial.println("Complete Packet");
        for (int i = 0; i < 14; i++) {
          temperatureDetectedHistory[i] = temperatureDetected[i];
          temperatureDetected[i] = false; // Reset for next packet
        }
        // Read Receiver CPU temperature
        float temp_celsius = temperatureRead();
        Receiver_temp_farenheit = (temp_celsius * 9.0/5.0) + 32.0;
        //Serial.printf("Receiver CPU Temp: %.1f °F\n", Receiver_temp_farenheit);

        sendPacket = "V," ;
        sendPacket +=  String(aux_sleep_minutes) ;
        sendPacket +=  "," ;
        sendPacket +=  String(OLED_flag_from_receiver ? 1 : 0) ;
        sendPacket += "," ;
        sendPacket +=  String(debug_flag_from_receiver ? 1 : 0);
        sendPacket.toUpperCase();

        delay(delayTimeBeforeVPacket); // Small delay before sending to avoid collision

        int state = LoRa.transmit(sendPacket);
        if (state == RADIOLIB_ERR_NONE) {
          Serial.println(sendPacket);
        } else {
          Serial.printf("TX failed on Variable packet (error: %d)\n", state);
        }
        old_aux_sleep_minutes = aux_sleep_minutes;
        old_OLED_flag_from_receiver = OLED_flag_from_receiver;
        old_debug_flag_from_receiver = debug_flag_from_receiver;
        resetVariablesFlag = false;  
        
        
        runningTimer = 0; // Reset running timer on new packet
        lastSenderState = "Awake";
        newPacketReceived = true;


      }

      xQueueSend(loraQueue, &packet, portMAX_DELAY);
    }
    LoRa.startReceive();
  }
}

void ProcessTask(void *pvParameters){
  LoRaPacket packet;

  while (true){
    if (xQueueReceive(loraQueue, &packet, portMAX_DELAY) == pdTRUE){
      //Serial.print("Received: ");
      Serial.println(packet.text);

      // Process the packet here...
      String newPacket;
      newPacket = packet.text;

      // Read the first Character to determine the data type
      // T = Temperature packet, D = Data packet, C = Complete packet
      String command= newPacket.substring (0,1);
      char cm= command.charAt(0);
      if (cm=='T' || cm=='D' ) {
        int c1 = newPacket.indexOf(',');
        int c2 = newPacket.indexOf(',', c1 + 1);
        int c3 = newPacket.indexOf(',', c2 + 1);
        int c4 = newPacket.indexOf(',', c3 + 1);
        int c5 = newPacket.indexOf(',', c4 + 1);
        int c6 = newPacket.indexOf(',', c5 + 1);
        int c7 = newPacket.indexOf(',', c6 + 1);
        int c8 = newPacket.indexOf(',', c7 + 1);
        int c9 = newPacket.indexOf(',', c8 + 1);

        //Serial.printf("c1=%d, c2=%d, c3=%d, c4=%d, c5=%d, c6=%d, c7=%d, c8=%d, c9=%d\n", c1, c2, c3, c4, c5, c6, c7, c8, c9);
        //Serial.printf("Received packet:  cm='%c'\n",  cm);
        //Serial.printf("Full packet: %s\n", packet.c_str());
        if (cm == 'T') {
          // Serial.println("Temperature Packet");
          // Temperature packet: T,ROM address,temperature
          String rom = newPacket.substring(c1 + 1, c2);
          String tempCStr = newPacket.substring(c2 + 1);
          
          rom.toUpperCase(); rom.trim();
          float tempC = tempCStr.toFloat();
          float tempF = tempC * 9.0 / 5.0 + 32.0;

          bool matched = false;
          for (int i = 0; i < 14; i++) {
            String saved = romTable[i];
            saved.toUpperCase(); saved.trim();
            if (saved.length() > 0 && saved == rom) {
              temperatures[i] = tempF;
              temperatureDetected[i] = true;
              Serial.printf("Pos %d (%s) = %.1f °F\n", i, rom.c_str(), tempF);
              matched = true;
            }
          }
          if (!matched) Serial.printf("Unassigned ROM: %s = %.1f °F\n", rom.c_str(), tempF);

          addUniqueRom(rom);
          lastRssiStr = String(LoRa.getRSSI());
          lastPacketTime = millis();
          runningTimer = 0; // Reset running timer on new packet
          lastSenderState = "Awake";
        }
        if (cm == 'D') {
          // Serial.println("Data Packet");
          // Data packet: D,battery voltage,sender_version,SLEEP_MINUTES, OLED_FLAG, DEBUG_FLAG, water_bottom_detected, water_top_detected, Sender CPU temperature
          String battStr = newPacket.substring(c1 + 1, c2);
          SenderVersion = newPacket.substring(c2 + 1, c3);
          String sleepStr = newPacket.substring(c3 + 1, c4);
          SenderSleepTime = sleepStr.toInt();
          int tempSleep = newPacket.substring(c4 + 1, c5).toInt();
          bool tempOLED = newPacket.substring(c5 + 1, c6).toInt() != 0;
          bool tempDebug = newPacket.substring(c6 + 1,c7).toInt() != 0;
          water_bottom_detected = newPacket.substring(c7 + 1,c8);
          water_top_detected = newPacket.substring(c8 + 1,c9);
          if (c9 == -1) {
            Sender_temp_farenheit = "N/A";
          } else {
            Sender_temp_farenheit = newPacket.substring(c9 + 1);
          }
          Serial.println(Sender_temp_farenheit);

          if (tempSleep == aux_sleep_minutes &&
              tempOLED == OLED_flag_from_receiver &&
              tempDebug == debug_flag_from_receiver) {
            // No change
          } else {
            resetVariablesFlag = true; // Force sending updated variables
            Serial.println("Send Variable packet to Sender");
          }
          lastSenderBatt = battStr;
          lastRssiStr = String(LoRa.getRSSI());
          /*
          Serial.println("Data Packet Received:");
          Serial.print("  Sender Battery: " );
          Serial.print(  battStr );
          Serial.println(" V");
          Serial.print("  Sender Version: " );
          Serial.println( SenderVersion);
          Serial.print("  Sleep Minutes: " );
          Serial.println(  sleepStr);
          Serial.print("  Aux Sleep Minutes: " );
          Serial.println(  String(tempSleep));
          Serial.print("  OLED Flag: " );
          Serial.println(  String(tempOLED));
          Serial.print("  Debug Flag: " );
          Serial.println(  String(tempDebug));
          Serial.print("  Water Bottom Detected: " );
          Serial.println( water_bottom_detected);
          Serial.print("  Water Top Detected: " );
          Serial.println( water_top_detected);
          Serial.print("  Sender CPU Temp: " );
          Serial.println( Sender_temp_farenheit);
          */
          lastSenderBatt = battStr;
          lastRssiStr = String(LoRa.getRSSI());
          runningTimer = 0; // Reset running timer on new packet
          lastSenderState = "Awake";
        }
     }

      if (millis() - lastPacketTime > SLEEP_TIMEOUT_MS && lastPacketTime != 0) {
        if (lastSenderState != "Asleep") {
          lastSenderState = "Asleep";
          Serial.println("Sender asleep");
        }
      }
    }
  }
}



// === SETUP ===
void setup() {
  esp_log_level_set("Preferences", ESP_LOG_NONE); 
  Serial.begin(115200); delay(1000);

  setupLoRa(); // Setup LoRa with interrupt for receiving packets
  loraQueue = xQueueCreate(10, sizeof(LoRaPacket));
xTaskCreatePinnedToCore(
    ProcessTask,
    "Process",
    4096,
    NULL,
    1,
    &task1Handle,
    1);      // Core 1

xTaskCreatePinnedToCore(
    LoRaTask,
    "LoRa",
    4096,
    NULL,
    2,
    &task0Handle,
    0);      // Core 0


  if (loraQueue == NULL)
  {
      Serial.println("Queue creation failed!");
      while (1);
  }

  Serial.printf("=== RECEIVER  %s  ===\n", receiver_version);
  printOTAInfo(); // Print the OTA partition information for debugging
  pinMode(RESET_PIN, INPUT_PULLUP); // Pin to trigger hard reset of WiFi settings
  pinMode(Display_On_Pin, OUTPUT); // Control power to the OLED display
  digitalWrite(Display_On_Pin, LOW);
  pinMode (Telegram_Debug_Mode_Pin, INPUT_PULLUP ); // Directs which Telegram Ttolken to use (Debug vs Production)
  if (digitalRead(Telegram_Debug_Mode_Pin) == LOW) {
      Serial.println("Telegram Debug Mode Activated");
      bot = UniversalTelegramBot(BOT_TOKEN_Debug, secured_client);
  }

  delay(100);                  // Wait for the display to power up

  Wire.begin(17, 18); Wire.setClock(100000);
  u8g2.begin();
  displayLookingForWiFi();

  WiFiManager wm; wm.setConfigPortalTimeout(180);
  if (!wm.autoConnect("WaterTempRec","password123")) {
    Serial.println("WiFi failed → portal");
  } else {
    Serial.print("IP: "); Serial.println(WiFi.localIP());
    
    secured_client.setInsecure();
    Serial.println("\nWiFi connected");
  //  sendEmailAlert("Receiver Started", "The water temperature receiver has started and connected to WiFi.");
    
}
  server.on("/", handleRoot);
  server.on("/romconfig", handleRomConfig);
  server.begin();

  // Initialize temperatures to a default value (e.g., -196.6 °F)
  // Setup ROM table, graph limits, POST URL, email config, operator vars, recipients, and sender credentials
  for (int i=0;i<14;i++) temperatures[i] = -196.6;
  loadRomTable();
  loadGraphLimits();
  loadPostUrl();  // load the POST URL
  loadEmailConfig(); // load email configuration
  loadOperatorVars(); 
  loadRecipients();  // load multiple recipients
  loadSenderCreds(); // Loaded sender credentials
  resetVariablesFlag = true; // Force sending operator vars on first send

  updateOLED();   // Update the OLED display with initial information

  topWaterAlertSent = false; // Reset alert flag on startup
  bottomWaterAlertSent = false; // Reset alert flag on startup
}

// === LOOP ===
void loop() {
  checkHardResetPin(); // Check reset pin for resetting the wifi address
  server.handleClient(); 

  //Serial.print("IP: "); Serial.println(WiFi.localIP());
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected, restarting...");
    ESP.restart(); 
  }

//-------------------------

  // Update OLED with latest info (including water detection status)(if enabled)
  if (newPacketReceived == true){
    newPacketReceived = false;
    updateOLED();
    postTemperaturesToJsonPlaceholder();   // Only POST at the end
    Serial.println("Posted temperatures to HTTP endpoint");      
  }

  // Handle email message on water detected
  if ((water_bottom_detected == "Y" ) && !bottomWaterAlertSent) {
    //Serial.println("Bottom Water detected - sending email alert");
    String alertDetails = "Water Detected at: Bottom Sensor";
    sendEmailAlert("Water Detected Alert", alertDetails);
    bottomWaterAlertSent = true;
  }else{
    if (water_bottom_detected == "N" ) {
      bottomWaterAlertSent = false; // Reset alert flag when water is no longer detected
    }
  }

  if ((water_top_detected == "Y" ) && !topWaterAlertSent) {
    //Serial.println("Top Water detected - sending email alert");
    String alertDetails = "Water Detected at: Top Sensor";
    sendEmailAlert("Water Detected Alert", alertDetails);
    topWaterAlertSent = true;
  }else{
    if (water_top_detected == "N" ) {
      topWaterAlertSent = false; // Reset alert flag when water is no longer detected
    }
  }

  //Handle Telegram messages
  checkTelegram();

  // Update running timer every 30 seconds to track how long it's been since the last packet was received
  if (millis() - minCounter > 30000) {
    minCounter = millis() ;
    runningTimer = (millis() - lastPacketTime)/60000;
  }

  delay(100);
}