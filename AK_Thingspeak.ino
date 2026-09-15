// ════════════════════════════════════════════════════════════
//
//     █████╗ ██╗  ██╗██╗   ██╗ █████╗ ██████╗ ██╗██╗   ██╗███╗   ███╗
//    ██╔══██╗██║ ██╔╝██║   ██║██╔══██╗██╔══██╗██║██║   ██║████╗ ████║
//    ███████║█████╔╝ ██║   ██║███████║██████╔╝██║██║   ██║██╔████╔██║
//    ██╔══██║██╔═██╗ ██║   ██║██╔══██║██╔══██╗██║██║   ██║██║╚██╔╝██║
//    ██║  ██║██║  ██╗╚██████╔╝██║  ██║██║  ██║██║╚██████╔╝██║ ╚═╝ ██║
//    ╚═╝  ╚═╝╚═╝  ╚═╝ ╚═════╝ ╚═╝  ╚═╝╚═╝  ╚═╝╚═╝ ╚═════╝ ╚═╝     ╚═╝
//
//    Tank     : VERDANT
//    Channel  : AKuarium
//    Hardware : NodeMCU ESP8266 + DS18B20 + 0.96" OLED
//    Cloud    : ThingSpeak
//
// ════════════════════════════════════════════════════════════

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <time.h>

// ======================== CONFIG ========================

const char* WIFI_SSID      = "ACT_10DF";
const char* WIFI_PASSWORD  = "UmRhq3dZ";

const char* TS_API_KEY     = "089CV1O69W67BLVJ";

#define ONE_WIRE_BUS    D4
#define SCREEN_WIDTH    128
#define SCREEN_HEIGHT   64
#define OLED_RESET      -1
#define SCREEN_ADDR     0x3C

#define SENSOR_INTERVAL     5000
#define THINGSPEAK_INTERVAL 300000     // 5 minutes

#define TEMP_SAFE_LOW   24.0
#define TEMP_SAFE_HIGH  29.0

// IST = UTC + 5 hours 30 minutes = 19800 seconds
#define IST_OFFSET_SEC  19800

// Max characters to show for SSID on display
#define SSID_MAX_DISPLAY  8

// ======================== OBJECTS ========================

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

// ======================== BOOT LOGO ========================

