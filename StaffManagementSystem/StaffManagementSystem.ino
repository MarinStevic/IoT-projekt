/*
 * These are the connections for every module:
 * 
 * BME280
 * SDA --> GPIO21
 * SCL --> GPIO22
 * 
 * LCD module
 * DC  --> GPIO26
 * RST --> GPIO4
 * CS  --> GPIO5
 * SCK (CLK, SCL) --> GPIO18
 * MOSI(DIN, SDA) --> GPIO5
 * 
 * WS2812b
 * DIN --> GPIO15
 * 
 * RDM6300
 * TX --> GPIO27
 * 
 * Buzzer
 * +V --> GPIO25
 */

#include <ArduinoJson.h>
#include <rdm6300.h>
#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ST7735.h> // Hardware-specific library for ST7789
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <Adafruit_NeoPixel.h>
#include <HTTPClient.h>
#include <Tone32.h>

// WiFi settings
const char* ssid = "SSID";
const char* password = "PASSWORD";
// NeoPixel
Adafruit_NeoPixel pixels(1, 15, NEO_GRB + NEO_KHZ800);

// RDM6300
#define RDM6300_RX_PIN 27
Rdm6300 rdm6300;

// BME280
#define SEALEVELPRESSURE_HPA (1013.25)
Adafruit_BME280 bme;

// LCD module
#define TFT_DC    26
#define TFT_RST   4
#define TFT_CS    5
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

unsigned long int t = millis();
unsigned long int t1 = millis();
bool wifiStatus = false;
 
void setup(void) {
  // Init serial communication
  Serial.begin(115200);

  // Init NeoPixel and turn off lights
  pixels.begin();
  light(0, 0, 0);

  // Init WiFi connection
  WiFi.begin(ssid, password);
  Serial.println("Connecting to WiFi");
  while(WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    if (wifiStatus) {
      light(255, 255, 0);
      wifiStatus = !wifiStatus;
    }
    else {
      light(255, 125, 0);
      wifiStatus = !wifiStatus;
    }    
  }
  light(0, 0, 0);
  Serial.println("");
  Serial.print("Connected to WiFi network with IP Address: ");
  Serial.println(WiFi.localIP());

  // Init RDM6300
  rdm6300.begin(RDM6300_RX_PIN);

  // Init BME280
  bme.begin(0x76);

  // Init LCD module
  tft.initR(INITR_144GREENTAB);
  
  tft.setTextWrap(false);
  tft.setTextColor(ST77XX_BLACK);
  tft.setTextSize(2);
  tft.fillScreen(ST77XX_WHITE);

  // Draw temperature and humidity icons
  drawTemp();
  drawHumidity();
  tft.setCursor(32, 25);
  tft.fillRect(32, 25, 100, 14, ST77XX_WHITE);
  tft.print(bme.readTemperature());
  tft.println("C");
  tft.setCursor(32, 70);
  tft.fillRect(32, 70, 100, 14, ST77XX_WHITE);
  tft.print(bme.readHumidity());
  tft.println("%");
  t = millis();
}
 
