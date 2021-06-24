﻿/* Copyright (c) Microsoft Corporation. All rights reserved.
   Licensed under the MIT License. */

// Please see the readme.md for details on how to confugure, build and run this application.

#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// applibs_versions.h defines the API struct versions to use for applibs APIs.
#include "applibs_versions.h"
#include <applibs/log.h>
#include <applibs/networking.h>
#include <applibs/gpio.h>
#include <applibs/storage.h>
#include <applibs/eventloop.h>
#include <applibs/uart.h>

// The following #include imports a "sample appliance" definition. This app comes with multiple
// implementations of the sample appliance, each in a separate directory, which allow the code to
// run on different hardware.
//
// By default, this app targets hardware that follows the Avnet Azure Sphere Starter Kit Rev1
// specification
//
// To target different hardware, you'll need to update CMakeLists.txt. For example, to target the
// Avnet MT3620 Starter Kit Rev2, change the TARGET_DIRECTORY argument in the call to
// azsphere_target_hardware_definition to "HardwareDefinitions/avnet_mt3620_sk_rev2".
//
// See https://aka.ms/AzureSphereHardwareDefinitions for more details.
#include <hw/sample_appliance.h>

#include "eventloop_timer_utilities.h"
#include "parson.h" // Used to parse Device Twin messages.
#include "rsl10.h"
#include "build_options.h"
#include "router.h"
#include "iotConnect.h"

// Azure IoT SDK
#include <iothub_client_core_common.h>
#include <iothub_device_client_ll.h>
#include <iothub_client_options.h>
#include <iothubtransportmqtt.h>
#include <iothub.h>
#include <azure_sphere_provisioning.h>
#include <iothub_security_factory.h>
#include "exit_codes.h"
#include "deviceTwin.h"
#include "signal.h"
#include "i2c.h"
#include "oled.h"

volatile sig_atomic_t exitCode = ExitCode_Success;

/// <summary>
/// Connection types to use when connecting to the Azure IoT Hub.
/// </summary>
typedef enum {
    ConnectionType_NotDefined = 0,
    ConnectionType_DPS = 1,
    ConnectionType_Direct = 2
} ConnectionType;

/// <summary>
/// Authentication state of the client with respect to the Azure IoT Hub.
/// </summary>
typedef enum {
    /// <summary>Client is not authenticated by the Azure IoT Hub.</summary>
    IoTHubClientAuthenticationState_NotAuthenticated = 0,
    /// <summary>Client has initiated authentication to the Azure IoT Hub.</summary>
    IoTHubClientAuthenticationState_AuthenticationInitiated = 1,
    /// <summary>Client is authenticated by the Azure IoT Hub.</summary>
    IoTHubClientAuthenticationState_Authenticated = 2
} IoTHubClientAuthenticationState;

// Azure IoT definitions.
static char *scopeId = NULL;                                      // ScopeId for DPS.
static char *hubHostName = NULL;                                  // Azure IoT Hub Hostname.
static char *deviceId = NULL;                                     // Device ID must be in lowercase.
static ConnectionType connectionType = ConnectionType_NotDefined; // Type of connection to use.
static IoTHubClientAuthenticationState iotHubClientAuthenticationState =
    IoTHubClientAuthenticationState_NotAuthenticated; // Authentication state with respect to the
                                                      // IoT Hub.

IOTHUB_DEVICE_CLIENT_LL_HANDLE iothubClientHandle = NULL;
static const int deviceIdForDaaCertUsage = 1; // A constant used to direct the IoT SDK to use
                                              // the DAA cert under the hood.

#ifdef USE_ETH_0
static const char NetworkInterface[] = "eth0";
#else
static const char NetworkInterface[] = "wlan0";
#endif 

// Function declarations
void SendEventCallback(IOTHUB_CLIENT_CONFIRMATION_RESULT result, void *context);
void TwinReportState(const char *jsonState);
void ReportedStateCallback(int result, void *context);
static void UartEventHandler(EventLoop *el, int fd, EventLoop_IoEvents events, void *context);
//static void SendUartMessage(int uartFd, const char *dataToSend);
static const char *GetReasonString(IOTHUB_CLIENT_CONNECTION_STATUS_REASON reason);
static const char *GetAzureSphereProvisioningResultString(
    AZURE_SPHERE_PROV_RETURN_VALUE provisioningResult);
static void SetUpAzureIoTHubClient(void);
void SendTelemetry(const char *jsonMessage, bool);
static void AzureTimerEventHandler(EventLoopTimer *timer);
static void SendTelemetryTimerEventHandle(EventLoopTimer *timer);
static ExitCode ValidateUserConfiguration(void);
static void ParseCommandLineArguments(int argc, char *argv[]);
static bool SetUpAzureIoTHubClientWithDaa(void);
static bool SetUpAzureIoTHubClientWithDps(void);
bool IsConnectionReadyToSendTelemetry(void);
static bool ButtonStateChanged(int fd, GPIO_Value_Type *oldState);

#ifdef OLED_SD1306
static void UpdateOledEventHandler(EventLoopTimer *timer);
#endif

// Initialization/Cleanup
static ExitCode InitPeripheralsAndHandlers(void);
void CloseFdAndPrintError(int fd, const char *fdName);
static void ClosePeripheralsAndHandlers(void);

// BT510 Specific routines
extern void parseRsl10Message(char *);
extern RSL10Device_t Rsl10DeviceList[MAX_RSL10_DEVICES];

#ifdef RSL10_SALES_DEMO
void addWifiNetwork(char* , int, char*, int);
bool networkExists(char* , int);
#ifdef RSL10_SALES_DEMO
EventLoopTimer *resetTelemetryTimeTimer = NULL;
#endif 
#endif // RSL10_SALES_DEMO

// File descriptors - initialized to invalid value

#ifdef TARGET_QIIO_200
char *cellinfo = NULL;
#endif 

// Buttons
static int buttonAgpioFd = -1;
static int buttonBgpioFd = -1;

// State variables
static GPIO_Value_Type buttonAState = GPIO_Value_High;
static GPIO_Value_Type buttonBState = GPIO_Value_High;

// UART
static int uartFd = -1;

// BLE PMOD signals
static int nRfnResetFd = -1;
static int nRfnAutorunFd = -1;

#define RGB_NUM_LEDS 3
//  Guardian LEDs
//  Guardian has 3 independent LEDs mapped to the following MT3620 Module I/Os
//  LED_1 (Silkscreen Label 1) - AVNET_AESMS_PIN11_GPIO8 on GPIO8
//  LED_2 (Silkscreen Label 2)- AVNET_AESMS_PIN12_GPIO9 on GPIO9
//  LED_3 (Silkscreen Label 3)- AVNET_AESMS_PIN13_GPIO10 on GPIO10
static int gpioConnectionStateLedFds[RGB_NUM_LEDS] = {-1, -1, -1};
static GPIO_Id gpioConnectionStateLeds[RGB_NUM_LEDS] = {SAMPLE_RGBLED_RED, SAMPLE_RGBLED_GREEN,
                                                        SAMPLE_RGBLED_BLUE};