const unsigned char epd_bitmap_ImageLogo [] PROGMEM = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0f, 0xf8, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x07, 0xfe, 0x00, 0x03, 0xff, 0x00, 0x00, 0x03, 0xff, 0xf0, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x0f, 0xfe, 0x00, 0x03, 0xff, 0x00, 0x00, 0x1f, 0xff, 0xe0, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x0f, 0xff, 0x00, 0x03, 0xff, 0x00, 0x00, 0x7f, 0xff, 0xc0, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x0f, 0xff, 0x00, 0x03, 0xff, 0x00, 0x01, 0xff, 0xff, 0xc0, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x1f, 0xff, 0x00, 0x03, 0xff, 0x00, 0x03, 0xff, 0xff, 0x80, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x1f, 0xff, 0x80, 0x03, 0xff, 0x00, 0x0f, 0xff, 0xff, 0x80, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x1f, 0xff, 0x80, 0x03, 0xff, 0x00, 0x1f, 0xff, 0xff, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x3f, 0xff, 0xc0, 0x03, 0xff, 0x00, 0x3f, 0xff, 0xff, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x3f, 0xff, 0xc0, 0x03, 0xff, 0x00, 0x7f, 0xff, 0xfe, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x7f, 0xff, 0xc0, 0x03, 0xff, 0x00, 0xff, 0xff, 0xfe, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x7f, 0xff, 0xe0, 0x03, 0xff, 0x01, 0xff, 0xbf, 0xfc, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x7f, 0xff, 0xe0, 0x03, 0xff, 0x03, 0xff, 0x7f, 0xfc, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xe0, 0x03, 0xff, 0x03, 0xfc, 0xff, 0xf8, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xf0, 0x03, 0xff, 0x07, 0xf9, 0xff, 0xf8, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0xff, 0xbf, 0xf0, 0x03, 0xff, 0x07, 0xf3, 0xff, 0xf0, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x01, 0xff, 0x9f, 0xf8, 0x03, 0xff, 0x0f, 0xe7, 0xff, 0xf0, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x01, 0xff, 0x9f, 0xf8, 0x03, 0xff, 0x0f, 0xcf, 0xff, 0xe0, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x03, 0xff, 0x1f, 0xf8, 0x03, 0xff, 0x0f, 0x9f, 0xff, 0xc0, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x03, 0xff, 0x0f, 0xfc, 0x03, 0xff, 0x1f, 0xbf, 0xff, 0xc0, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x03, 0xff, 0x0f, 0xfc, 0x03, 0xff, 0x1f, 0x3f, 0xff, 0x80, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x07, 0xfe, 0x07, 0xfe, 0x03, 0xff, 0x1e, 0x7f, 0xff, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x07, 0xfe, 0x07, 0xfe, 0x03, 0xff, 0x1e, 0xff, 0xfc, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x0f, 0xfe, 0x07, 0xfe, 0x03, 0xff, 0x3c, 0xff, 0xf8, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x0f, 0xfc, 0x03, 0xff, 0x03, 0xff, 0x3d, 0xff, 0xe0, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x0f, 0xfc, 0x03, 0xff, 0x03, 0xff, 0x39, 0xff, 0x80, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x1f, 0xf8, 0x03, 0xff, 0x83, 0xff, 0x3b, 0xf8, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x1f, 0xf8, 0x01, 0xff, 0x83, 0xff, 0x33, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x3f, 0xf8, 0x01, 0xff, 0x83, 0xff, 0x76, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x3f, 0xf0, 0x00, 0xff, 0xc3, 0xff, 0x60, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x3f, 0xf0, 0x00, 0xff, 0xc3, 0xff, 0x61, 0xfe, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x7f, 0xff, 0xf0, 0xff, 0xc3, 0xff, 0x63, 0xfe, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x7f, 0xff, 0xfe, 0x7f, 0xe3, 0xff, 0x63, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x7f, 0xff, 0xff, 0xff, 0xe3, 0xff, 0xc3, 0xff, 0x80, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xf3, 0xff, 0xc1, 0xff, 0xc0, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xf3, 0xff, 0xc1, 0xff, 0xe0, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x01, 0xff, 0xff, 0xff, 0xff, 0xf3, 0xff, 0xc0, 0xff, 0xe0, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x01, 0xff, 0xf7, 0xff, 0xff, 0xfb, 0xff, 0x80, 0x7f, 0xf0, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x01, 0xff, 0xc0, 0xff, 0xff, 0xfb, 0xff, 0x80, 0x7f, 0xf8, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x03, 0xff, 0x80, 0x3f, 0xff, 0xff, 0xff, 0x80, 0x3f, 0xf8, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x03, 0xff, 0x00, 0x06, 0x0f, 0xff, 0xff, 0x80, 0x1f, 0xfc, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x07, 0xff, 0x00, 0x00, 0x0f, 0xff, 0xff, 0x80, 0x0f, 0xfe, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x07, 0xfe, 0x00, 0x00, 0x0f, 0xff, 0xff, 0x00, 0x0f, 0xff, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x07, 0xfe, 0x00, 0x00, 0x07, 0xff, 0xff, 0x00, 0x07, 0xff, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x0f, 0xfe, 0x00, 0x00, 0x07, 0xff, 0xff, 0x00, 0x03, 0xff, 0x80, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x0f, 0xfc, 0x00, 0x00, 0x07, 0xff, 0xff, 0x00, 0x01, 0xff, 0xc0, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x0f, 0xfc, 0x00, 0x00, 0x03, 0xff, 0xff, 0x00, 0x01, 0xff, 0xe0, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x1f, 0xfc, 0x00, 0x00, 0x03, 0xff, 0xff, 0x00, 0x00, 0xff, 0xe0, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x1f, 0xf8, 0x00, 0x00, 0x01, 0xff, 0xff, 0x00, 0x00, 0x7f, 0xf0, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x3f, 0xf8, 0x00, 0x00, 0x01, 0xff, 0xff, 0x00, 0x00, 0x3f, 0xf8, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x3f, 0xf0, 0x00, 0x00, 0x00, 0xff, 0xff, 0x00, 0x00, 0x1f, 0xf8, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// Array of all bitmaps for convenience. (Total bytes used to store images in PROGMEM = 1040)
const int epd_bitmap_allArray_LEN = 1;
const unsigned char* epd_bitmap_allArray[1] = {
	epd_bitmap_ImageLogo
};