void loop() {
  uint32_t tag;
  bool near = false;

  // If it has been 15 minutes since the device started working or last sent temperature and humidity readings send again
  if (millis() - t1 > 900000 || millis() - t1 < 0) {
    if(WiFi.status()== WL_CONNECTED){
      HTTPClient http;
      
      // Your Domain name with URL path or IP address with path
      http.begin("https://haize-365-faks-iot-api-geokqogmwq-ew.a.run.app/user/access/measurements");
      
      // If you need an HTTP request with a content type: application/json, use the following:
      http.addHeader("Content-Type", "application/json");
      int httpResponseCode = http.POST("{\"temperature\": " + String(bme.readTemperature()) + ", \"humidity\": " + String(bme.readHumidity()) + "}");
     
      Serial.print("HTTP Response code: ");
      Serial.println(httpResponseCode);
        
      // Free resources
      http.end();
      
      t1 = millis();
    }
    else {
      Serial.println("WiFi Disconnected");
    }
  }

  // Refresh screen data every 5 seconds
  if (millis() - t > 5000 || millis() - t < 0) {
    tft.setCursor(32, 25);
    tft.fillRect(32, 25, 100, 14, ST77XX_WHITE);
    tft.print(bme.readTemperature());
    tft.println("C");
    tft.setCursor(32, 70);
    tft.fillRect(32, 70, 100, 14, ST77XX_WHITE);
    tft.print(bme.readHumidity());
    tft.println("%");
    t = millis();
  }

  // Check if there is an RFID tag near the antenna
  if (rdm6300.update()) {
    tag = rdm6300.get_tag_id();
    Serial.println(tag);
    near = true;
  }

  // If tag is near send HTTP request with tag ID to check if user has access
  if (near) {
    light(0, 0, 100);
    tone(25, NOTE_DS8, 100, 0);
    noTone(25, 0);
    
    if(WiFi.status()== WL_CONNECTED){
      HTTPClient http;

      String serverPath = "https://haize-365-faks-iot-api-geokqogmwq-ew.a.run.app/user/access/rfidCreate/" + String(tag);
      
      // Your Domain name with URL path or IP address with path
      http.begin(serverPath.c_str());
      
      // Send HTTP GET request
      int httpResponseCode = http.GET();
      
      if (httpResponseCode > 0) {
        Serial.print("HTTP Response code: ");
        Serial.println(httpResponseCode);
        String payload = http.getString();
        Serial.println(payload);
        
        if (httpResponseCode == 200) {
          DynamicJsonDocument doc(1024);
          deserializeJson(doc, payload);
          String username = doc["user"]["display_name"];
          String type = doc["type"];
          light(0, 100, 0);
          tone(25, NOTE_C7, 500, 0); // access granted
          noTone(25, 0);
          tft.fillScreen(ST77XX_WHITE);
          tft.setCursor(4, 25);
          for (int i = 0; i < username.length(); i++) {
            if (username[i] != ' ')
              tft.print(username[i]);
            else
              tft.setCursor(4, 45);
          }
          tft.setCursor(4, 65);
          tft.println(type);
          tft.setCursor(4, 85);
          tft.println("Postujem!");
        }
        else {
          light(100, 0, 0);
          tone(25, NOTE_C4, 500, 0); // access denied
          noTone(25, 0);
        }
      }
      else {
        Serial.print("Error code: ");
        Serial.println(httpResponseCode);
      }
      // Free resources
      http.end();
    }
    else {
      Serial.println("WiFi Disconnected");
    }
    delay(2000);
    light(0, 0, 0);
    tft.fillScreen(ST77XX_WHITE);
    drawTemp();
    drawHumidity();
    tft.setCursor(32, 25);
    tft.fillRect(32, 25, 100, 14, ST77XX_WHITE);
    tft.print(bme.readTemperature());
    tft.println("C");
    tft.setCursor(32, 70);
    tft.fillRect(32, 70, 100, 14, ST77XX_WHITE);
    tft.print(bme.readHumidity());
    tft.println("%");
    t = millis();
  }

  rdm6300.update();

  delay(10);
}

// Function that sets the RGB LED to sent value
void light(int red, int green, int blue) {
  pixels.setPixelColor(0, pixels.Color(red, green, blue));
  pixels.show();
}