// Timer / polling
EventLoop *eventLoop = NULL;
static EventRegistration *uartEventReg = NULL;
EventLoopTimer *sendTelemetryTimer = NULL;
static EventLoopTimer *azureTimer = NULL;
static EventLoopTimer *buttonPollTimer = NULL;
#ifdef OLED_SD1306
static EventLoopTimer *oledUpdateTimer = NULL;
#endif 

// Azure IoT poll periods
static const int AzureIoTDefaultPollPeriodSeconds = 1;        // poll azure iot every second
static const int AzureIoTMinReconnectPeriodSeconds = 60;      // back off when reconnecting
static const int AzureIoTMaxReconnectPeriodSeconds = 10 * 60; // back off limit

static int azureIoTPollPeriodSeconds = -1;

// Usage text for command line arguments in application manifest.
static const char *cmdLineArgsUsageText =
    "DPS connection type: \" CmdArgs \": [\"--ConnectionType\", \"DPS\", \"--ScopeID\", "
    "\"<scope_id>\"]\n"
    "Direction connection type: \" CmdArgs \": [\"--ConnectionType\", \"Direct\", "
    "\"--Hostname\", \"<azureiothub_hostname>\", \"--DeviceID\", \"<device_id>\"]\n";

#define RGB_LED1_INDEX 0
#define RGB_LED2_INDEX 1
#define RGB_LED3_INDEX 2

// Define which LED to light up for each case
typedef enum {
    RGB_No_Connections = 0b000,
    RGB_No_Network = 0b001,        // No WiFi connection
    RGB_Network_Connected = 0b010, // Connected to Azure, not IoT Hub
    RGB_IoT_Hub_Connected = 0b100, // Connected to IoT Hub
} RGB_Status;

// Using the bits set in networkStatus, turn on/off the status LEDs
void setConnectionStatusLed(RGB_Status networkStatus)
{
    GPIO_SetValue(gpioConnectionStateLedFds[RGB_LED1_INDEX],
                  (networkStatus & (1 << RGB_LED1_INDEX)) ? GPIO_Value_Low : GPIO_Value_High);
    GPIO_SetValue(gpioConnectionStateLedFds[RGB_LED2_INDEX],
                  (networkStatus & (1 << RGB_LED2_INDEX)) ? GPIO_Value_Low : GPIO_Value_High);
    GPIO_SetValue(gpioConnectionStateLedFds[RGB_LED3_INDEX],
                  (networkStatus & (1 << RGB_LED3_INDEX)) ? GPIO_Value_Low : GPIO_Value_High);
}

// Determine the network status and call the routine to set the status LEDs
void updateConnectionStatusLed(void)
{
    RGB_Status networkStatus;
    bool bIsNetworkReady = false;

    if (Networking_IsNetworkingReady(&bIsNetworkReady) < 0) {
        networkStatus = RGB_No_Connections; // network error
    } else {
        networkStatus = !bIsNetworkReady ? RGB_No_Network // no Network, No WiFi
                : (iotHubClientAuthenticationState == IoTHubClientAuthenticationState_Authenticated)
                ? RGB_IoT_Hub_Connected   // IoT hub connected
                : RGB_Network_Connected; // only Network connected
    }

    // Set the LEDs based on the current status
    setConnectionStatusLed(networkStatus);
}

/// <summary>
///     Signal handler for termination requests. This handler must be async-signal-safe.
/// </summary>
static void TerminationHandler(int signalNumber)
{
    // Don't use Log_Debug here, as it is not guaranteed to be async-signal-safe.
    exitCode = ExitCode_TermHandler_SigTerm;
}

/// <summary>
///     Main entry point for this sample.
/// </summary>
int main(int argc, char *argv[])
{
    Log_Debug("Azure IoT Application starting.\n");

#ifdef USE_ETH_0
    // Configure eth0 for the Qiio device
    int err = Networking_SetInterfaceState("eth0", true);
    if (err < 0) {
        Log_Debug("Error setting interface state %d", errno);
        return -1;
    }
#endif 

#ifdef TARGET_QIIO_200
        
    if (!router_get_cellinfo(&cellinfo, NULL) && cellinfo) {
        Log_Debug("cellinfo : %s\n", cellinfo);
    } else {
        Log_Debug("Unable to read cellinfo\n");
    }
    Log_Debug("Network setup successfully\n");
#endif 

#ifdef RSL10_SALES_DEMO

	// Make sure that the expected wifi network is configured on this device
	if (!networkExists("IOTDEMO", 7)) {
		Log_Debug("Add network IOTDEMO\n");
		addWifiNetwork("IOTDEMO", 7, "iotDemo1", 8);
	}
	
	// Make sure that the expected wifi network is configured on this device
	if (!networkExists("IOTDEMO1", 8)) {
		Log_Debug("Add network IOTDEMO1\n");
		addWifiNetwork("IOTDEMO1", 8, "iotDemo1", 8);
	}

#endif 

    bool isNetworkingReady = false;
    if ((Networking_IsNetworkingReady(&isNetworkingReady) == -1) || !isNetworkingReady) {
        Log_Debug("WARNING: Network is not ready. Device cannot connect until network is ready.\n");
    }
    
    ParseCommandLineArguments(argc, argv);

    exitCode = ValidateUserConfiguration();
    if (exitCode != ExitCode_Success) {
        return exitCode;
    }

    exitCode = InitPeripheralsAndHandlers();

    // Main loop
    while (exitCode == ExitCode_Success) {
        EventLoop_Run_Result result = EventLoop_Run(eventLoop, -1, true);
        // Continue if interrupted by signal, e.g. due to breakpoint being set.
        if (result == EventLoop_Run_Failed && errno != EINTR) {
            exitCode = ExitCode_Main_EventLoopFail;
        }
    }

    ClosePeripheralsAndHandlers();

    Log_Debug("Application exiting.\n");

    return exitCode;
}

#ifdef RSL10_SALES_DEMO
/// <summary>
///     Reset the telemetry timer period back to the default time
/// </summary>
static void ResetTelemetryTimerHandler(EventLoopTimer *timer)
{
    if (ConsumeEventLoopTimerEvent(timer) != 0) {
        exitCode = ExitCode_AzureTimer_Consume;
        return;
    }

    // Update the send telemetry timer to send data every second
	struct timespec newSendTelemeteryPeriod = { .tv_sec = DEFAULT_TELEMETRY_TX_TIME,.tv_nsec = 0 };
    SetEventLoopTimerPeriod(sendTelemetryTimer, &newSendTelemeteryPeriod);

}
#endif 