// ======================== VARIABLES ========================

float currentTemp        = 0.0;
float previousTemp       = 0.0;
float minTemp            = 99.0;
float maxTemp            = 0.0;
int   readingCount       = 0;
bool  sensorOK           = false;
bool  wifiConnected      = false;
bool  ntpSynced          = false;

bool  lastUploadSuccess  = false;
int   uploadCount        = 0;
int   uploadFailCount    = 0;
unsigned long lastUploadTime     = 0;

unsigned long lastSensorRead     = 0;
unsigned long lastThingSpeakSend = 0;
unsigned long uptimeStart        = 0;

// Truncated SSID for display
char displaySSID[SSID_MAX_DISPLAY + 3]; // +3 for "..\0"

// ======================== CUSTOM ICONS (8x8) ========================

const unsigned char icon_thermo[] PROGMEM = {
  0x04, 0x0A, 0x0A, 0x0A, 0x0A, 0x1B, 0x11, 0x0E
};

const unsigned char icon_wifi[] PROGMEM = {
  0x00, 0x3C, 0x42, 0x18, 0x24, 0x00, 0x08, 0x00
};

const unsigned char icon_wifi_off[] PROGMEM = {
  0x41, 0x22, 0x14, 0x08, 0x14, 0x22, 0x41, 0x00
};

const unsigned char icon_upload[] PROGMEM = {
  0x08, 0x1C, 0x3E, 0x08, 0x08, 0x08, 0x08, 0x00
};

const unsigned char icon_check[] PROGMEM = {
  0x00, 0x01, 0x02, 0x64, 0x38, 0x10, 0x00, 0x00
};

const unsigned char icon_cross[] PROGMEM = {
  0x00, 0x42, 0x24, 0x18, 0x18, 0x24, 0x42, 0x00
};

const unsigned char icon_warn[] PROGMEM = {
  0x08, 0x08, 0x1C, 0x1C, 0x3E, 0x3E, 0x7F, 0x7F
};

const unsigned char icon_fish[] PROGMEM = {
  0x00, 0x0C, 0x3E, 0x7F, 0x7F, 0x3E, 0x0C, 0x00
};

const unsigned char icon_cold[] PROGMEM = {
  0x08, 0x49, 0x2A, 0x1C, 0x1C, 0x2A, 0x49, 0x08
};

const unsigned char icon_hot[] PROGMEM = {
  0x24, 0x18, 0x7E, 0x3C, 0x3C, 0x7E, 0x18, 0x24
};

const unsigned char icon_heart[] PROGMEM = {
  0x00, 0x36, 0x7F, 0x7F, 0x3E, 0x1C, 0x08, 0x00
};

// ════════════════════════════════════════════════════════════
//  HELPER: TRUNCATE SSID
// ════════════════════════════════════════════════════════════

void buildDisplaySSID() {
  int len = strlen(WIFI_SSID);

  if (len <= SSID_MAX_DISPLAY) {
    // Fits — copy as is
    strcpy(displaySSID, WIFI_SSID);
  } else {
    // Too long — take first (SSID_MAX_DISPLAY - 2) chars + ".."
    strncpy(displaySSID, WIFI_SSID, SSID_MAX_DISPLAY - 2);
    displaySSID[SSID_MAX_DISPLAY - 2] = '.';
    displaySSID[SSID_MAX_DISPLAY - 1] = '.';
    displaySSID[SSID_MAX_DISPLAY] = '\0';
  }
}

