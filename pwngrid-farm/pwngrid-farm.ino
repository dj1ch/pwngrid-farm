#include <Arduino.h>
#include <random>
#include <WiFi.h>

#ifdef ESP32
#include "esp_wifi.h"
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#else
#error Board not supported!
#endif

/**
 * @brief For the random SSID generator
 */
const char letters_low[] = "abcdefghijklmnopqrstuvwxyz";
const char letters_up[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
const char numbers[] = "0123456789";

void setup() {
  #ifdef ESP32
  wifi_init_config_t wifi_cfg = WIFI_INIT_CONFIG_DEFAULT();
  wifi_country_t ctry_cfg = {.cc="US", .schan = 1, .nchan = 13};
  ESP_ERROR_CHECK(esp_wifi_init(&wifi_cfg));
  ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
  ESP_ERROR_CHECK(esp_wifi_set_country(&ctry_cfg));
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
  ESP_ERROR_CHECK(esp_wifi_start());
  #elif defined(ESP8266)

  #endif
}

void loop() {
  String ssid = generate_ssid();
  String mac = "";
  uint8_t *macArr = generate_mac();

  for (int i = 0; i < 6; i++) {
    if (macArr[i] < 16) mac += "0";
    mac += String(macArr[i], HEX);
    if (i < 5) mac += ":";
  }
  mac.toUpperCase();

  Serial.println("Setting up Access Point...");
  Serial.println("SSID: " +  ssid);
  Serial.println("Mac Address: " +  mac);

  #ifndef ESP32
  esp_wifi_set_mac(WIFI_IF_AP, &newMACAddress[0]);
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid);
  #endif
}

/**
 * @brief Boilerplate code to return a random number
 */
int random(int min, int max) { return min + rand() % (max - min + 1); }

uint8_t random_unsigned(uint8_t min, uint8_t max) { return min + rand() % (max - min + 1); }

/**
 * @brief Returns a generated SSID based off of a random number generator
 */
String generate_ssid() {
  String ssid = "";
  int ssid_len = random(1, 33);

  for (int i = 0; i < ssid_len; i++) {
    int char_type = random(0, 3);

    if (char_type == 0) {
      ssid += letters_low[random(0, 26)];
    } else if (char_type == 1) {
      ssid += letters_up[random(0, 26)];
    } else {
      ssid += numbers[random(0, 10)];
    }
  }

  return ssid;
}

/**
 * @brief Returns a generated mac address based off of random number generator
 */
uint8_t* generate_mac() {
    static uint8_t mac[6];
    
    for (int i = 0; i < 6; i++) {
        mac[i] = random_unsigned(0x00, 0xFF);
    }
    
    return mac;
}