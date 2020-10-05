/* Copyright (c) Avnet Incorporated. All rights reserved.
   Licensed under the MIT License. */

// This JSON hardware definition file specifies peripheral pinouts
// of the Rev.2 Avnet MT3620 Starter Kit (SK2). The Rev.2 board can
// be fitted with an Ethernet Click, it also allows reconfiguration
// of Click2, Pmod and Grove connectors, by moving config resistors.
// See Hardware User Guide (pages 22-26) for more details.
// http://avnet.me/mt3620-kit-UG-V2 
//
// ISU1 maps to Click socket #2. It can be configured to support
// either a 4-wire UART (default) or an SPI interface.
// UART configuration (default) has R59 and R60 populated with
// zero ohm resistors (and R61 and R62 unpopulated).
// SPI configuration requires de-populating R59 and R60, then
// moving these config resistors to locations R61 and R62.
//
// ISU1 maps also to the PMOD connector (unpopulated). It's default
// configuration supports type 4A UART Pmod peripheral boards for
// BLE or wired UART expansion. Alternatively it can be configured
// to support type 2A SPI Pmod peripheral boards.
// UART configuration (default) has R39 and R50 populated with
// zero ohm resistors (and locations R41 and R52 unpopulated).
// SPI configuration requires de-populating R39 and R50, then
// moving these config resistors to locations R41 and R52.
//
// ISU2 maps to the Grove connector. It's default configuration
// supports I2C Grove boards. Alternatively it provides a way to
// access a third UART / connection with UART Grove boards
// I2C configuration has R57, R53 and R55 populated with zero ohm
// resistors (and locations R54 and R56 unpopulated).
// UART configuration requires de-populating R57, R53 and R55, then
// fitting zero ohm resistors to locations R54 and R56
//
// Note! If the Grove connector is reconfigured for UART mode (TX, RX)
// then ISU2 based I2C bus to onboard sensors, OLED display and
// Click sockets will no longer be available
//
// See Hardware User Guide (page 26) for more details
// http://avnet.me/mt3620-kit-UG-V2 
//

// This file is autogenerated from ../../avnet_mt3620_sk_rev2.json.  Do not edit it directly.

#pragma once
#include "avnet_aesms_mt3620_rev2.h"

// Application status LED uses GPIO4.
#define AVNET_MT3620_SK_APP_STATUS_LED_YELLOW AVNET_AESMS_PIN8_GPIO4

// GPIO drives WLAN status LED and is also exposed on CLICK1 (PWM)
#define AVNET_MT3620_SK_WLAN_STATUS_LED_YELLOW AVNET_AESMS_PIN5_GPIO0

// User LED Red channel uses GPIO8.
#define AVNET_MT3620_SK_USER_LED_RED AVNET_AESMS_PIN11_GPIO8

// User LED Green channel uses GPIO9.
#define AVNET_MT3620_SK_USER_LED_GREEN AVNET_AESMS_PIN12_GPIO9

// User LED Blue channel uses GPIO10.
#define AVNET_MT3620_SK_USER_LED_BLUE AVNET_AESMS_PIN13_GPIO10

// User BUTTON A uses GPIO12.
#define AVNET_MT3620_SK_USER_BUTTON_A AVNET_AESMS_PIN14_GPIO12

// User BUTTON B uses GPIO13.
#define AVNET_MT3620_SK_USER_BUTTON_B AVNET_AESMS_PIN15_GPIO13

// GPIO42 is exposed on CLICK1 (AN).
#define AVNET_MT3620_SK_GPIO42 AVNET_AESMS_PIN30_GPIO42

// GPIO43 is exposed on CLICK2 (AN).
#define AVNET_MT3620_SK_GPIO43 AVNET_AESMS_PIN31_GPIO43

// GPIO34 is exposed on CLICK2 (INT) and PMOD (Pin 1).
#define AVNET_MT3620_SK_GPIO34 AVNET_AESMS_PIN25_GPIO34

// GPIO35 is exposed on CLICK2 (RST) and PMOD (Pin 8).
#define AVNET_MT3620_SK_GPIO35 AVNET_AESMS_PIN26_GPIO35

// GPIO31 is exposed twice on CLICK2 (SCK) and (TX) and PMOD (Pin 2).
#define AVNET_MT3620_SK_GPIO31 AVNET_AESMS_PIN22_GPIO31

// GPIO33 is exposed twice on CLICK2 (SD0) and (RX) and PMOD (Pin 3).
#define AVNET_MT3620_SK_GPIO33 AVNET_AESMS_PIN24_GPIO33

// GPIO32 is exposed on CLICK2 (SDI) and PMOD (Pin 4).
#define AVNET_MT3620_SK_GPIO32 AVNET_AESMS_PIN23_GPIO32

// GPIO0 is exposed on CLICK1 (PWM) and drives the WLAN LED.
#define AVNET_MT3620_SK_GPIO0 AVNET_AESMS_PIN5_GPIO0

// GPIO1 is exposed on CLICK2 (PWM) and PMOD (Pin 9).
#define AVNET_MT3620_SK_GPIO1 AVNET_AESMS_PIN6_GPIO1

// GPIO2 is exposed on CLICK1 (RST).
#define AVNET_MT3620_SK_GPIO2 AVNET_AESMS_PIN7_GPIO2

// GPIO5 is exposed on CLICK1 (INT).
#define AVNET_MT3620_SK_GPIO5 AVNET_AESMS_PIN9_GPIO5