// ════════════════════════════════════════════════════════════
//  HELPER: GET IST TIME
// ════════════════════════════════════════════════════════════

void setupNTP() {
  configTime(IST_OFFSET_SEC, 0, "pool.ntp.org", "time.nist.gov");
  Serial.print(F("NTP: Syncing"));

  // Wait up to 10 seconds for NTP sync
  int attempts = 0;
  time_t now = time(nullptr);
  while (now < 100000 && attempts < 20) {
    delay(500);
    Serial.print(".");
    now = time(nullptr);
    attempts++;
  }

  if (now > 100000) {
    ntpSynced = true;
    struct tm* timeinfo = localtime(&now);
    Serial.println();
    Serial.print(F("NTP: Synced! IST: "));
    Serial.print(timeinfo->tm_hour);
    Serial.print(F(":"));
    Serial.println(timeinfo->tm_min);
  } else {
    ntpSynced = false;
    Serial.println(F("\nNTP: Sync failed"));
  }
}

// Returns true if time is valid, fills hours and minutes
bool getIST(int &hours, int &minutes) {
  time_t now = time(nullptr);
  if (now < 100000) return false;

  struct tm* t = localtime(&now);
  hours = t->tm_hour;
  minutes = t->tm_min;
  return true;
}

// ════════════════════════════════════════════════════════════
//  SETUP
// ════════════════════════════════════════════════════════════

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println();
  Serial.println(F("╔═══════════════════════════════════╗"));
  Serial.println(F("║   AKuarium - Tank: VERDANT        ║"));
  Serial.println(F("║   Aqua Temperature Monitor v1.0   ║"));
  Serial.println(F("╚═══════════════════════════════════╝"));

  // Build truncated SSID for display
  buildDisplaySSID();
  Serial.print(F("Display SSID: "));
  Serial.println(displaySSID);

  // Initialize OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDR)) {
    Serial.println(F("OLED FAILED"));
    while (true);
  }

  // Boot sequence
  bootSequence();

  // Initialize sensor
  sensors.begin();
  sensors.setResolution(12);
  sensorOK = (sensors.getDeviceCount() > 0);

  Serial.print(F("Sensors found: "));
  Serial.println(sensors.getDeviceCount());

  if (!sensorOK) {
    showError("SENSOR ERROR", "Check DS18B20", "wiring on D4");
    delay(3000);
  }

  // Connect WiFi
  showWiFiConnecting();
  connectWiFi();

  // Sync NTP time (only if WiFi connected)
  if (wifiConnected) {
    showNTPSync();
    setupNTP();
  }

  // Show ready
  showReady();
  delay(1500);

  uptimeStart = millis();
}

// ════════════════════════════════════════════════════════════
//  MAIN LOOP
// ════════════════════════════════════════════════════════════

void loop() {
  unsigned long now = millis();

  // Read sensor
  if (now - lastSensorRead >= SENSOR_INTERVAL || lastSensorRead == 0) {
    lastSensorRead = now;
    readSensor();
  }

  // Send to ThingSpeak
  if (now - lastThingSpeakSend >= THINGSPEAK_INTERVAL || lastThingSpeakSend == 0) {
    lastThingSpeakSend = now;
    if (currentTemp > 0 && sensorOK) {
      sendToThingSpeak(currentTemp);
    }
  }

  // Update display
  drawMainScreen();
}

// ════════════════════════════════════════════════════════════
//  BOOT SEQUENCE
// ════════════════════════════════════════════════════════════