/// <summary>
///     Button timer event:  Check the status of the button
/// </summary>
static void ButtonPollTimerEventHandler(EventLoopTimer *timer)
{
#ifdef RSL10_SALES_DEMO
    bool buttonPressed = false;
#endif
    if (ConsumeEventLoopTimerEvent(timer) != 0) {
        exitCode = ExitCode_ButtonTimer_Consume;
        return;
    }

    if (ButtonStateChanged(buttonAgpioFd, &buttonAState)) {
    
   		if (buttonAState == GPIO_Value_Low) {
    		Log_Debug("Button A pressed!\n");

#ifdef RSL10_SALES_DEMO
            buttonPressed = true;
#endif 

#ifdef OLED_SD1306
            // Use buttonB presses to drive OLED to display the last screen
	    	oled_state--;
        
		    if (oled_state < 0 )
		    {
    			oled_state = OLED_NUM_SCREEN;
	    	}

            Log_Debug("OledState: %d\n", oled_state);
#endif 
	    }
	    else {
		    Log_Debug("Button A released!\n");
	    }

    }

	// If the B button has just been pressed/released, send a telemetry message
	// The button has GPIO_Value_Low when pressed and GPIO_Value_High when released
    if (ButtonStateChanged(buttonBgpioFd, &buttonBState)) {

    	if (buttonBState == GPIO_Value_Low) {
			Log_Debug("Button B pressed!\n");

#ifdef RSL10_SALES_DEMO
            buttonPressed = true;
#endif 

#ifdef OLED_SD1306
            // Use buttonB presses to drive OLED to display the next screen
	    	oled_state++;
        
		    if (oled_state > OLED_NUM_SCREEN)
		    {
    			oled_state = 0;
	    	}
            Log_Debug("OledState: %d\n", oled_state);
#endif 
		}
		else {
			Log_Debug("Button B released!\n");
		}
	}

#ifdef RSL10_SALES_DEMO
    if(buttonPressed){

        // Update the send telemetry timer to send data every second
	    struct timespec newAccelReadPeriod = { .tv_sec =1,.tv_nsec = 0 };
        SetEventLoopTimerPeriod(sendTelemetryTimer, &newAccelReadPeriod);

        resetTelemetryTimeTimer = CreateEventLoopDisarmedTimer(eventLoop, ResetTelemetryTimerHandler);

        // Setup a one shot timer to set the telemetry period back to the default time
        struct timespec resetTelemeterTimerTime = { .tv_sec = DEFAULT_TELEMETRY_TX_TIME*5,.tv_nsec = 0 };
        SetEventLoopTimerOneShot(resetTelemetryTimeTimer, &resetTelemeterTimerTime);



    }

#endif 
	
}
/// <summary>
///     Azure timer event:  Check connection status and send telemetry
/// </summary>
static void AzureTimerEventHandler(EventLoopTimer *timer)
{
    if (ConsumeEventLoopTimerEvent(timer) != 0) {
        exitCode = ExitCode_AzureTimer_Consume;
        return;
    }

    // Keep the status LEDs updated
    updateConnectionStatusLed();

    // Check whether the device is connected to the internet.
    Networking_InterfaceConnectionStatus status;
    if (Networking_GetInterfaceConnectionStatus(NetworkInterface, &status) == 0) {
        if ((status & Networking_InterfaceConnectionStatus_ConnectedToInternet) &&
            (iotHubClientAuthenticationState == IoTHubClientAuthenticationState_NotAuthenticated)) {
            SetUpAzureIoTHubClient();

#ifdef USE_IOT_CONNECT
            // Kick off the IoTConnect specific logic since we're connected!
            IoTConnectConnectedToIoTHub();
#endif 
        }
    } else {
        if (errno != EAGAIN) {
            Log_Debug("ERROR: Networking_GetInterfaceConnectionStatus: %d (%s)\n", errno,
                      strerror(errno));
            exitCode = ExitCode_InterfaceConnectionStatus_Failed;
            return;
        }
    }

// Make sure we're connected to the IoT Hub
    if (iothubClientHandle != NULL) {
        IoTHubDeviceClient_LL_DoWork(iothubClientHandle);
    }
}

/// <summary>
///     Azure timer event:  Check connection status and send telemetry
/// </summary>
static void SendTelemetryTimerEventHandle(EventLoopTimer *timer) 
{
    if (ConsumeEventLoopTimerEvent(timer) != 0) {
        exitCode = ExitCode_AzureTimer_Consume;
        return;
    }

    // Call the routine to send the current telemetry data
    rsl10SendTelemetry();
}

/// <summary>
///     Parse the command line arguments given in the application manifest.
/// </summary>
static void ParseCommandLineArguments(int argc, char *argv[])
{
    int option = 0;
    static const struct option cmdLineOptions[] = {{"ConnectionType", required_argument, NULL, 'c'},
                                                   {"ScopeID", required_argument, NULL, 's'},
                                                   {"Hostname", required_argument, NULL, 'h'},
                                                   {"DeviceID", required_argument, NULL, 'd'},
                                                   {NULL, 0, NULL, 0}};

    // Loop over all of the options.
    while ((option = getopt_long(argc, argv, "c:s:h:d:", cmdLineOptions, NULL)) != -1) {
        // Check if arguments are missing. Every option requires an argument.
        if (optarg != NULL && optarg[0] == '-') {
            Log_Debug("WARNING: Option %c requires an argument\n", option);
            continue;
        }
        switch (option) {
        case 'c':
            Log_Debug("ConnectionType: %s\n", optarg);
            if (strcmp(optarg, "DPS") == 0) {
                connectionType = ConnectionType_DPS;
            } else if (strcmp(optarg, "Direct") == 0) {
                connectionType = ConnectionType_Direct;
            }
            break;
        case 's':
            Log_Debug("ScopeID: %s\n", optarg);
            scopeId = optarg;
            break;
        case 'h':
            Log_Debug("Hostname: %s\n", optarg);
            hubHostName = optarg;
            break;
        case 'd':
            Log_Debug("DeviceID: %s\n", optarg);
            deviceId = optarg;
            break;
        default:
            // Unknown options are ignored.
            break;
        }
    }
}

