/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef BUILD_OPTIONS_H
#define BUILD_OPTIONS_H

//////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Connectivity options
//
//  IOT_HUB_APPLICATION: Enable for any configuration that connects to an IoTHub/IoTCentral.
//  USE_IOT_CONNECT: Enable to connect to Avnet's IoTConnect Cloud solution.
//
//  USE_PNP: Enable to buid a PNP compatable application.  Note that the user must define, validate and 
//           publish the PnP model to Microsoft's GitHub repo
//           https://docs.microsoft.com/en-us/azure/iot-pnp/overview-iot-plug-and-play                                    
//
//////////////////////////////////////////////////////////////////////////////////////////////////

// If your application is going to connect straight to a IoT Hub or IoT Connect, then enable this define.
//#define IOT_HUB_APPLICATION

// Define to build for Avnet's IoT Connect platform
//#define USE_IOT_CONNECT

// If this is a IoT Conect build, make sure to enable the IOT Hub application code
#ifdef USE_IOT_CONNECT
#define IOT_HUB_APPLICATION
#define IOT_CONNECT_API_VERSION 1
#endif 

// Define if you want to build the Azure IoT Hub/IoTCentral Plug and Play application functionality
//#define USE_PNP

// Make sure we're using the IOT Hub code for the PNP configuration
#ifdef USE_PNP
#define IOT_HUB_APPLICATION

// Use this model for exercising the certified PnP implementation, this model is in the public repo
#define IOT_PLUG_AND_PLAY_MODEL_ID "dtmi:avnet:mt3620_g100;1" // https://docs.microsoft.com/en-us/azure/iot-pnp/overview-iot-plug-and-play                                   

// Use this model for testing and exercising new features added since the certification June 2021
// This is a local model, i.e.,  this model is NOT in the public repo
//#define IOT_PLUG_AND_PLAY_MODEL_ID "dtmi:avnet:mt3620_g100;2" // https://docs.microsoft.com/en-us/azure/iot-pnp/overview-iot-plug-and-play                                   


#else // !USE_PNP
// Define a NULL model ID if we're not building for PnP
#define IOT_PLUG_AND_PLAY_MODEL_ID ""

#endif 

//////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Use the Avnet Starter Kit RGB LED to show network connection status
//
//  USE_SK_RGB_FOR_IOT_HUB_CONNECTION_STATUS: Enable to add logic to drive the Avnet Starter Kit
//  RGB LED to show network status.
//
//  Note: This feature is only avaliable when building IOT_HUB_APPLICATIONs 
//
//  Red: No wifi connection
//  Green: Wifi connection, not connected to Azure IoTHub
//  Blue: Wifi connected and authenticated to Azure IoTHub (Blue is good!)
//
//  For GUARDIAN_100 build
//
//  LED1 on: No wifi connection
//  LED2 on: Wifi connection, not connected to Azure IoTHub
//  LED3 on: Wifi connected and authenticated to Azure IoTHub (Blue is good!)
//
//////////////////////////////////////////////////////////////////////////////////////////////////

#define USE_SK_RGB_FOR_IOT_HUB_CONNECTION_STATUS

