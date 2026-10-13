#pragma once

// Copy this file to config.h (git-ignored) and fill in your details.
// Without config.h (or if the Wi-Fi join fails), the ESP32 starts its own
// access point instead; its page is then at http://192.168.4.1/

// Wi-Fi network the ESP32 joins (2.4 GHz).
#define WIFI_STA_SSID "your-wifi-name"
#define WIFI_STA_PASSWORD "your-wifi-password"

// mDNS name: the page is served at http://<DEVICE_HOSTNAME>.local
#define DEVICE_HOSTNAME "esp32-ac"

// Fallback access point name and password (password: 8+ characters).
#define AP_SSID "ESP32-AC-Remote"
#define AP_PASSWORD "choose-an-ap-password"

// Optional: request a fixed address instead of DHCP. A DHCP reservation on
// your router is usually better. Values are comma-separated octets.
// #define WIFI_STATIC_IP 192, 168, 0, 50
// #define WIFI_GATEWAY 192, 168, 0, 1
// #define WIFI_SUBNET 255, 255, 255, 0
// #define WIFI_DNS 192, 168, 0, 1