void bootSequence() {
  // Phase 1: AK Logo
  display.clearDisplay();
  display.drawBitmap(0, 0, epd_bitmap_ImageLogo, 128, 64, WHITE);
  display.display();
  delay(2500);

  // Phase 2: Channel + Tank branding
  display.clearDisplay();
  display.drawRect(0, 0, 128, 64, WHITE);
  display.drawRect(2, 2, 124, 60, WHITE);

  display.setTextSize(1);
  display.setTextColor(WHITE);

  display.setCursor(34, 8);
  display.print(F("AKuarium"));

  display.drawLine(20, 18, 108, 18, WHITE);

  display.setTextSize(2);
  display.setCursor(28, 24);
  display.print(F("VERDANT"));

  display.setTextSize(1);
  display.setCursor(20, 44);
  display.print(F("Aqua Temp Monitor"));

  display.setCursor(46, 54);
  display.print(F("v1.0"));

  display.display();
  delay(2500);

  // Phase 3: Progress bar
  showBootProgress("INIT SENSOR...", 20);
  delay(400);
  showBootProgress("INIT DISPLAY...", 40);
  delay(400);
  showBootProgress("LOADING CONFIG...", 60);
  delay(400);
  showBootProgress("STARTING WiFi...", 80);
  delay(400);
  showBootProgress("SYSTEM READY", 100);
  delay(600);
}

void showBootProgress(const char* text, int percent) {
  display.clearDisplay();
  display.drawRect(0, 0, 128, 64, WHITE);

  display.setTextSize(1);
  display.setTextColor(WHITE);

  display.setCursor(34, 6);
  display.print(F("VERDANT"));

  display.drawLine(10, 16, 118, 16, WHITE);

  display.setCursor(10, 24);
  display.print(text);

  display.drawRect(10, 38, 108, 12, WHITE);
  int fillWidth = map(percent, 0, 100, 0, 104);
  display.fillRect(12, 40, fillWidth, 8, WHITE);

  display.setCursor(52, 54);
  display.print(percent);
  display.print(F("%"));

  display.display();
}

// ════════════════════════════════════════════════════════════
//  WiFi + NTP SCREENS
// ════════════════════════════════════════════════════════════

void showWiFiConnecting() {
  display.clearDisplay();
  display.drawRect(0, 0, 128, 64, WHITE);

  display.setTextSize(1);
  display.setTextColor(WHITE);

  display.drawBitmap(60, 8, icon_wifi, 8, 8, WHITE);

  display.setCursor(18, 22);
  display.print(F("Connecting to:"));

  display.setCursor(30, 36);
  display.print(WIFI_SSID);

  display.display();
}

void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    delay(500);
    Serial.print(".");
    attempts++;

    display.fillRect(10, 48, 108, 14, BLACK);
    display.setCursor(20, 48);
    for (int d = 0; d < (attempts % 4) + 1; d++) {
      display.print(F(". "));
    }
    display.fillRect(10, 58, map(attempts, 0, 40, 0, 108), 4, WHITE);
    display.display();
  }

  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    Serial.println();
    Serial.print(F("Connected! IP: "));
    Serial.println(WiFi.localIP());

    display.clearDisplay();
    display.drawRect(0, 0, 128, 64, WHITE);
    display.drawBitmap(60, 6, icon_check, 8, 8, WHITE);

    display.setTextSize(1);
    display.setCursor(18, 18);
    display.print(F("WiFi CONNECTED!"));

    display.setCursor(10, 32);
    display.print(F("IP:"));
    display.setCursor(10, 42);
    display.print(WiFi.localIP());

    display.setCursor(10, 54);
    display.print(F("RSSI: "));
    display.print(WiFi.RSSI());
    display.print(F(" dBm"));

    display.display();
    delay(2000);

  } else {
    wifiConnected = false;
    Serial.println(F("\nWiFi FAILED"));

    display.clearDisplay();
    display.drawRect(0, 0, 128, 64, WHITE);
    display.drawBitmap(60, 8, icon_cross, 8, 8, WHITE);

    display.setTextSize(1);
    display.setCursor(20, 24);
    display.print(F("WiFi FAILED!"));
    display.setCursor(14, 40);
    display.print(F("Running offline"));

    display.display();
    delay(2000);
  }
}