/// <summary>
///     Validates if the values of the Scope ID, IotHub Hostname and Device ID were set.
/// </summary>
/// <returns>ExitCode_Success if the parameters were provided; otherwise another
/// ExitCode value which indicates the specific failure.</returns>
static ExitCode ValidateUserConfiguration(void)
{
    ExitCode validationExitCode = ExitCode_Success;

    if (connectionType < ConnectionType_DPS || connectionType > ConnectionType_Direct) {
        validationExitCode = ExitCode_Validate_ConnectionType;
    }

    if (connectionType == ConnectionType_DPS) {
        if (scopeId == NULL) {
            validationExitCode = ExitCode_Validate_ScopeId;
        } else {
            Log_Debug("Using DPS Connection: Azure IoT DPS Scope ID %s\n", scopeId);
        }
    }

    if (connectionType == ConnectionType_Direct) {
        if (hubHostName == NULL) {
            validationExitCode = ExitCode_Validate_IotHubHostname;
        } else if (deviceId == NULL) {
            validationExitCode = ExitCode_Validate_DeviceId;
        }
        if (deviceId != NULL) {
            // Validate that device ID is in lowercase.
            size_t len = strlen(deviceId);
            for (size_t i = 0; i < len; i++) {
                if (isupper(deviceId[i])) {
                    Log_Debug("Device ID must be in lowercase.\n");
                    return ExitCode_Validate_DeviceId;
                }
            }
        }
        if (validationExitCode == ExitCode_Success) {
            Log_Debug("Using Direct Connection: Azure IoT Hub Hostname %s\n", hubHostName);
        }
    }

    if (validationExitCode != ExitCode_Success) {
        Log_Debug("Command line arguments for application shoud be set as below\n%s",
                  cmdLineArgsUsageText);
    }
    return validationExitCode;
}

/// <summary>
///     Set up SIGTERM termination handler, initialize peripherals, and set up event handlers.
/// </summary>
/// <returns>
///     ExitCode_Success if all resources were allocated successfully; otherwise another
///     ExitCode value which indicates the specific failure.
/// </returns>
static ExitCode InitPeripheralsAndHandlers(void)
{
    struct sigaction action;
    memset(&action, 0, sizeof(struct sigaction));
    action.sa_handler = TerminationHandler;
    sigaction(SIGTERM, &action, NULL);

    eventLoop = EventLoop_Create();
    if (eventLoop == NULL) {
        Log_Debug("Could not create event loop.\n");
        return ExitCode_Init_EventLoop;
    }

#ifdef RSL10_SALES_DEMO
    // Open SAMPLE_BUTTON_1 GPIO as input (ButtonA)
    Log_Debug("Opening SAMPLE_BUTTON_1 as input.\n");
    buttonAgpioFd = GPIO_OpenAsInput(SAMPLE_BUTTON_1);
    if (buttonAgpioFd == -1) {
        Log_Debug("ERROR: Could not open SAMPLE_BUTTON_1: %s (%d).\n", strerror(errno), errno);
        return ExitCode_Init_ButtonA;
    }

    // Open SAMPLE_BUTTON_2 GPIO as input (ButtonB)
    Log_Debug("Opening SAMPLE_BUTTON_2 as input.\n");
    buttonBgpioFd = GPIO_OpenAsInput(SAMPLE_BUTTON_2);
    if (buttonBgpioFd == -1) {
        Log_Debug("ERROR: Could not open SAMPLE_BUTTON_2: %s (%d).\n", strerror(errno), errno);
        return ExitCode_Init_ButtonB;
    }

    // Set up a timer to poll for button events.
    static const struct timespec buttonPressCheckPeriod = {.tv_sec = 0, .tv_nsec = 1000 * 1000};
    buttonPollTimer = CreateEventLoopPeriodicTimer(eventLoop, &ButtonPollTimerEventHandler,
                                                   &buttonPressCheckPeriod);
    if (buttonPollTimer == NULL) {
        return ExitCode_Init_ButtonPollTimer;
    }
#endif 
#ifdef OLED_SD1306

    // Initialize the i2c sensors
    lp_imu_initialize();

    // Set up a timer to drive quick oled updates.
    static const struct timespec oledUpdatePeriod = {.tv_sec = 0, .tv_nsec = 100 * 1000 * 1000};
    oledUpdateTimer = CreateEventLoopPeriodicTimer(eventLoop, &UpdateOledEventHandler,
                                                   &oledUpdatePeriod);
    if (oledUpdateTimer == NULL) {
        return ExitCode_Init_OledUpdateTimer;
    }
#endif 

    // Initialize the nRF Reset GPIO
    nRfnResetFd = GPIO_OpenAsOutput(SAMPLE_PMOD_PIN8, GPIO_OutputMode_PushPull, GPIO_Value_Low);
    if (nRfnResetFd < 0) {
         Log_Debug("ERROR: Could not open nRF52 GPIO: %s (%d).\n", strerror(errno), errno);
         return ExitCode_Init_nRF_Reset;
    }
    
    // Take the nRF out of reset
    GPIO_SetValue(nRfnResetFd, GPIO_Value_High);

    // Initialize the nRF Reset GPIO.  Driving the signal low allows the nRF application
    // to run automatically
    nRfnAutorunFd = GPIO_OpenAsOutput(SAMPLE_PMOD_PIN9, GPIO_OutputMode_PushPull, GPIO_Value_Low);
    if (nRfnAutorunFd < 0) {
        Log_Debug("ERROR: Could not open nRF52 GPIO: %s (%d).\n", strerror(errno), errno);
        return ExitCode_Init_nRF_Reset;
    }

    // Initailize the user LED FDs,
    for (int i = 0; i < RGB_NUM_LEDS; i++) {
        gpioConnectionStateLedFds[i] = GPIO_OpenAsOutput(gpioConnectionStateLeds[i],
                                                         GPIO_OutputMode_PushPull, GPIO_Value_High);
        if (gpioConnectionStateLedFds[i] < 0) {
            Log_Debug("ERROR: Could not open LED GPIO: %s (%d).\n", strerror(errno), errno);
            return ExitCode_Init_StatusLeds;
        }
    }


    // Create a UART_Config object, open the UART and set up UART event handler
    UART_Config uartConfig;
    UART_InitConfig(&uartConfig);
    uartConfig.baudRate = 115200;
    uartConfig.flowControl = UART_FlowControl_None;
    uartFd = UART_Open(SAMPLE_PMOD_UART, &uartConfig);
    if (uartFd == -1) {
        Log_Debug("ERROR: Could not open UART: %s (%d).\n", strerror(errno), errno);
        return ExitCode_Init_UartOpen;
    }
    uartEventReg = EventLoop_RegisterIo(eventLoop, uartFd, EventLoop_Input, UartEventHandler, NULL);
    if (uartEventReg == NULL) {
        return ExitCode_Init_RegisterIo;
    }

    azureIoTPollPeriodSeconds = AzureIoTDefaultPollPeriodSeconds;
    struct timespec azureTelemetryPeriod = {.tv_sec = azureIoTPollPeriodSeconds, .tv_nsec = 0};
    azureTimer =
        CreateEventLoopPeriodicTimer(eventLoop, &AzureTimerEventHandler, &azureTelemetryPeriod);
    if (azureTimer == NULL) {
        return ExitCode_Init_AzureTimer;
    }

    struct timespec sendTelemetryPeriod = {.tv_sec = DEFAULT_TELEMETRY_TX_TIME, .tv_nsec = 0};
    sendTelemetryTimer =
        CreateEventLoopPeriodicTimer(eventLoop, &SendTelemetryTimerEventHandle, &sendTelemetryPeriod);
    if (sendTelemetryTimer == NULL) {
        return ExitCode_Init_TelemetryTimer;
    }

#ifdef USE_IOT_CONNECT
    if (IoTConnectInit() != ExitCode_Success) {

        return ExitCode_Init_IoTCTimer;
    }
#endif

    return ExitCode_Success;
}