//////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Track telemetry TX status and resend on reconnect
//
//  ENABLE_TELEMETRY_RESEND_LOGIC: Enable to add logic that will track telemetry send status and
//  will attempt to resend un-sent telemetry data when the application reconnects to the IoTHub
//
//  Note: This feature is only available when building IOT_HUB_APPLICATIONs 
//
//  Feature Overview:
// 
//  Startup Logic: When the application starts an empty linked list is created and a callback is
//  configured called AzureIoT_SendTelemetryCallback().  This callback will be called when a 
//  telemetry send message has been successfully transmitted to the IoTHub.  Note that this callback
/// does NOT get called when the telemetry send fails.
//
//  Runtime Logic:  When the application sends telemetry using the Cloud_SendTelemetry() a new node is
//  added to the telemetry linked list capturing the telemetry JSON string.  When the routine sends the
//  telemetry using the AzureIoT_SendTelemetry() function, a pointer to the linked list node is passed
//  in as a void* context pointer. 
//
//  If the telemetry is successfully sent to the IoTHub, then AzureIoT_SendTelemetryCallback() is 
//  instantiated and includes the void* context pointer that refers to the linked list node.  At this time
//  the node is deleted from the list, since the message was sent.
//
//  In the happy path, this linked list would always have one item in the list and only for a short period
//  of time.  That's the time between when the application sends the message and when the callback is called
//  informing the application that the message was sent.
//
//  In the un-happy path, the telemetry message is not sent for some reason.  Maybe the network connection
//  went down, or the connection to the IoTHub is disrupted.  In this case, any telemetry messages that the
//  application attempts to send will be captured in the linked list.  
//
//  When ConnectionChangedCallbackHandler() is instantiated, the routine checks to see if the telemetry list 
//  contains any nodes.  If so, then the logic will attempt to send the telemetry messages again.  In this
//  case the linked list node already exists, so a new node is not added to the list.  Hopefully at this time
//  everything is working again and AzureIoT_SendTelemetryCallback() will be called informing the application
//  that the message was successfully sent, at which time the node will be removed from the list.
//
//  Things to consider when using this functionality/feature
//
//  1. Each time a new node is added to the list memory is allocated.  If the application never reconnects
//     eventually the device will run out of memory.  Consider catching this condition and writing any 
//     pending telemetry data to persistent memory so that the telemetry could be sent after the application
//     restarts.  Currently if memory for a new node can't be allocated, the application will exit with reason
//     code ExitCode_AddTelemetry_Malloc_Failed.
//  
//  2. If the telemetry is re-sent, then there is no guarantee or control mechanism to define how long after 
//     the first attempt the resend will occur.  If your cloud implementation is sensitive to time, then
//     consider adding a timestamp to your telemetry message as an additional {"key": Value} entry.  The
//     implementation DOES resend the messages in the same order that they we're originally sent.
//     
//////////////////////////////////////////////////////////////////////////////////////////////////

//#define ENABLE_TELEMETRY_RESEND_LOGIC

//////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Read and process incomming data from the G100 external USB UART device
//
//  ENABLE_UART_RX: Enable logic receives data from the G100 USB Type-B connector and process' it
//
//  This logic opens the UART connected to the G100 USB Type-B port and reads data into a buffer.
//  The data will be read until a '\n' new line character is received.  At that time the entire 
//  message will be passed to a routine parseAndSendToAzure().  parseAndSendToAzure() will check
//  to see if the incomming data is valid JSON and if so, it will pass the entire message up to the
//  IoTHub.  Otherwise, the message is output to debug.
//
//////////////////////////////////////////////////////////////////////////////////////////////////

//#define ENABLE_UART_RX

//////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Use the G100 external UART for debug
//
//  ENABLE_DEBUG_TO_UART: Enable logic that sends key evnets to the G100 USB Type-B connector.
//  Events that will be sent . . . 
//    Telemetry JSON
//    Device Twin Reported Properties JSON
//    Direct Method called (name of direct method)
//    IoTHub connection/disconnection events
//    <Your custom debug>
//
//  A new device twin property is also included in the logic called "enableUartDebug."  This
//  boolean device twin can enable/disable sending debug messages to the exernal UART at runtime
//
//  ENABLE_DEBUG_BY_DEFAULT controls the initial state of the device twin variable.  When set to 
//  "true", debug is enabled by default, and when set to "false" it's disabled by default.
//
//  To see the debug connect the G100 to your development PC, open a terminal application such as
//  TeraTerm.  Open the port with settings 115200, 8, N, 1.  By default the application enables
//  hardware flow control (RTS/CTS).
//
//////////////////////////////////////////////////////////////////////////////////////////////////

//#define ENABLE_DEBUG_TO_UART
#define ENABLE_DEBUG_BY_DEFAULT true