void showNTPSync() {
  display.clearDisplay();
  display.drawRect(0, 0, 128, 64, WHITE);

  display.setTextSize(1);
  display.setTextColor(WHITE);

  display.setCursor(22, 14);
  display.print(F("Syncing Clock"));

  display.setCursor(18, 28);
  display.print(F("NTP + IST Offset"));

  display.setCursor(30, 46);
  display.print(F("Please wait"));

  display.display();
}

void showReady() {
  display.clearDisplay();
  display.drawRect(0, 0, 128, 64, WHITE);
  display.drawRect(2, 2, 124, 60, WHITE);

  display.drawBitmap(60, 8, icon_fish, 8, 8, WHITE);

  display.setTextSize(1);
  display.setTextColor(WHITE);

  display.setCursor(16, 22);
  display.print(F("VERDANT is ONLINE"));

  display.drawLine(16, 34, 112, 34, WHITE);

  display.setCursor(12, 40);
  display.print(F("Monitoring started"));

  display.setCursor(30, 52);
  display.print(F("- AKuarium -"));

  display.display();
}

// ════════════════════════════════════════════════════════════
//  SENSOR
// ════════════════════════════════════════════════════════════

void readSensor() {
  sensors.requestTemperatures();
  float temp = sensors.getTempCByIndex(0);

  if (temp == -127.0 || temp == 85.0 || temp < 5.0 || temp > 50.0) {
    for (int retry = 0; retry < 3; retry++) {
      delay(200);
      sensors.requestTemperatures();
      temp = sensors.getTempCByIndex(0);
      if (temp != -127.0 && temp != 85.0 && temp > 5.0 && temp < 50.0) {
        break;
      }
    }
    if (temp == -127.0 || temp == 85.0 || temp < 5.0 || temp > 50.0) {
      sensorOK = false;
      Serial.println(F("Sensor read FAILED"));
      return;
    }
  }

  sensorOK = true;
  previousTemp = currentTemp;
  currentTemp = temp;
  readingCount++;

  if (temp < minTemp) minTemp = temp;
  if (temp > maxTemp) maxTemp = temp;

  Serial.print(F("Temp: "));
  Serial.print(temp, 2);
  Serial.print(F(" C | Min: "));
  Serial.print(minTemp, 2);
  Serial.print(F(" | Max: "));
  Serial.print(maxTemp, 2);
  Serial.print(F(" | #"));
  Serial.println(readingCount);
}

// ════════════════════════════════════════════════════════════
//  THINGSPEAK
// ════════════════════════════════════════════════════════════

void sendToThingSpeak(float temperature) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println(F("WiFi lost. Reconnecting..."));
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
      delay(500);
      attempts++;
    }
    wifiConnected = (WiFi.status() == WL_CONNECTED);
    if (!wifiConnected) {
      lastUploadSuccess = false;
      uploadFailCount++;
      return;
    }
  }

  WiFiClient client;
  HTTPClient http;

  String url = "http://api.thingspeak.com/update?api_key=";
  url += TS_API_KEY;
  url += "&field1=";
  url += String(temperature, 2);

  Serial.print(F("ThingSpeak: Sending "));
  Serial.print(temperature, 2);
  Serial.print(F(" C ... "));

  http.begin(client, url);
  int httpCode = http.GET();

  if (httpCode == 200) {
    String response = http.getString();
    int entryNum = response.toInt();

    if (entryNum > 0) {
      lastUploadSuccess = true;
      uploadCount++;
      lastUploadTime = millis();
      Serial.print(F("OK! Entry #"));
      Serial.println(entryNum);
    } else {
      lastUploadSuccess = false;
      uploadFailCount++;
      Serial.println(F("RATE LIMITED"));
    }
  } else {
    lastUploadSuccess = false;
    uploadFailCount++;
    Serial.print(F("FAIL: "));
    Serial.println(httpCode);
  }

  http.end();
}