/// <summary>
///     Closes a file descriptor and prints an error on failure.
/// </summary>
/// <param name="fd">File descriptor to close</param>
/// <param name="fdName">File descriptor name to use in error message</param>
void CloseFdAndPrintError(int fd, const char *fdName)
{
    if (fd >= 0) {
        int result = close(fd);
        if (result != 0) {
            Log_Debug("ERROR: Could not close fd %s: %s (%d).\n", fdName, strerror(errno), errno);
        }
    }
}

/// <summary>
///     Close peripherals and handlers.
/// </summary>
static void ClosePeripheralsAndHandlers(void)
{
    DisposeEventLoopTimer(sendTelemetryTimer);
    DisposeEventLoopTimer(azureTimer);
    DisposeEventLoopTimer(buttonPollTimer);
#ifdef OLED_SD1306
    DisposeEventLoopTimer(oledUpdateTimer);
#endif 

    EventLoop_Close(eventLoop);
    EventLoop_UnregisterIo(eventLoop, uartEventReg);

#ifndef RSL10_SALES_DEMO
    CloseFdAndPrintError(buttonAgpioFd, "ButtonA Fd");
    CloseFdAndPrintError(buttonBgpioFd, "ButtonB Fd");
#endif 

    Log_Debug("Closing file descriptors\n");

    // Turn the WiFi connection status LEDs off
    setConnectionStatusLed(RGB_No_Connections);

    // Close the status LED file descriptors
    for (int i = 0; i < RGB_NUM_LEDS; i++) {
        // Turn off the LEDs before closing the FD
        GPIO_SetValue(gpioConnectionStateLedFds[i],GPIO_Value_High);
        CloseFdAndPrintError(gpioConnectionStateLedFds[i], "ConnectionStatusLED");
    }

    CloseFdAndPrintError(uartFd, "Uart");

#ifdef OLED_SD1306
    // Close the i2c FD
    lp_imu_close();
#endif 

}

/// <summary>
///     Callback when the Azure IoT connection state changes.
///     This can indicate that a new connection attempt has succeeded or failed.
///     It can also indicate than an existing connection has expired due to SAS token expiry.
/// </summary>
static void ConnectionStatusCallback(IOTHUB_CLIENT_CONNECTION_STATUS result,
                                     IOTHUB_CLIENT_CONNECTION_STATUS_REASON reason,
                                     void *userContextCallback)
{

    Log_Debug("Azure IoT connection status: %s\n", GetReasonString(reason));

    if (result != IOTHUB_CLIENT_CONNECTION_AUTHENTICATED) {
        iotHubClientAuthenticationState = IoTHubClientAuthenticationState_NotAuthenticated;
        return;
    }

    // If it's not !Authenticated, it must be authenticated.  Set the state.
    iotHubClientAuthenticationState = IoTHubClientAuthenticationState_Authenticated;

#ifndef RSL10_SALES_DEMO
#ifdef TARGET_QIIO_200
    // Send static device twin properties when connection is established
    TwinReportState("{\"demoManufacturer\":\"Qiio\",\"demoModel\":\"200 development board\"}");

    // If we pulled the cellular details from the device, send them up as device twin
    // reported properties.
    if (cellinfo) {
        TwinReportState(cellinfo);
        free(cellinfo);
        cellinfo = NULL;
    }
#else
    // The current IoTTemplate does not support these device twins, so don't send them

    // Send static device twin properties when connection is established
    TwinReportState("{\"manufacturer\":\"Avnet\",\"model\":\"Azure Sphere RSL10 Demo\"}");
#endif 
#endif 

    // Since the connection state just changed, update the status LEDs
    updateConnectionStatusLed();

#ifdef USE_IOT_CONNECT
    IoTConnectConnectedToIoTHub();
#endif
}

/// <summary>
///     Sets up the Azure IoT Hub connection (creates the iothubClientHandle)
///     When the SAS Token for a device expires the connection needs to be recreated
///     which is why this is not simply a one time call.
/// </summary>
static void SetUpAzureIoTHubClient(void)
{
    bool isClientSetupSuccessful = false;

    if (iothubClientHandle != NULL) {
        IoTHubDeviceClient_LL_Destroy(iothubClientHandle);
    }

    if (connectionType == ConnectionType_Direct) {
        isClientSetupSuccessful = SetUpAzureIoTHubClientWithDaa();
    } else if (connectionType == ConnectionType_DPS) {
        isClientSetupSuccessful = SetUpAzureIoTHubClientWithDps();
    }

    if (!isClientSetupSuccessful) {
        // If we fail to connect, reduce the polling frequency, starting at
        // AzureIoTMinReconnectPeriodSeconds and with a backoff up to
        // AzureIoTMaxReconnectPeriodSeconds
        if (azureIoTPollPeriodSeconds == AzureIoTDefaultPollPeriodSeconds) {
            azureIoTPollPeriodSeconds = AzureIoTMinReconnectPeriodSeconds;
        } else {
            azureIoTPollPeriodSeconds *= 2;
            if (azureIoTPollPeriodSeconds > AzureIoTMaxReconnectPeriodSeconds) {
                azureIoTPollPeriodSeconds = AzureIoTMaxReconnectPeriodSeconds;
            }
        }

        struct timespec azureTelemetryPeriod = {azureIoTPollPeriodSeconds, 0};
        SetEventLoopTimerPeriod(azureTimer, &azureTelemetryPeriod);

        Log_Debug("ERROR: Failed to create IoTHub Handle - will retry in %i seconds.\n",
                  azureIoTPollPeriodSeconds);
        return;
    }

    // Successfully connected, so make sure the polling frequency is back to the default
    azureIoTPollPeriodSeconds = AzureIoTDefaultPollPeriodSeconds;
    struct timespec azureTelemetryPeriod = {.tv_sec = azureIoTPollPeriodSeconds, .tv_nsec = 0};
    SetEventLoopTimerPeriod(azureTimer, &azureTelemetryPeriod);

    // Set client authentication state to initiated. This is done to indicate that
    // SetUpAzureIoTHubClient() has been called (and so should not be called again) while the
    // client is waiting for a response via the ConnectionStatusCallback().
    iotHubClientAuthenticationState = IoTHubClientAuthenticationState_AuthenticationInitiated;


// The current sales demo does not support any device twin operations, disable them for that build configuration
#ifndef RSL10_SALES_DEMO    
    IoTHubDeviceClient_LL_SetDeviceTwinCallback(iothubClientHandle, DeviceTwinCallback, NULL);
#endif     
    IoTHubDeviceClient_LL_SetConnectionStatusCallback(iothubClientHandle, ConnectionStatusCallback,
                                                      NULL);
}