//////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Defer OTA update logic
//
//  When enabled the application has visibility and can manage/defer OTA updates for both the 
//  system (OS) and user applications.
//
//  The implementation provides two different approaches to managing OTA updates.  Note that these two
//  approaches should NOT both be used by an application since each approach uses common control flags and
//  each assumes it has ownership of the flags.
//
//  1. Set the system to only accept OTA updates at a specified time of day (UTC)
//  2. Allow the application to defer/resume OTA events
//
//  1. Define a time of day (UTC) to apply OTA updates.  The device twin "otaTargetUtcTime" takes a 
//     string argument in the format "HH:MM:xx" where HH is the hour of the day (0 - 23), and MM is the 
//     minutes of the hour (0 - 59).  For example sending "13:02:00" will defer any OTA updates until
//     01:02 PM (UTC).  The otaTargetUtcTime device twin handler writes the target time
//     to mutable storage and the implementation reads the mutable storage on startup to persist the
//     configuration across resets.  Once set the configuration will remain active until disabled.
//
//     To disable the functionality, update the device twin with an empty string "".  Note that the application
//     validates the string.  The following device twin strings are invalid:  "1:12:00", "12:1:00", "12:01", "a1:12:00"
//
//     If the empty string is received while an update is pending, then the delay will be cleared and the update will
//     kick off right away.
//
//  2. This method allows the application to defer OTA updates for a specified period of time.  This 
//     functionality could be useful if an application is executing in a critical section and can't
//     be interrupted by an OTA update.  The application simply calls void delayOtaUpdates(pausePeriod)
//     to defer OTA updates and then call allowOtaUpdates() once control exits the critical section.
//
//     Note that if an OTA update has already started, these calls can't stop the update.  However,
//     the implementation provides mechanisms to determine the current state of OTA updates.  See section
//     3 below.
//    
//  3. The Azure Sphere application can also poll the status of OTA updates.  For example if an
//     application frequently sleeps, or powers down to conserve power, the application can call
//     OtaUpdateIsInProgress() or OtaUpdateIsPending( to determine if an OTA update is pending or is 
//     currently being applied.  In this case the application may want to delay sleeping until the 
//     update has been applied.
//
//   app_manifest.json - The implementation requies the folowing entrys:
//      "SystemEventNotifications": true,
//      "SoftwareUpdateDeferral": true,
//      "MutableStorage": { "SizeKB": 8 }
//
//////////////////////////////////////////////////////////////////////////////////////////////////

//#define DEFER_OTA_UPDATES
//#define ENABLE_OTA_DEBUG_TO_UART  // Write OTA debug out the debug uart, usefull to test/degub this feature

// If SEND_OTA_STAUS_TELEMETRY is enabled the application sends additional telemetry to capture the OTA
// events and parameters in the cloud.
//
// TYPE_INT {"otaUpdateDelayPeriod", newDelayTime}                     // Deferal time in minutes
// TYPE_STRING {"otaUpdateType", UpdateTypeToString(data.update_type)} // System (OS) or application
// TYPE_STRING {"otaUpdateStatus", EventStatusToString(status)}        // "Pending", "Final", "Deferred", "Completed";
// TYPE_INT {"otaMaxDeferalTime", data.max_deferral_time_in_minutes)}  // Max allowable deferment time from the OS
//#define SEND_OTA_STATUS_TELEMETRY // Send OTA events and defer details as telemetry

//////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Optional connection to real-time M4 application
//
//  M4_INTERCORE_COMMS: Enable to add Intercore Communication code to the project
//  This will enable reading the ALST19 light sensor data from the M4 application
//  To exercise the inter-core communication code run the M4 application first
//
//////////////////////////////////////////////////////////////////////////////////////////////////

//#define M4_INTERCORE_COMMS

#ifdef M4_INTERCORE_COMMS
#define MAX_REAL_TIME_APPS 2
#define MAX_RT_MESSAGE_SIZE 256

// List of currently implemented Azure RTOS real time applications 
// define a max of two applications
#define ENABLE_GENERIC_RT_APP      // Example application that implements all the interfaces to work with this high level implementation
#endif 

//////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Default timer values
//
//////////////////////////////////////////////////////////////////////////////////////////////////

// Defines how often the read sensor periodic handler runs
#define SENSOR_READ_PERIOD_SECONDS 15
#define SENSOR_READ_PERIOD_NANO_SECONDS 0 * 1000

// Defines the default period to send telemetry data to the IoTHub
#define SEND_TELEMETRY_PERIOD_SECONDS 30
#define SEND_TELEMETRY_PERIOD_NANO_SECONDS 0 * 1000

//////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Application/Device Constants
//
//  These items will be sent to the IoT Hub on connection as read only device twins
//
//////////////////////////////////////////////////////////////////////////////////////////////////
#define VERSION_STRING "AvnetG100Template-V2" // {"versionString"; "AvnetG100Template-V2"}
#define DEVICE_MFG "Avnet" // {"manufacturer"; "Avnet"}
#define DEVICE_MODEL "Azure Sphere Guardian 100" // {"model"; "Azure Sphere Guardian 100"}

#endif 