// ════════════════════════════════════════════════════════════
//  MAIN DISPLAY
// ════════════════════════════════════════════════════════════
//
//  PIXEL MAP (no overlaps):
//
//  Y=0  ┌─ TOP BAR: WiFi icon + SSID + Sensor icon + Time IST ─┐
//  Y=8  │                                                        │
//  Y=9  ├──────────────── divider line ──────────────────────────┤
//  Y=11 │                                                        │
//       │  BIG TEMPERATURE + trend                               │
//  Y=26 │                                                        │
//  Y=33 ├──────────────── divider line ──────────────────────────┤
//  Y=35 │  Lo:xx.x    Hi:xx.x    R:x.x                          │
//  Y=42 │                                                        │
//  Y=44 ├──────────────── divider line ──────────────────────────┤
//  Y=46 │  TS: OK 30s ago                                        │
//  Y=55 │  Sent:12 Err:0                        00:25            │
//  Y=62 └────────────────────────────────────────────────────────┘
//
// ════════════════════════════════════════════════════════════

void drawMainScreen() {
  display.clearDisplay();

  drawTopBar();
  display.drawLine(0, 9, 128, 9, WHITE);

  drawTemperature();
  display.drawLine(0, 33, 128, 33, WHITE);

  drawStats();
  display.drawLine(0, 44, 128, 44, WHITE);

  drawThingSpeakStatus();

  display.display();
}

// ════════════════════════════════════════════════════════════
//  TOP BAR
// ════════════════════════════════════════════════════════════
//
//  Layout:
//  [WiFi 8px][2px][SSID max 48px][gap][Thermo 8px][2px][HH:MM IST 54px]
//   0         10   10-57          58   60-67       70   74-127
//
// ════════════════════════════════════════════════════════════

void drawTopBar() {
  display.setTextSize(1);
  display.setTextColor(WHITE);

  // WiFi icon
  if (wifiConnected) {
    display.drawBitmap(0, 0, icon_wifi, 8, 8, WHITE);
  } else {
    display.drawBitmap(0, 0, icon_wifi_off, 8, 8, WHITE);
  }

  // Truncated SSID
  display.setCursor(10, 0);
  display.print(displaySSID);

  // Sensor status icon
  if (sensorOK) {
    display.drawBitmap(60, 0, icon_thermo, 8, 8, WHITE);
  } else {
    display.drawBitmap(60, 0, icon_warn, 8, 8, WHITE);
  }

  // Time in IST (24hr format)
  int hrs, mins;
  if (getIST(hrs, mins)) {
    display.setCursor(74, 0);
    if (hrs < 10) display.print(F("0"));
    display.print(hrs);
    display.print(F(":"));
    if (mins < 10) display.print(F("0"));
    display.print(mins);
    display.print(F(" IST"));
  } else {
    display.setCursor(74, 0);
    display.print(F("--:-- IST"));
  }
}

// ════════════════════════════════════════════════════════════
//  TEMPERATURE
// ════════════════════════════════════════════════════════════

void drawTemperature() {
  if (!sensorOK) {
    display.drawBitmap(4, 14, icon_warn, 8, 8, WHITE);
    display.setTextSize(1);
    display.setCursor(16, 12);
    display.print(F("SENSOR ERROR!"));
    display.setCursor(16, 24);
    display.print(F("Check DS18B20 wiring"));
    return;
  }

  // Status icon
  if (currentTemp < TEMP_SAFE_LOW) {
    display.drawBitmap(0, 14, icon_cold, 8, 8, WHITE);
  } else if (currentTemp > TEMP_SAFE_HIGH) {
    display.drawBitmap(0, 14, icon_hot, 8, 8, WHITE);
  } else {
    display.drawBitmap(0, 14, icon_heart, 8, 8, WHITE);
  }

  // Big temperature
  display.setTextSize(2);
  display.setCursor(12, 11);
  if (currentTemp < 10.0) {
    display.print(F(" "));
  }
  display.print(currentTemp, 1);

  // Degree symbol
  display.setTextSize(1);
  display.setCursor(72, 10);
  display.print(F("o"));

  // C
  display.setTextSize(2);
  display.setCursor(78, 11);
  display.print(F("C"));

  // Trend
  if (readingCount > 1) {
    float diff = currentTemp - previousTemp;
    display.setTextSize(1);

    if (diff > 0.05) {
      display.setCursor(100, 11);
      display.print(F("RISE"));
      display.setCursor(100, 21);
      display.print(F("+"));
      display.print(abs(diff), 1);
    } else if (diff < -0.05) {
      display.setCursor(100, 11);
      display.print(F("FALL"));
      display.setCursor(100, 21);
      display.print(F("-"));
      display.print(abs(diff), 1);
    } else {
      display.setCursor(100, 11);
      display.print(F("STDY"));
      display.setCursor(100, 21);
      display.print(F(" 0.0"));
    }
  }
}

