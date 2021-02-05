/* Copyright (c) Avnet Corporation. All rights reserved.
   Licensed under the MIT License. */

/*
This file implements routines requied to parse RSL10 advertisement messages received over a uart interface.
*/

// BW To Do List
// Architect and document IoTConnect implementation
// OTA updates for BLE PMOD
//      Other stuff?

// Document required production features
// 1. Configure devices
// 2. Configure IoTConnect to know about devices
// 3. Way to black/white lisl RSL10 devices in case there are multiple Azure Sphere devices that can see the RSL10 messages

#pragma once
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <applibs/log.h>
#include <applibs/networking.h>
#include "build_options.h"
#include "exit_codes.h"
#include "signal.h"

// Enable this define to send test messages to the parser from main.c line ~1190
//#define ENABLE_MESSAGE_TESTING

// Enable this define to see more debug around the message parsing
#define ENABLE_MSG_DEBUG

extern volatile sig_atomic_t exitCode;

// Define the Json string for reporting RSL10 telemetry data
static const char rsl10TelemetryJsonObject[] = "{\"temp%s\":%2.2f, \"humidity%s\":%2.2f, \"pressure%s\":%2.2f}";

// Initial device twin message with device details captured
static const char rsl10DeviceTwinsonObject[] = "{\"mac%s\":\"%s\",\"Version%s\":\"%s\"}";

//  33 00AB8967452301 8B380D0C61118C9BE95C9298092331F0 00 0E09 720F CC1799 FFFF -53

// Define the content of the message from the BT510.
typedef struct RSL10Message {
//    char msgSendRxId[3]; // BS1 or BR1
//    uint8_t msgColon[1];
    uint8_t len[1 * 2];
    uint8_t BdAddress[7 * 2];
    uint8_t uuid[16 * 2];
    uint8_t version[1 * 2];
    uint8_t temperature[2 * 2];
    uint8_t humidity[2 * 2];
    uint8_t pressure[3 * 2];
    uint8_t ambiantLight[2 * 2];
    uint8_t rssi[3 * 2];
} Rsl10Message_t;

#define MAX_RSL10_DEVICES 10
#define RSL10_ADDRESS_LEN 18

// RSL10 Global variables

// Array to hold specific data for each RSL10 detected by the system
typedef struct RSL10Device {
    char bdAddress[RSL10_ADDRESS_LEN];
    float lastTemperature;
    float lastHumidity;
    float lastPressure;
    float lastAmbiantLight;
    int lastRssi;
} RSL10Device_t;

extern void SendTelemetry(const char *);
extern void TwinReportState(const char *jsonState);
extern RSL10Device_t RSL10DeviceList[MAX_RSL10_DEVICES];
extern char authorizedDeviceList[MAX_RSL10_DEVICES][RSL10_ADDRESS_LEN];

// RSL10 Specific routines
int stringToInt(char *, size_t);
void textFromHexString(char *, char *, int);
void getBdAddress(char *, Rsl10Message_t *);
void getRxRssi(char *rxRssi, Rsl10Message_t *);
void getTemperature(float *temperature, Rsl10Message_t *);
void getHumidity(float *humidity, Rsl10Message_t *);
void getPressure(float *pressure, Rsl10Message_t *);
void getAmbiantLight(uint16_t *ambiantLight, Rsl10Message_t *);
int getRsl10DeviceIndex(char *);
int addRsl10DeviceToList(char *, Rsl10Message_t *);
void processData(int, int);
bool isDeviceAuthorized(char *);
void rsl10SendTelemetry(void);