// Function to draw temperature icon
void drawTemp() {
  //0 - white, 1 - black, 2 - red
  int drawing[32][14] = {
    {0,0,0,0,0,1,1,1,1,0,0,0,0,0},
    {0,0,0,0,1,0,0,0,0,1,0,0,0,0},
    {0,0,0,1,0,0,0,0,0,0,1,0,0,0},
    {0,0,0,1,0,0,0,0,1,1,1,0,0,0},
    {0,0,0,1,0,0,0,0,0,0,1,0,0,0},
    {0,0,0,1,0,0,0,0,0,0,1,0,0,0},
    {0,0,0,1,0,0,0,0,1,1,1,0,0,0},
    {0,0,0,1,0,0,0,0,0,0,1,0,0,0},
    {0,0,0,1,0,0,0,0,0,0,1,0,0,0},
    {0,0,0,1,0,0,0,0,1,1,1,0,0,0},
    {0,0,0,1,2,2,2,2,2,2,1,0,0,0},
    {0,0,0,1,2,0,2,2,2,2,1,0,0,0},
    {0,0,0,1,2,0,2,2,1,1,1,0,0,0},
    {0,0,0,1,2,0,2,2,2,2,1,0,0,0},
    {0,0,0,1,2,0,2,2,2,2,1,0,0,0},
    {0,0,0,1,2,0,2,2,1,1,1,0,0,0},
    {0,0,0,1,2,0,2,2,2,2,1,0,0,0},
    {0,0,0,1,2,0,2,2,2,2,1,0,0,0},
    {0,0,0,1,2,0,2,2,1,1,1,0,0,0},
    {0,0,0,1,2,0,2,2,2,2,1,0,0,0},
    {0,0,1,2,0,0,2,2,2,2,2,1,0,0},
    {0,1,2,0,0,2,2,2,2,2,2,2,1,0},
    {1,2,0,0,2,2,2,2,2,2,2,2,2,1},
    {1,2,0,0,2,2,2,2,2,2,2,2,2,1},
    {1,2,0,0,2,2,2,2,2,2,2,2,2,1},
    {1,2,0,0,2,2,2,2,2,2,2,2,2,1},
    {1,2,0,0,2,2,2,2,2,2,2,2,2,1},
    {1,2,0,0,2,2,2,2,2,2,2,2,2,1},
    {1,2,2,0,0,2,2,2,2,2,2,2,2,1},
    {0,1,2,2,0,0,0,0,0,2,2,2,1,0},
    {0,0,1,1,2,2,2,2,2,2,1,1,0,0},
    {0,0,0,0,1,1,1,1,1,1,0,0,0,0}
  };
  for (int i=0; i < 32; i++) {
    for (int j=0; j < 14; j++) {
      if (drawing[i][j] == 0)
        tft.drawPixel(9 + j, 16 + i, ST77XX_WHITE);
      if (drawing[i][j] == 1)
        tft.drawPixel(9 + j, 16 + i, ST77XX_BLACK);
      if (drawing[i][j] == 2)
        tft.drawPixel(9 + j, 16 + i, ST77XX_RED);
    }
  }
}

// Function to draw humidity icon
void drawHumidity() {
  //0 - white, 1 - black, 2 - blue
  int drawing[26][17] = {
    {0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,1,2,1,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,1,2,1,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,1,2,2,2,1,0,0,0,0,0,0},
    {0,0,0,0,0,0,1,2,2,2,1,0,0,0,0,0,0},
    {0,0,0,0,0,1,2,2,2,2,2,1,0,0,0,0,0},
    {0,0,0,0,0,1,2,2,2,2,2,1,0,0,0,0,0},
    {0,0,0,0,1,2,2,2,2,2,2,2,1,0,0,0,0},
    {0,0,0,0,1,2,2,2,2,2,2,2,1,0,0,0,0},
    {0,0,0,1,2,2,2,2,2,2,2,2,2,1,0,0,0},
    {0,0,0,1,2,2,2,2,2,2,2,2,2,1,0,0,0},
    {0,0,1,2,2,2,2,2,2,2,2,2,2,2,1,0,0},
    {0,0,1,2,2,2,2,2,2,2,2,2,2,2,1,0,0},
    {0,1,2,2,0,2,2,2,2,2,2,2,2,2,2,1,0},
    {0,1,2,2,0,2,2,2,2,2,2,2,2,2,2,1,0},
    {1,2,2,0,0,2,2,2,2,2,2,2,2,2,2,2,1},
    {1,2,2,0,0,2,2,2,2,2,2,2,2,2,2,2,1},
    {1,2,2,0,0,2,2,2,2,2,2,2,2,2,2,2,1},
    {1,2,2,0,0,0,2,2,2,2,2,2,2,2,2,2,1},
    {0,1,2,2,0,0,2,2,2,2,2,2,2,2,2,1,0},
    {0,1,2,2,2,0,0,2,2,2,2,2,2,2,2,1,0},
    {0,0,1,2,2,2,0,0,2,2,2,2,2,2,1,0,0},
    {0,0,0,1,2,2,2,2,2,2,2,2,2,1,0,0,0},
    {0,0,0,0,1,1,2,2,2,2,2,1,1,0,0,0,0},
    {0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,0,0}
  };
  for (int i=0; i < 26; i++) {
    for (int j=0; j < 17; j++) {
      if (drawing[i][j] == 0)
        tft.drawPixel(8 + j, 64 + i, ST77XX_WHITE);
      if (drawing[i][j] == 1)
        tft.drawPixel(8 + j, 64 + i, ST77XX_BLACK);
      if (drawing[i][j] == 2)
        tft.drawPixel(8 + j, 64 + i, ST77XX_BLUE);
    }
  }
}
