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

// Define a generic message structure.  We'll use this to extract the BdAddress and message ID
// to determine if and how to process the message
typedef struct RSL10MessageHeader {
    char msgSendRxId[3]; // ESD
    uint8_t BdAddress[7 * 2];
} RSL10MessageHeader_t;


// Define the content of the environmental message from the RSL10.
// ESD 00AB8967452301 00 CC09 4F12 B8069B FFFF -50
typedef struct RSL10EnvironmentalMessage {
    char msgSendRxId[3]; // ESD
    uint8_t BdAddress[7 * 2];
    uint8_t version[1 * 2];
    uint8_t temperature[2 * 2];
    uint8_t humidity[2 * 2];
    uint8_t pressure[3 * 2];
    uint8_t ambiantLight[2 * 2];
    char blankSpace[1];
    char rssi[3];
} Rsl10EnvironmentalMessage_t;

// Define the content of the environmental message from the RSL10.
// MSD 00AB8967452301 00 01 64 F9FF 1300 D9FF 00FC 5 9 5 B -49
typedef struct RSL10MotionMessage {
    char msgSendRxId[3]; // MSD
    uint8_t BdAddress[7 * 2];
    uint8_t version[1 * 2];
    uint8_t sampleIndex[1 * 2];
    uint8_t SensorSetting[1 * 2];
    uint8_t accel_raw_x[2 * 2];
    uint8_t accel_raw_y[2 * 2];
    uint8_t accel_raw_z[2 * 2];
    uint8_t orientation_x[1 * 2];
    uint8_t orientation_y[1 * 2];
    uint8_t orientation_z[1 * 2];
    uint8_t orientation_w[1 * 2];
    char blankSpace[1];
    char rssi[3];
} Rsl10MotionMessage_t;

// Define the content of the battery data message from the RSL10.
// BAT 00AB8967452301 0ABD -52
typedef struct RSL10BatteryMessage {
    char msgSendRxId[3]; // MSD
    uint8_t BdAddress[7 * 2];
    uint8_t battery[2 * 2];
    char blankSpace[1];
    char rssi[3];
} Rsl10BatteryMessage_t;

#define MAX_RSL10_DEVICES 10
#define RSL10_ADDRESS_LEN 18

// RSL10 Global variables

// Array to hold specific data for each RSL10 detected by the system
typedef struct RSL10Device {
    // Common data for all message types
    char bdAddress[RSL10_ADDRESS_LEN];
    int16_t lastRssi;
    
    // Environmental data
    float lastTemperature;
    float lastHumidity;
    float lastPressure;
    uint16_t lastAmbiantLight;
    bool environmentalDataRefreshed;
    
    
    // Movement data
    uint8_t lastSampleIndex;
    uint8_t lastsampleRate;
    uint8_t lastAccelRange;
    uint8_t lastDataType;

    float lastAccel_raw_x;
    float lastAccel_raw_y;
    float lastAccel_raw_z;
    float lastOrientation_x;
    float lastOrientation_y;
    float lastOrientation_z;
    float lastOrientation_w;
    bool movementDataRefreshed;

    // Battery data
    float lastBattery;
    bool batteryDataRefreshed;
} RSL10Device_t;

void SendTelemetry(const char *jsonMessage, bool);
extern void TwinReportState(const char *jsonState);
extern RSL10Device_t Rsl10DeviceList[MAX_RSL10_DEVICES];
extern char authorizedDeviceList[MAX_RSL10_DEVICES][RSL10_ADDRESS_LEN];

// RSL10 Specific routines
int stringToInt(char *, size_t);
void textFromHexString(char *, char *, int);
void getBdMessageID(char *, RSL10MessageHeader_t *);
void getBdAddress(char *, RSL10MessageHeader_t *);

void rsl10ProcessMovementMessage(char*, int8_t);
void rsl10ProcessEnvironmentalMessage(char*, int8_t);
void rsl10ProcessBatteryMessage(char*, int8_t);

void getRxRssi(int16_t *, char * );
void getTemperature(float *, Rsl10EnvironmentalMessage_t *);
void getHumidity(float *, Rsl10EnvironmentalMessage_t *);
void getPressure(float *, Rsl10EnvironmentalMessage_t *);
void getAmbiantLight(uint16_t *, Rsl10EnvironmentalMessage_t *);
void getBattery(float *, Rsl10BatteryMessage_t *);
void getSensorSettings(RSL10Device_t*, Rsl10MotionMessage_t*);
void getAccelReadings(RSL10Device_t*, Rsl10MotionMessage_t*);
void getOrientation(RSL10Device_t*, Rsl10MotionMessage_t*);

int8_t getRsl10DeviceIndex(char *);
int8_t  addRsl10DeviceToList(char *);
void processData(int, int);
bool isDeviceAuthorized(char *);
void rsl10SendTelemetry(void);