/// <summary>
///     Sets up the Azure IoT Hub connection (creates the iothubClientHandle)
///     with DAA
/// </summary>
static bool SetUpAzureIoTHubClientWithDaa(void)
{
    // Set up auth type
    int retError = iothub_security_init(IOTHUB_SECURITY_TYPE_X509);
    if (retError != 0) {
        Log_Debug("ERROR: iothub_security_init failed with error %d.\n", retError);
        return false;
    }

    // Create Azure Iot Hub client handle
    iothubClientHandle =
        IoTHubDeviceClient_LL_CreateFromDeviceAuth(hubHostName, deviceId, MQTT_Protocol);

    if (iothubClientHandle == NULL) {
        Log_Debug("IoTHubDeviceClient_LL_CreateFromDeviceAuth returned NULL.\n");
        return false;
    }

    // Enable DAA cert usage when x509 is invoked
    if (IoTHubDeviceClient_LL_SetOption(iothubClientHandle, "SetDeviceId",
                                        &deviceIdForDaaCertUsage) != IOTHUB_CLIENT_OK) {
        Log_Debug("ERROR: Failure setting Azure IoT Hub client option \"SetDeviceId\".\n");
        return false;
    }

    return true;
}

/// <summary>
///     Sets up the Azure IoT Hub connection (creates the iothubClientHandle)
///     with DPS
/// </summary>
static bool SetUpAzureIoTHubClientWithDps(void)
{
    AZURE_SPHERE_PROV_RETURN_VALUE provResult =
        IoTHubDeviceClient_LL_CreateWithAzureSphereDeviceAuthProvisioning(scopeId, 10000,
                                                                          &iothubClientHandle);
    Log_Debug("IoTHubDeviceClient_LL_CreateWithAzureSphereDeviceAuthProvisioning returned '%s'.\n",
              GetAzureSphereProvisioningResultString(provResult));

    if (provResult.result != AZURE_SPHERE_PROV_RESULT_OK) {
        return false;
    }

    return true;
}

/// <summary>
///     Converts the Azure IoT Hub connection status reason to a string.
/// </summary>
static const char *GetReasonString(IOTHUB_CLIENT_CONNECTION_STATUS_REASON reason)
{
    static char *reasonString = "unknown reason";
    switch (reason) {
    case IOTHUB_CLIENT_CONNECTION_EXPIRED_SAS_TOKEN:
        reasonString = "IOTHUB_CLIENT_CONNECTION_EXPIRED_SAS_TOKEN";
        break;
    case IOTHUB_CLIENT_CONNECTION_DEVICE_DISABLED:
        reasonString = "IOTHUB_CLIENT_CONNECTION_DEVICE_DISABLED";
        break;
    case IOTHUB_CLIENT_CONNECTION_BAD_CREDENTIAL:
        reasonString = "IOTHUB_CLIENT_CONNECTION_BAD_CREDENTIAL";
        break;
    case IOTHUB_CLIENT_CONNECTION_RETRY_EXPIRED:
        reasonString = "IOTHUB_CLIENT_CONNECTION_RETRY_EXPIRED";
        break;
    case IOTHUB_CLIENT_CONNECTION_NO_NETWORK:
        reasonString = "IOTHUB_CLIENT_CONNECTION_NO_NETWORK";
        break;
    case IOTHUB_CLIENT_CONNECTION_COMMUNICATION_ERROR:
        reasonString = "IOTHUB_CLIENT_CONNECTION_COMMUNICATION_ERROR";
        break;
    case IOTHUB_CLIENT_CONNECTION_OK:
        reasonString = "IOTHUB_CLIENT_CONNECTION_OK";
        break;
    case IOTHUB_CLIENT_CONNECTION_NO_PING_RESPONSE:
        reasonString = "IOTHUB_CLIENT_CONNECTION_NO_PING_RESPONSE";
        break;
    }
    return reasonString;
}

/// <summary>
///     Converts AZURE_SPHERE_PROV_RETURN_VALUE to a string.
/// </summary>
static const char *GetAzureSphereProvisioningResultString(
    AZURE_SPHERE_PROV_RETURN_VALUE provisioningResult)
{
    switch (provisioningResult.result) {
    case AZURE_SPHERE_PROV_RESULT_OK:
        return "AZURE_SPHERE_PROV_RESULT_OK";
    case AZURE_SPHERE_PROV_RESULT_INVALID_PARAM:
        return "AZURE_SPHERE_PROV_RESULT_INVALID_PARAM";
    case AZURE_SPHERE_PROV_RESULT_NETWORK_NOT_READY:
        return "AZURE_SPHERE_PROV_RESULT_NETWORK_NOT_READY";
    case AZURE_SPHERE_PROV_RESULT_DEVICEAUTH_NOT_READY:
        return "AZURE_SPHERE_PROV_RESULT_DEVICEAUTH_NOT_READY";
    case AZURE_SPHERE_PROV_RESULT_PROV_DEVICE_ERROR:
        return "AZURE_SPHERE_PROV_RESULT_PROV_DEVICE_ERROR";
    case AZURE_SPHERE_PROV_RESULT_GENERIC_ERROR:
        return "AZURE_SPHERE_PROV_RESULT_GENERIC_ERROR";
    default:
        return "UNKNOWN_RETURN_VALUE";
    }
}

/// <summary>
///     Check the network status.
/// </summary>
bool IsConnectionReadyToSendTelemetry(void)
{
    Networking_InterfaceConnectionStatus status;
    if (Networking_GetInterfaceConnectionStatus(NetworkInterface, &status) != 0) {
        if (errno != EAGAIN) {
            Log_Debug("ERROR: Networking_GetInterfaceConnectionStatus: %d (%s)\n", errno,
                      strerror(errno));
            exitCode = ExitCode_InterfaceConnectionStatus_Failed;
            return false;
        }
        Log_Debug(
            "WARNING: Cannot send Azure IoT Hub telemetry because the networking stack isn't ready "
            "yet.\n");
        return false;
    }

    if ((status & Networking_InterfaceConnectionStatus_ConnectedToInternet) == 0) {
        Log_Debug(
            "WARNING: Cannot send Azure IoT Hub telemetry because the device is not connected to "
            "the internet.\n");
        return false;
    }

    return true;
}

