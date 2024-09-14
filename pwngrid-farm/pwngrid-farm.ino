/*
 * pwngrid-farm: a POC pwngrid bypass
 * Copyright (C) 2024 dj1ch
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE 
 * SOFTWARE.
*/

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
 * Most of the code is derived from the ESP32 and ESP8266 beacon spammer
 */

/**
 * @brief Settings
 */
const uint8_t channels[] = {1, 6, 11}; // used Wi-Fi channels (available: 1-14)
const bool wpa2 = true; // WPA2 networks
const bool appendSpaces = true; // makes all SSIDs 32 characters long to improve performance

/**
 * @brief For the random SSID generator
 */
const char letters_low[] = "abcdefghijklmnopqrstuvwxyz";
const char letters_up[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
const char numbers[] = "0123456789";

/**
 * @brief Run time variables
 */
char emptySSID[32];
uint8_t channelIndex = 0;
uint8_t macAddr[6];
uint8_t wifi_channel = 1;
uint32_t currentTime = 0;
uint32_t packetSize = 0;
uint32_t packetCounter = 0;
uint32_t attackTime = 0;
uint32_t packetRateTime = 0;

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
  currentTime = millis();

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

  if (currentTime - attackTime > 100) {
    attackTime = currentTime;

    // temp variables
    int i = 0;
    int j = 0;
    int ssidNum = 1;
    char tmp;
    int ssidsLen = strlen_P(ssid.c_str());
    bool sent = false;

    // go to next channel
    nextChannel();

    while (i < ssidsLen) {
      // read out next SSID
      j = 0;
      do {
        tmp = pgm_read_byte(ssid.c_str() + i + j);
        j++;
      } while (tmp != '\n' && j <= 32 && i + j < ssidsLen);

      uint8_t ssidLen = j - 1;

      // set MAC address
      macAddr[5] = ssidNum;
      ssidNum++;

      // write MAC address into beacon frame
      memcpy(&beaconPacket[10], macAddr, 6);
      memcpy(&beaconPacket[16], macAddr, 6);

      // reset SSID
      memcpy(&beaconPacket[38], emptySSID, 32);

      // write new SSID into beacon frame
      memcpy_P(&beaconPacket[38], &ssid[i], ssidLen);

      // set channel for beacon frame
      beaconPacket[82] = wifi_channel;

      // send packet
      if (appendSpaces) {
        for (int k = 0; k < 3; k++) {
          //Serial.printf("size: %d \n", packetSize);
          packetCounter += esp_wifi_80211_tx(WIFI_IF_STA, beaconPacket, packetSize, 0) == 0;
          delay(1);
        }
      }

      // remove spaces
      else {
        uint16_t tmpPacketSize = (109 - 32) + ssidLen; // calc size
        uint8_t* tmpPacket = new uint8_t[tmpPacketSize]; // create packet buffer
        memcpy(&tmpPacket[0], &beaconPacket[0], 37 + ssidLen); // copy first half of packet into buffer
        tmpPacket[37] = ssidLen; // update SSID length byte
        memcpy(&tmpPacket[38 + ssidLen], &beaconPacket[70], 39); // copy second half of packet into buffer

        // send packet
        for (int k = 0; k < 3; k++) {
          packetCounter += esp_wifi_80211_tx(WIFI_IF_STA, tmpPacket, tmpPacketSize, 0) == 0;
          delay(1);
        }

        delete tmpPacket; // free memory of allocated buffer
      }

      i += j;
    }
  }

  // show packet-rate each second
  if (currentTime - packetRateTime > 1000) {
    packetRateTime = currentTime;
    Serial.print("Packets/s: ");
    Serial.println(packetCounter);
    packetCounter = 0;
  }
  #elif defined(ESP8266)
  // send out SSIDs
  if (currentTime - attackTime > 100) {
    attackTime = currentTime;

    // temp variables
    int i = 0;
    int j = 0;
    int ssidNum = 1;
    char tmp;
    int ssidsLen = strlen_P(ssid.c_str());
    bool sent = false;

    // Go to next channel
    nextChannel();

    while (i < ssidsLen) {
      // Get the next SSID
      j = 0;
      do {
        tmp = pgm_read_byte(ssid.c_str() + i + j);
        j++;
      } while (tmp != '\n' && j <= 32 && i + j < ssidsLen);

      uint8_t ssidLen = j - 1;

      // set MAC address
      macAddr[5] = ssidNum;
      ssidNum++;

      // write MAC address into beacon frame
      memcpy(&beaconPacket[10], macAddr, 6);
      memcpy(&beaconPacket[16], macAddr, 6);

      // reset SSID
      memcpy(&beaconPacket[38], emptySSID, 32);

      // write new SSID into beacon frame
      memcpy_P(&beaconPacket[38], &ssid[i], ssidLen);

      // set channel for beacon frame
      beaconPacket[82] = wifi_channel;

      // send packet
      if (appendSpaces) {
        for (int k = 0; k < 3; k++) {
          packetCounter += wifi_send_pkt_freedom(beaconPacket, packetSize, 0) == 0;
          delay(1);
        }
      }

      // remove spaces
      else {

        uint16_t tmpPacketSize = (packetSize - 32) + ssidLen; // calc size
        uint8_t* tmpPacket = new uint8_t[tmpPacketSize]; // create packet buffer
        memcpy(&tmpPacket[0], &beaconPacket[0], 38 + ssidLen); // copy first half of packet into buffer
        tmpPacket[37] = ssidLen; // update SSID length byte
        memcpy(&tmpPacket[38 + ssidLen], &beaconPacket[70], wpa2 ? 39 : 13); // copy second half of packet into buffer

        // send packet
        for (int k = 0; k < 3; k++) {
          packetCounter += wifi_send_pkt_freedom(tmpPacket, tmpPacketSize, 0) == 0;
          delay(1);
        }

        delete tmpPacket; // free memory of allocated buffer
      }

      i += j;
    }
  }

  // show packet-rate each second
  if (currentTime - packetRateTime > 1000) {
    packetRateTime = currentTime;
    Serial.print("Packets/s: ");
    Serial.println(packetCounter);
    packetCounter = 0;
  }
  #endif
}


/**
 * @brief Switches to the next channel by incrementing it by one
 */
void nextChannel() {
    channelIndex = (channelIndex + 1) % (sizeof(channels) / sizeof(channels[0]));
    wifi_channel = channels[channelIndex];
    #ifdef ESP32
    esp_wifi_set_channel(wifi_channel, WIFI_SECOND_CHAN_NONE);
    #elif defined(ESP8266)
    wifi_set_channel(wifi_channel);
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