// ════════════════════════════════════════════════════════════
//  STATS
// ════════════════════════════════════════════════════════════

void drawStats() {
  display.setTextSize(1);
  display.setTextColor(WHITE);

  // Min
  display.setCursor(0, 35);
  display.print(F("Lo:"));
  if (minTemp < 99.0) {
    display.print(minTemp, 1);
  } else {
    display.print(F("--.-"));
  }

  // Max
  display.setCursor(50, 35);
  display.print(F("Hi:"));
  if (maxTemp > 0.0) {
    display.print(maxTemp, 1);
  } else {
    display.print(F("--.-"));
  }

  // Range
  if (minTemp < 99.0 && maxTemp > 0.0) {
    float range = maxTemp - minTemp;
    display.setCursor(98, 35);
    display.print(F("R:"));
    display.print(range, 1);
  }
}

// ════════════════════════════════════════════════════════════
//  THINGSPEAK STATUS
// ════════════════════════════════════════════════════════════

void drawThingSpeakStatus() {
  display.setTextSize(1);
  display.setTextColor(WHITE);

  // Status icon
  if (lastUploadSuccess) {
    display.drawBitmap(0, 47, icon_check, 8, 8, WHITE);
  } else if (uploadCount == 0 && uploadFailCount == 0) {
    display.drawBitmap(0, 47, icon_upload, 8, 8, WHITE);
  } else {
    display.drawBitmap(0, 47, icon_cross, 8, 8, WHITE);
  }

  // Line 1: ThingSpeak status (y=46)
  display.setCursor(11, 46);

  if (uploadCount == 0 && uploadFailCount == 0) {
    display.print(F("TS: Waiting..."));
  } else if (lastUploadSuccess) {
    display.print(F("TS: OK"));
    if (lastUploadTime > 0) {
      unsigned long secAgo = (millis() - lastUploadTime) / 1000;
      display.print(F(" "));
      display.print(secAgo);
      display.print(F("s ago"));
    }
  } else {
    display.print(F("TS: FAILED"));
  }

  // Line 2: Counters + uptime (y=55)
  display.setCursor(0, 55);
  display.print(F("Sent:"));
  display.print(uploadCount);

  if (uploadFailCount > 0) {
    display.print(F(" Err:"));
    display.print(uploadFailCount);
  }

  // Uptime far right
  unsigned long upSec = (millis() - uptimeStart) / 1000;
  int hrs = upSec / 3600;
  int mins = (upSec % 3600) / 60;

  display.setCursor(96, 55);
  if (hrs < 10) display.print(F("0"));
  display.print(hrs);
  display.print(F(":"));
  if (mins < 10) display.print(F("0"));
  display.print(mins);
}

// ════════════════════════════════════════════════════════════
//  ERROR SCREEN
// ════════════════════════════════════════════════════════════

void showError(const char* line1, const char* line2, const char* line3) {
  display.clearDisplay();
  display.drawRect(0, 0, 128, 64, WHITE);

  display.drawBitmap(56, 4, icon_warn, 8, 8, WHITE);

  display.setTextSize(1);
  display.setTextColor(WHITE);

  display.setCursor(10, 18);
  display.print(line1);

  display.setCursor(10, 33);
  display.print(line2);

  display.setCursor(10, 48);
  display.print(line3);

  display.display();
}