/// <summary>
///     Sends telemetry to Azure IoT Hub
/// </summary>
void SendTelemetry(const char *jsonMessage, bool appendIoTConnectHeader)
{
    IOTHUB_MESSAGE_HANDLE messageHandle;

    // First check to see if we're connected to the IoT Hub, if not return!
    if (iotHubClientAuthenticationState != IoTHubClientAuthenticationState_Authenticated) {
        // AzureIoT client is not authenticated. Log a warning and return.
        Log_Debug("WARNING: Azure IoT Hub is not authenticated. Not sending telemetry.\n");
        return;
    }

    // Check whether the device is connected to the internet.
    if (IsConnectionReadyToSendTelemetry() == false) {
        return;
    }

#ifdef USE_IOT_CONNECT

    char *ioTConnectTelemetryBuffer;
    size_t ioTConnectMessageSize = strlen(jsonMessage) + IOTC_TELEMETRY_OVERHEAD;

    ioTConnectTelemetryBuffer = malloc(ioTConnectMessageSize);
    if (ioTConnectTelemetryBuffer == NULL) {
        exitCode = ExitCode_IoTCMalloc_Failed;
        return;
    }

    // If we don't need to append the IoTConnect header, then just send the original message
    // This sould be just the IoTConnect hello message
    if(!appendIoTConnectHeader){

        Log_Debug("Sending Azure IoT Hub telemetry: %s.\n", jsonMessage);                                          
        messageHandle = IoTHubMessage_CreateFromString(jsonMessage);

    }
    else if (FormatTelemetryForIoTConnect(jsonMessage, ioTConnectTelemetryBuffer,
                                      ioTConnectMessageSize)) {

        Log_Debug("Sending Azure IoT Hub telemetry: %s.\n", ioTConnectTelemetryBuffer);

        // Otherwise, set the message handle to use the modified message
        messageHandle = IoTHubMessage_CreateFromString(ioTConnectTelemetryBuffer);
    }
    else{

        Log_Debug("Not sending telemetry, not connected to IoTConnect!\n");

        // Free the memory
        free(ioTConnectTelemetryBuffer);
        return;
    }
#else

    Log_Debug("Sending Azure IoT Hub telemetry: %s.\n", jsonMessage);
    messageHandle = IoTHubMessage_CreateFromString(jsonMessage);

#endif

    // Make sure we created a valid message handle, if not cleanup and exit
    if (messageHandle == 0) {
        Log_Debug("ERROR: unable to create a new IoTHubMessage.\n");

#ifdef USE_IOT_CONNECT
        // Free the memory
        free(ioTConnectTelemetryBuffer);
#endif
        return;
    }

#ifdef USE_IOT_CONNECT
        // Set Content type and Encoding scheme for IoTConnect routine scheme
        IoTHubMessage_SetContentTypeSystemProperty(messageHandle,"application%2fjson");
        IoTHubMessage_SetContentEncodingSystemProperty(messageHandle, "utf-8");
#endif 

    // Attempt to send the message we created
    if (IoTHubDeviceClient_LL_SendEventAsync(iothubClientHandle, messageHandle, SendEventCallback,
                                             /*&callback_param*/ NULL) != IOTHUB_CLIENT_OK) {
        Log_Debug("ERROR: failure requesting IoTHubClient to send telemetry event.\n");
    } else {
        Log_Debug("INFO: IoTHubClient accepted the telemetry event for delivery.\n");
    }

    // Cleanup
    IoTHubMessage_Destroy(messageHandle);
#ifdef USE_IOT_CONNECT

    // Free the memory
    free(ioTConnectTelemetryBuffer);

#endif
}

/// <summary>
///     Callback invoked when the Azure IoT Hub send event request is processed.
/// </summary>
void SendEventCallback(IOTHUB_CLIENT_CONFIRMATION_RESULT result, void *context)
{
    Log_Debug("INFO: Azure IoT Hub send telemetry event callback: status code %d.\n", result);
}

/// <summary>
///     Enqueues a report containing Device Twin reported properties. The report is not sent
///     immediately, but it is sent on the next invocation of IoTHubDeviceClient_LL_DoWork().
/// </summary>
void TwinReportState(const char *jsonState)
{
    if (iothubClientHandle == NULL) {
        Log_Debug("ERROR: Azure IoT Hub client not initialized.\n");
    } else {
        if (IoTHubDeviceClient_LL_SendReportedState(
                iothubClientHandle, (const unsigned char *)jsonState, strlen(jsonState),
                ReportedStateCallback, NULL) != IOTHUB_CLIENT_OK) {
            Log_Debug("ERROR: Azure IoT Hub client error when reporting state '%s'.\n", jsonState);
        } else {
            Log_Debug("INFO: Azure IoT Hub client accepted request to report state '%s'.\n",
                      jsonState);
        }
    }
}

/// <summary>
///     Callback invoked when the Device Twin report state request is processed by Azure IoT Hub
///     client.
/// </summary>
void ReportedStateCallback(int result, void *context)
{
    Log_Debug("INFO: Azure IoT Hub Device Twin reported state callback: status code %d.\n", result);
}