// GPIO28 is exposed twice on CLICK1 (SDI) and (RX).
#define AVNET_MT3620_SK_GPIO28 AVNET_AESMS_PIN20_GPIO28

// GPIO26 is exposed twice on CLICK1. (SCK) and (TX)
#define AVNET_MT3620_SK_GPIO26 AVNET_AESMS_PIN18_GPIO26

// GPIO37 is exposed on CLICK1 (SCL), CLICK2 (SCL), GROVE Connector (Pin 1), and OLED Display Connector (Pin 3).
#define AVNET_MT3620_SK_GPIO37 AVNET_AESMS_PIN27_GPIO37

// GPIO38 is exposed on CLICK1 (SDA), CLICK2 (SDA), GROVE Connector (Pin 2),and OLED Display Connector (Pin 4).
#define AVNET_MT3620_SK_GPIO38 AVNET_AESMS_PIN28_GPIO38

// GPIO39 is exposed on PMOD (Pin 7).
#define AVNET_MT3620_SK_GPIO39 AVNET_AESMS_PIN16_GPIO39

// GPIO29 is exposed on CLICK1 (CS)
#define AVNET_MT3620_SK_GPIO29 AVNET_AESMS_PIN21_GPIO29

// GPIO27 is exposed on CLICK1 (SDI)
#define AVNET_MT3620_SK_GPIO27 AVNET_AESMS_PIN19_GPIO27

// PWM CONTROLLER 0: channel 0 is exposed on CLICK1 (PWM), channel 1 is exposed on: CLICK2 (PWM) and PMOD (Pin 9), channel 2 is exposed on: CLICK1 (INT), CLICK2 (INT) and PMOD (Pin 7).  Pins for this controller are shared with AVNET_MT3620_SK_GPIO0, AVNET_MT3620_SK_GPIO1 and AVNET_MT3620_SK_GPIO2. If this PWM controller is requested, none of these GPIOs can be used.
#define AVNET_MT3620_SK_PWM_CONTROLLER0 AVNET_AESMS_PWM_CONTROLLER0

// PWM CONTROLLER 1: channel 0 is used by Application Status LED, and channel 1 is used by WLAN status LED. Pins for this controller are shared with AVNET_MT3620_SK_APP_STATUS_LED_YELLOW and AVNET_MT3620_SK_WLAN_STATUS_LED_YELLOW. If this PWM controller is requested, none of these GPIOs can be used.
#define AVNET_MT3620_SK_PWM_CONTROLLER1 AVNET_AESMS_PWM_CONTROLLER1

// PWM CONTROLLER 2: channel 0 is used by User LED Red, channel 1 is used by User LED Green, channel 2 is used by User LED Blue. Pins for this controller are shared with AVNET_MT3620_SK_USER_LED_RED, AVNET_MT3620_SK_USER_LED_GREEN, and AVNET_MT3620_SK_USER_LED_BLUE. If this PWM controller is requested, none of these GPIOs can be used.
#define AVNET_MT3620_SK_PWM_CONTROLLER2 AVNET_AESMS_PWM_CONTROLLER2

// ADC CONTROLLER 0: channel 0 is used by Ambient Light Sensor, channel 1 is exposed on CLICK1 (AN), channel 2 is exposed on CLICK2 (AN). Pins for this controller are shared with AVNET_MT3620_SK_GPIO42 and AVNET_MT3620_SK_GPIO43. If this ADC controller is requested, none of these GPIOs can be used.
#define AVNET_MT3620_SK_ADC_CONTROLLER0 AVNET_AESMS_ADC_CONTROLLER0

// ISU0 UART is exposed on CLICK1.
#define AVNET_MT3620_SK_ISU0_UART AVNET_AESMS_ISU0_UART

// ISU0 SPI is exposed on CLICK1).
#define AVNET_MT3620_SK_ISU0_SPI AVNET_AESMS_ISU0_SPI

// ISU1 UART is exposed on CLICK2 and PMOD.
#define AVNET_MT3620_SK_ISU1_UART AVNET_AESMS_ISU1_UART

// ISU1 SPI is exposed on CLICK2 when R59, R60, R61 and R62 are modified.  See HW User Guide.
#define AVNET_MT3620_SK_ISU1_SPI_CONFIGURABLE_CLICK2 AVNET_AESMS_ISU1_SPI

// ISU1 SPI is exposed on (PMOD) when R39, R50, R41 and R52 are modified.  See HW User Guide.
#define AVNET_MT3620_SK_ISU1_SPI_CONFIGURABLE_PMOD AVNET_AESMS_ISU1_SPI

// ISU1 I2C is exposed on CLICK2 pin SDO (SLC) and pin SDI (SDA).
#define AVNET_MT3620_SK_ISU1_I2C AVNET_AESMS_ISU1_I2C

// ISU2 UART is also exposed on the Grove connector when Resistor locations R53, R54, R55, R56 and R57 are modified. See HW User Guide.
#define AVNET_MT3620_SK_ISU2_UART_CONFIGURABLE_GROVE AVNET_AESMS_ISU2_UART

// ISU2 I2C is shared between on-device sensors, GROVE Connector, OLED DISPLAY Connector, CLICK1, and CLICK2.
#define AVNET_MT3620_SK_ISU2_I2C AVNET_AESMS_ISU2_I2C

