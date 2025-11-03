#include <WiFi.h>
#include "wifi_bitmap.h"
#include "wifi_credentials.h"
#include <WebServer.h>
#include "HX711.h"
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <HTTPClient.h> // Add this at the top with your includes

// Pin Definitions for TFT (SPI)
#define TFT_CS 5
#define TFT_DC 17
#define TFT_RST 16
#define TFT_MOSI 23
#define TFT_SCK 18  

// Pin Definitions for HX711 (Weight sensor)
#define DOUT 32
#define CLK 33
#define WAKE_PIN 33

// Replace with your network credentials
const char *ssid = wifiSSID;
const char *password = wifiPassword;

HX711 scale;
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);

WebServer server(80);

float weight = 0;
bool wifiConnected = false;
float calibration_factor = -28892.07048;


float getWeight()
{
  if (scale.is_ready())
  {
    float w = scale.get_units(10); // average 10 readings
    Serial.print("Measured weight: ");
    Serial.print(w, 2);
    Serial.println(" kg");
    return w;
  }
  else
  {
    Serial.println("HX711 not ready, returning 0.0");
    return 0.0;
  }
}

void handleRoot()
{
  String response = String(weight, 2);
  server.send(200, "text/plain", response);
}

void drawWiFiIcon(bool connected)
{
  int x = 260;
  int y = 10;

  if (connected)
  {
    tft.drawBitmap(x, y, wifi_icon, 48, 48, ILI9341_BLACK, ILI9341_GREEN);
  }
  else
  {
    tft.drawBitmap(x, y, wifi_icon, 48, 48, ILI9341_BLACK, ILI9341_RED);
  }
}

void setupWiFi()
{
  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  int attempt = 0;

  while (WiFi.status() != WL_CONNECTED && attempt < 20)
  {
    delay(500);
    attempt++;
    drawWiFiIcon(false);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    wifiConnected = true;
    drawWiFiIcon(true);
    Serial.println("\nWiFi connected!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
  }
  else
  {
    wifiConnected = false;
    drawWiFiIcon(false);
    Serial.println("\nWiFi connection failed.");
  }

  if (wifiConnected)
  {
    server.on("/", handleRoot);
    server.begin();
    Serial.println("HTTP server started.");
  }
}

void displayWeight(float weight)
{
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(4);
  tft.setCursor(20, 30);
  tft.print("Weight:");

  tft.fillRect(10, 80, 300, 200, ILI9341_BLACK);

  tft.setTextColor(ILI9341_YELLOW);
  tft.setCursor(10, 80);
  tft.print(weight, 2);
  tft.print(" kg");

  drawWiFiIcon(wifiConnected);
}

void calculate_scale_factor(float known_weight)
{
  Serial.println("Calculating scale factor...");
  long reading = scale.get_value(10);
  Serial.print("Raw reading: ");
  Serial.println(reading);

  calibration_factor = reading / known_weight;
  Serial.print("New calibration factor: ");
  Serial.println(calibration_factor);

  scale.set_scale(calibration_factor);
  Serial.println("Scale calibrated.");
}

void postWeight(float weight) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin("http://localhost/Fitness/member/iot.php");
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");

    String postData = "weight=" + String(weight, 2);

    int httpResponseCode = http.POST(postData);

    Serial.print("POST Response code: ");
    Serial.println(httpResponseCode);

    http.end();
  } else {
    Serial.println("WiFi not connected. Cannot POST weight.");
  }
}

void handleSendWeight() {
  weight = getWeight();
  displayWeight(weight);
  postWeight(weight); // Send to PHP server
  server.send(200, "text/plain", "Weight sent: " + String(weight, 2));
}

void setup()
{
  Serial.begin(9600);
  delay(1000);
  Serial.println("Booting...");

  scale.begin(DOUT, CLK);
  scale.set_scale(calibration_factor);
  scale.tare();
  long avg = scale.read_average();
  Serial.print("Average reading after tare: ");
  Serial.println(avg);

  tft.begin();
  tft.setRotation(1);
  tft.invertDisplay(false);
  tft.fillScreen(ILI9341_BLACK);

  setupWiFi();

  esp_sleep_enable_ext0_wakeup((gpio_num_t)WAKE_PIN, 1);

  Serial.println("Entering sleep in 10 seconds...");
  delay(10000); // Keep awake for 10 seconds before sleep

  Serial.println("Waking weight measurement will follow...");
  weight = getWeight();
  displayWeight(weight);

  // Optional calibration step
  // calculate_scale_factor(4.3);

  //esp_deep_sleep_start(); // Enter deep sleep mode (DISABLED)
}

void loop()
{
  if (wifiConnected)
  {
    server.handleClient();
  }

  weight = getWeight();
  displayWeight(weight);

  postWeight(weight); // <-- Add this line to send the weight

  delay(1000);
}