/// <summary>
///     Handle UART event: if there is incoming data, print it.
///     This satisfies the EventLoopIoCallback signature.
/// </summary>
static void UartEventHandler(EventLoop *el, int fd, EventLoop_IoEvents events, void *context)
{
    // Uncomment for circular queue debug
//    #define ENABLE_UART_DEBUG

#define RX_BUFFER_SIZE 512
#define DATA_BUFFER_SIZE 512
#define DATA_BUFFER_MASK (DATA_BUFFER_SIZE - 1)

    // Buffer for incomming data
    uint8_t receiveBuffer[RX_BUFFER_SIZE];

    // Buffer for persistant data.  Sometimes we don't receive all the
    // data at once so we need to store it in a persistant buffer before processing.
    static uint8_t dataBuffer[DATA_BUFFER_SIZE];

    // The index into the dataBuffer to write the next piece of RX data
    static int nextData = 0;

    // The index to the head of the valid/current data, this is the beginning
    // of the next response
    static int currentData = 0;

    // The number of btyes in the dataBuffer, used to make sure we don't overflow the buffer
    static int bytesInBuffer = 0;

    ssize_t bytesRead;

    // Read the uart
    bytesRead = read(uartFd, receiveBuffer, RX_BUFFER_SIZE);

#ifdef ENABLE_UART_DEBUG
    Log_Debug("Enter: bytesInBuffer: %d\n", bytesInBuffer);
    Log_Debug("Enter: bytesRead: %d\n", bytesRead);
    Log_Debug("Enter: nextData: %d\n", nextData);
    Log_Debug("Enter: currentData: %d\n", currentData);
#endif

    // Check to make sure we're not going to over run the buffer
    if ((bytesInBuffer + bytesRead) > DATA_BUFFER_SIZE) {

        // The buffer is full, attempt to recover by emptying the buffer!
        Log_Debug("Buffer Full!  Purging\n");

        nextData = 0;
        currentData = 0;
        bytesInBuffer = 0;
        return;
    }

    // Move data from the receive Buffer into the Data Buffer.  We do this
    // because sometimes we don't receive the entire message in one uart read.
    for (int i = 0; i < bytesRead; i++) {

        // Copy the data into the dataBuffer
        dataBuffer[nextData] = receiveBuffer[i];
#ifdef ENABLE_UART_DEBUG
//        Log_Debug("dataBuffer[%d] = %c\n", nextData, receiveBuffer[i]);
#endif
        // Increment the bytes count
        bytesInBuffer++;

        // Increment the nextData pointer and adjust for wrap around
        nextData = ((nextData + 1) & DATA_BUFFER_MASK);
    }

    // Check to see if we can find a response.  A response will end with a '\n' character
    // Start looking at the beginning of the first non-processed message @ currentData

    // Use a temp buffer pointer in case we don't find a message
    int tempCurrentData = currentData;

    // Iterate over the valid data from currentData to nextData locations in the buffer
    while (tempCurrentData != nextData) {
        if (dataBuffer[tempCurrentData] == '\n') {

#ifdef ENABLE_UART_DEBUG
            // Found a message from index currentData to tempNextData
            Log_Debug("Found message from %d to %d\n", currentData, tempCurrentData);
#endif
            // Determine the size of the new message we just found, account for the case
            // where the message wraps from the end of the buffer to the beginning
            int responseMsgSize = 0;
            if (currentData > tempCurrentData) {
                responseMsgSize = (DATA_BUFFER_SIZE - currentData) + tempCurrentData;
            } else {
                responseMsgSize = tempCurrentData - currentData;
            }

            // Declare a new buffer to hold the response we just found
            uint8_t responseMsg[responseMsgSize + 1];

            // Copy the response from the buffer, do it one byte at a time
            // since the message may wrap in the data buffer
            for (int j = 0; j < responseMsgSize; j++) {
                responseMsg[j] = dataBuffer[(currentData + j) & DATA_BUFFER_MASK];
                bytesInBuffer--;
            }

            // Decrement the bytesInBuffer one more time to account for the '\n' charcter
            bytesInBuffer--;

            // Null terminate the message and print it out to debug
            responseMsg[responseMsgSize] = '\0';
#ifdef ENABLE_MSG_DEBUG            
            Log_Debug("\nRX: %s\n", responseMsg);
#endif 
            // Call the routine that knows how to parse the response and send data to Azure
            parseRsl10Message(responseMsg);

            // Update the currentData index and adjust for the '\n' character
            currentData = tempCurrentData + 1;
            // Overwrite the '\n' character so we don't accidently find it and think
            // we found a new mssage
            dataBuffer[tempCurrentData] = '\0';
        }

        else if (tempCurrentData == nextData) {

#ifdef ENABLE_UART_DEBUG
            Log_Debug("No message found, exiting . . . \n");
#endif
            return;
        }

        // Increment the temp CurrentData pointer and let it wrap if needed
        tempCurrentData = ((tempCurrentData + 1) & DATA_BUFFER_MASK);
    }

#ifdef ENABLE_UART_DEBUG
    Log_Debug("Exit: nextData: %d\n", nextData);
    Log_Debug("Exit: currentData: %d\n", currentData);
    Log_Debug("Exit: bytesInBuffer: %d\n", bytesInBuffer);
#endif
}

#ifdef RSL10_SALES_DEMO
// <summary>
///     Check whether a given button has just been pressed/released.
/// </summary>
/// <param name="fd">The button file descriptor</param>
/// <param name="oldState">Old state of the button (pressed or released)</param>
/// <returns>true if button state has changed, false otherwise</returns>
static bool ButtonStateChanged(int fd, GPIO_Value_Type *oldState)
{
    bool didButtonStateChange = false;
    GPIO_Value_Type newState;
    int result = GPIO_GetValue(fd, &newState);
    if (result != 0) {
        Log_Debug("ERROR: Could not read button GPIO: %s (%d).\n", strerror(errno), errno);
        exitCode = ExitCode_IsButtonPressed_GetValue;
    } else {
        // Button is pressed if it is low and different than last known state.
        didButtonStateChange = (newState != *oldState);
        *oldState = newState;
    }

    return didButtonStateChange;
}
#endif 

#ifdef OLED_SD1306
/// <summary>
///     Button timer event:  Check the status of the button
/// </summary>
static void UpdateOledEventHandler(EventLoopTimer *timer)
{

    if (ConsumeEventLoopTimerEvent(timer) != 0) {
        exitCode = ExitCode_ButtonTimer_Consume;
        return;
    }

	// Update/refresh the OLED data
	update_oled();
}

#endif 

#ifdef RSL10_SALES_DEMO
// Add a wifi network to the device. 
void addWifiNetwork(char* ssid, int ssidLength, char* psk, int pskLength) {
	
	Log_Debug("Adding ssid: %.*s\n", ssidLength, ssid);

	// Add the new network
	int networkId = WifiConfig_AddNetwork();
	int configResult = (networkId == -1) ? -1 : 0;

	// Create a new network entry.
	if (configResult != -1) {
		configResult =
			WifiConfig_SetSSID(networkId, ssid, (size_t)ssidLength);
	}

	// Set the access point security attributes
	if (configResult != -1) {
			configResult = WifiConfig_SetSecurityType(networkId, WifiConfig_Security_Wpa2_Psk);
			if (configResult != -1) {
				configResult =
					WifiConfig_SetPSK(networkId, psk, (size_t)pskLength);
			}
	}

	// Use targeted scan
	if (configResult != -1) {
		configResult = WifiConfig_SetTargetedScanEnabled(networkId, true);
	}

	// Enable the network
	if (configResult != -1) {
		configResult = WifiConfig_SetNetworkEnabled(networkId, true);
	}

	// If an error occured after creating the network but before persisting it,
	// then forget the network
	if (networkId != -1 && configResult == -1) {
		WifiConfig_ForgetNetworkById(networkId);
	}

	if (configResult == 0) {
		Log_Debug("INFO: Wi-Fi network details stored successfully.\n");
	}
	else {
		Log_Debug("ERROR: Store Wi-Fi network failed: %s (%d).\n", strerror(errno), errno);
	}
}

// Check to see if a network already exists on a system
bool networkExists(char* ssid, int ssidLength)
{
	ssize_t numNetworks = WifiConfig_GetStoredNetworkCount();
	WifiConfig_StoredNetwork networkArray[numNetworks];
	WifiConfig_GetStoredNetworks(networkArray, (size_t)numNetworks);

	for (int i = 0; i < numNetworks; i++) {

		if (strncmp(ssid, networkArray[i].ssid, (size_t)ssidLength) == 0) {
			return true;
		}
	}
	return false;
}
#endif // RSL10_SALES_DEMO