#pragma once

// ---------------- LED Settings ----------------
#define NUM_LEDS 90

// ---------------- Animation Settings ----------------
#define ANIMATION_SWITCH_INTERVAL_MS 20000

// ---------------- Wi-Fi Credentials ----------------
#define WIFI_SSID "Goonnectivity Internet Solutions"
#define WIFI_PASSWORD "Tr0janH0rse"

// ---------------- OTA Settings ----------------
#define OTA_SERVER_URL "https://lighting-firmware-server.onrender.com"

// ---------------- FreeRTOS Settings ----------------
#define ANIMATION_TASK_STACK_SIZE 4096
#define MESH_TASK_STACK_SIZE 4096
#define ANIMATION_TASK_PRIORITY 1
#define MESH_TASK_PRIORITY 1
#define ANIMATION_TASK_CORE 1
#define MESH_TASK_CORE 0
