#include <Arduino.h>
#include <random>

#ifdef ESP32
#include "esp_wifi.h"
#include <WiFi.h>
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

/**
 * @brief Beacon frame packet(s) from https://github.com/spacehuhn/esp8266_beaconSpam/blob/master/esp8266_beaconSpam/esp8266_beaconSpam.ino#L96C1-L146C3
 */
// beacon frame definition
uint8_t beaconPacket[109] = {
  /*  0 - 3  */ 0x80, 0x00, 0x00, 0x00,             // Type/Subtype: managment beacon frame
  /*  4 - 9  */ 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // Destination: broadcast
  /* 10 - 15 */ 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, // Source
  /* 16 - 21 */ 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, // Source

  // Fixed parameters
  /* 22 - 23 */ 0x00, 0x00,                         // Fragment & sequence number (will be done by the SDK)
  /* 24 - 31 */ 0x83, 0x51, 0xf7, 0x8f, 0x0f, 0x00, 0x00, 0x00, // Timestamp
  /* 32 - 33 */ 0xe8, 0x03,                         // Interval: 0x64, 0x00 => every 100ms - 0xe8, 0x03 => every 1s
  /* 34 - 35 */ 0x31, 0x00,                         // capabilities Tnformation

  // Tagged parameters

  // SSID parameters
  /* 36 - 37 */ 0x00, 0x20,                         // Tag: Set SSID length, Tag length: 32
  /* 38 - 69 */ 0x20, 0x20, 0x20, 0x20,
  0x20, 0x20, 0x20, 0x20,
  0x20, 0x20, 0x20, 0x20,
  0x20, 0x20, 0x20, 0x20,
  0x20, 0x20, 0x20, 0x20,
  0x20, 0x20, 0x20, 0x20,
  0x20, 0x20, 0x20, 0x20,
  0x20, 0x20, 0x20, 0x20,                           // SSID

  // Supported Rates
  /* 70 - 71 */ 0x01, 0x08,                         // Tag: Supported Rates, Tag length: 8
  /* 72 */ 0x82,                    // 1(B)
  /* 73 */ 0x84,                    // 2(B)
  /* 74 */ 0x8b,                    // 5.5(B)
  /* 75 */ 0x96,                    // 11(B)
  /* 76 */ 0x24,                    // 18
  /* 77 */ 0x30,                    // 24
  /* 78 */ 0x48,                    // 36
  /* 79 */ 0x6c,                    // 54

  // Current Channel
  /* 80 - 81 */ 0x03, 0x01,         // Channel set, length
  /* 82 */      0x01,               // Current Channel

  // RSN information
  /*  83 -  84 */ 0x30, 0x18,
  /*  85 -  86 */ 0x01, 0x00,
  /*  87 -  90 */ 0x00, 0x0f, 0xac, 0x02,
  /*  91 -  92 */ 0x02, 0x00,
  /*  93 - 100 */ 0x00, 0x0f, 0xac, 0x04, 0x00, 0x0f, 0xac, 0x04, /*Fix: changed 0x02(TKIP) to 0x04(CCMP) is default. WPA2 with TKIP not supported by many devices*/
  /* 101 - 102 */ 0x01, 0x00,
  /* 103 - 106 */ 0x00, 0x0f, 0xac, 0x02,
  /* 107 - 108 */ 0x00, 0x00
};

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
  WiFi.mode(WIFI_OFF);
  wifi_set_opmode(SOFTAP_MODE);
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

  #ifdef ESP32
  esp_wifi_set_mac(WIFI_IF_AP, &macArr[0]);
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid);
  #elif defined(ESP8266)
  wifi_set_macaddr(0, const_cast<uint8*>(macArr));
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