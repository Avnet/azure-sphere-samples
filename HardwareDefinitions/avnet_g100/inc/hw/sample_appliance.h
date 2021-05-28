/* Copyright (c) Microsoft Corporation. All rights reserved.
   Licensed under the MIT License. */

// This file defines the mapping from the Avnet MT3620 Guardian 100 (G100)

// This file is autogenerated from ../../sample_appliance.json.  Do not edit it directly.

#pragma once
#include "avnet_aesms_mt3620.h"

// G100: LED 1.
#define LED_1 AVNET_AESMS_PIN11_GPIO8

// G100: LED 2.
#define LED_2 AVNET_AESMS_PIN12_GPIO9

// G100: LED 3.
#define LED_3 AVNET_AESMS_PIN13_GPIO10

// G100 USB Type B Connector.
#define EXTERNAL_UART AVNET_AESMS_ISU1_UART

// Dummy entry to satisfy cross project app_manifest.json definitions, do not use!
#define SAMPLE_BUTTON_1 AVNET_AESMS_PIN14_GPIO12

// Dummy entry to satisfy cross project app_manifest.json definitions, do not use!
#define SAMPLE_BUTTON_2 AVNET_AESMS_PIN15_GPIO13

// Dummy entry to satisfy cross project app_manifest.json definitions, do not use!
#define SAMPLE_APP_LED AVNET_AESMS_PIN19_GPIO27

// Dummy entry to satisfy cross project app_manifest.json definitions, do not use!
#define SAMPLE_LSM6DSO_I2C AVNET_AESMS_ISU2_I2C

