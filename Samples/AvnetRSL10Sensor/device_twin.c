/*

MIT License

Copyright (c) Avnet Corporation. All rights reserved.
Author: Brian Willess

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

*/

#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <stdio.h>
#include <math.h>

// applibs_versions.h defines the API struct versions to use for applibs APIs.
#include "applibs_versions.h"

#include <applibs/log.h>
#include <applibs/gpio.h>
#include <hw/sample_appliance.h>
#include "deviceTwin.h"
#include "parson.h"
#include "exit_codes.h"
#include "build_options.h"
#include "rsl10.h"

uint8_t oled_ms1[CLOUD_MSG_SIZE] = "    Azure Sphere";
uint8_t oled_ms2[CLOUD_MSG_SIZE];
uint8_t oled_ms3[CLOUD_MSG_SIZE] = "    Avnet MT3620";
uint8_t oled_ms4[CLOUD_MSG_SIZE] = "    Starter Kit";

#ifndef RSL10_SALES_DEMO

extern volatile sig_atomic_t terminationRequired;

// Track the current device twin version.  This is updated when we receive a device twin
// update, and used when we send a device twin reported property
int desiredVersion = 0;

int telemetryPollPeriod = 0;

// Define each device twin key that we plan to catch, process, and send reported property for.
// .twinKey - The JSON Key piece of the key: value pair
// .twinVar - The address of the application variable keep this key: value pair data
// .twinFD - The associated File Descriptor for this item.  This is usually a GPIO FD.  NULL if NA.
// .twinGPIO - The associted GPIO number for this item.  NO_GPIO_ASSOCIATED_WITH_TWIN if NA
// .twinType - The data type for this item, TYPE_BOOL, TYPE_STRING, TYPE_INT, or TYPE_FLOAT
// .active_high - true if GPIO item is active high, false if active low.  This is used to init the GPIO 
// .twinHandler - The handler that will be called for this device twin.  The function must have the same signaure 
// void <yourFunctionName>(void* thisTwinPtr, JSON_Object *desiredProperties);
// 
twin_t twinArray[] = {
    {.twinKey = "requireRsl10Authorization",.twinVar = &requireRsl10Authorization,.twinFd = NULL,.twinGPIO = NO_GPIO_ASSOCIATED_WITH_TWIN,.twinType = TYPE_INT,.active_high = true,.twinHandler = (genericBoolDTFunction)},
    {.twinKey = "telemetryPollTime",.twinVar = &telemetryPollPeriod,.twinFd = NULL,.twinGPIO = NO_GPIO_ASSOCIATED_WITH_TWIN,.twinType = TYPE_INT,.active_high = true,.twinHandler = (telemetryTimerDTFunction)},
    {.twinKey = "authorizedMac1",.twinVar = &Rsl10DeviceList[0],.twinFd = NULL,.twinGPIO = NO_GPIO_ASSOCIATED_WITH_TWIN,.twinType = TYPE_STRING,.active_high = true,.twinHandler = (rsl10AuthorizedDTFunction)},
    {.twinKey = "authorizedMac2",.twinVar = &Rsl10DeviceList[1],.twinFd = NULL,.twinGPIO = NO_GPIO_ASSOCIATED_WITH_TWIN,.twinType = TYPE_STRING,.active_high = true,.twinHandler = (rsl10AuthorizedDTFunction)},
    {.twinKey = "authorizedMac3",.twinVar = &Rsl10DeviceList[2],.twinFd = NULL,.twinGPIO = NO_GPIO_ASSOCIATED_WITH_TWIN,.twinType = TYPE_STRING,.active_high = true,.twinHandler = (rsl10AuthorizedDTFunction)},
    {.twinKey = "authorizedMac4",.twinVar = &Rsl10DeviceList[3],.twinFd = NULL,.twinGPIO = NO_GPIO_ASSOCIATED_WITH_TWIN,.twinType = TYPE_STRING,.active_high = true,.twinHandler = (rsl10AuthorizedDTFunction)},
    {.twinKey = "authorizedMac5",.twinVar = &Rsl10DeviceList[4],.twinFd = NULL,.twinGPIO = NO_GPIO_ASSOCIATED_WITH_TWIN,.twinType = TYPE_STRING,.active_high = true,.twinHandler = (rsl10AuthorizedDTFunction)},
    {.twinKey = "authorizedMac6",.twinVar = &Rsl10DeviceList[5],.twinFd = NULL,.twinGPIO = NO_GPIO_ASSOCIATED_WITH_TWIN,.twinType = TYPE_STRING,.active_high = true,.twinHandler = (rsl10AuthorizedDTFunction)},
    {.twinKey = "authorizedMac7",.twinVar = &Rsl10DeviceList[6],.twinFd = NULL,.twinGPIO = NO_GPIO_ASSOCIATED_WITH_TWIN,.twinType = TYPE_STRING,.active_high = true,.twinHandler = (rsl10AuthorizedDTFunction)},
    {.twinKey = "authorizedMac8",.twinVar = &Rsl10DeviceList[7],.twinFd = NULL,.twinGPIO = NO_GPIO_ASSOCIATED_WITH_TWIN,.twinType = TYPE_STRING,.active_high = true,.twinHandler = (rsl10AuthorizedDTFunction)},
    {.twinKey = "authorizedMac9",.twinVar = &Rsl10DeviceList[8],.twinFd = NULL,.twinGPIO = NO_GPIO_ASSOCIATED_WITH_TWIN,.twinType = TYPE_STRING,.active_high = true,.twinHandler = (rsl10AuthorizedDTFunction)},
    {.twinKey = "authorizedMac10",.twinVar = &Rsl10DeviceList[9],.twinFd = NULL,.twinGPIO = NO_GPIO_ASSOCIATED_WITH_TWIN,.twinType = TYPE_STRING,.active_high = true,.twinHandler = (rsl10AuthorizedDTFunction)}
   };

// Calculate how many twin_t items are in the array.  We use this to iterate through the structure.
int twinArraySize = sizeof(twinArray) / sizeof(twin_t);

///<summary>
///		Generic device twin handler for Integer types
///     This handler will update the integer variable referenced in the device twin handler
///</summary>
void genericIntDTFunction(void* thisTwinPtr, JSON_Object *desiredProperties){

    // Declare a local variable to point to the deviceTwin table entry and cast the incomming void* to a twin_t*
    twin_t *localTwinPtr = (twin_t*)thisTwinPtr;

    // Updte the variable referenced in the twin table
    *(int *)(twin_t*)localTwinPtr->twinVar = (int)json_object_get_number(desiredProperties, localTwinPtr->twinKey);
    Log_Debug("Received device update. New %s is %d\n", localTwinPtr->twinKey, *(int *)localTwinPtr->twinVar);
    
    // Send the reported property to the IoTHub
    checkAndUpdateDeviceTwin(localTwinPtr->twinKey, localTwinPtr->twinVar, TYPE_INT, true);
}

///<summary>
///		Generic device twin handler for Float types
///     This handler will update the float variable referenced in the device twin handler
///</summary>
void genericFloatDTFunction(void* thisTwinPtr, JSON_Object *desiredProperties){

    // Declare a local variable to point to the deviceTwin table entry and cast the incomming void* to a twin_t*
    twin_t *localTwinPtr = (twin_t*)thisTwinPtr;

    // Update the variable referenced by the twin table
    *(float *)localTwinPtr->twinVar = (float)json_object_get_number(desiredProperties, localTwinPtr->twinKey);
    Log_Debug("Received device update. New %s is %0.2f\n", localTwinPtr->twinKey, *(float *)localTwinPtr->twinVar);

    // Send the reported property to the IoTHub    
    checkAndUpdateDeviceTwin(localTwinPtr->twinKey, localTwinPtr->twinVar, TYPE_FLOAT, true);
}

///<summary>
///		Generic device twin handler for Boolean types (no GPIO device associated)
///</summary>
void genericBoolDTFunction(void* thisTwinPtr, JSON_Object *desiredProperties){

    // Declare a local variable to point to the deviceTwin table entry and cast the incomming void* to a twin_t*
    twin_t *localTwinPtr = (twin_t*)thisTwinPtr;

    // Update the variable referenced by the twin table
    *(bool *)localTwinPtr->twinVar = (bool)json_object_get_boolean(desiredProperties, localTwinPtr->twinKey);
    Log_Debug("Received device update. New %s is %s\n", localTwinPtr->twinKey,
                                                       *(bool *)localTwinPtr->twinVar ? "true" : "false");

    // Send the reported proptery to the IoTHub
    checkAndUpdateDeviceTwin(localTwinPtr->twinKey, localTwinPtr->twinVar, TYPE_BOOL, true);
}

///<summary>
///		Generic device twin handler for Boolean types with associated GPIO
///     This handler will update the integer variable referenced in the device twin handler and also set
//      the GPIO signal using details in the twin table
///</summary>
void genericGPIODTFunction(void* thisTwinPtr, JSON_Object *desiredProperties){

    // Declare a local variable to point to the deviceTwin table entry and cast the incomming void* to a twin_t*
    twin_t *localTwinPtr = (twin_t*)thisTwinPtr;

    // Read the new boolean value from the desired property structure
    *(bool *)localTwinPtr->twinVar = (bool)json_object_get_boolean(desiredProperties, localTwinPtr->twinKey);
    
    // Set the GPIO value using details from the twin table
    int result = GPIO_SetValue(*localTwinPtr->twinFd, localTwinPtr->active_high
                                                 ? (GPIO_Value) * (bool *)localTwinPtr->twinVar
                                                 : !(GPIO_Value) * (bool *)localTwinPtr->twinVar);
    
    // Check to make sure we we able to set the GPIO signal
    if (result != 0) {
        Log_Debug("Fd: %d\n", localTwinPtr->twinFd);
        Log_Debug("FAILURE: Could not set GPIO_%d, %d output value %d: %s (%d).\n",
                    localTwinPtr->twinGPIO, localTwinPtr->twinFd,
                    (GPIO_Value) * (bool *)localTwinPtr->twinVar, strerror(errno), errno);
        exitCode = ExitCode_SetGPIO_Failed;
    }
    
    Log_Debug("Received device update. New %s is %s\n", localTwinPtr->twinKey,
                                                       *(bool *)localTwinPtr->twinVar ? "true" : "false");
    
    // Send the reported property to the IoTHub    
    checkAndUpdateDeviceTwin(localTwinPtr->twinKey, localTwinPtr->twinVar, TYPE_BOOL, true);
}

///<summary>
///		Generic device twin handler for String types
///     This handler will update the string variable referenced in the device twin handler
///</summary>
void genericStringDTFunction(void* thisTwinPtr, JSON_Object *desiredProperties){

    // Declare a local variable to point to the deviceTwin table entry and cast the incomming void* to a twin_t*
    twin_t *localTwinPtr = (twin_t*)thisTwinPtr;

    // check to see if we have an empty string
    if(json_object_get_string(desiredProperties, localTwinPtr->twinKey) != NULL){                
                
        // The string is NOT empty, move the string into the local variable
        strcpy((char *)localTwinPtr->twinVar, (char *)json_object_get_string(desiredProperties, localTwinPtr->twinKey));
    }
    else {
        // The string is empty, update the local variable to ""
        strcpy((char *)localTwinPtr->twinVar, "");
    }
    
    Log_Debug("Received device update. New %s is %s\n", localTwinPtr->twinKey, (char *)localTwinPtr->twinVar);
    
    // Send the reported proptery to the IoTHub
    checkAndUpdateDeviceTwin(localTwinPtr->twinKey, localTwinPtr->twinVar, TYPE_STRING, true);

}

///<summary>
///		Handler for the RSL10 authorized MAC entries
///     This handler will add/remove/modify the specified MAC from the device list
///</summary>
void rsl10AuthorizedDTFunction(void* thisTwinPtr, JSON_Object *desiredProperties){

    // Declare a local variable to point to the deviceTwin table entry and cast the incomming void* to a twin_t*
    twin_t *localTwinPtr = (twin_t*)thisTwinPtr;

    // For this device twin entry we passed in the address of a RSL10Device_t structure, cast it so we can access
    // the device list entry for this device.
    RSL10Device_t *localDevicePtr = (RSL10Device_t*)localTwinPtr->twinVar;

    // check to see if the string is empty ""
    if(json_object_get_string(desiredProperties, localTwinPtr->twinKey) != NULL){                
                
        // Update the global mac string with the new MAC
        strcpy((char *)localDevicePtr->authorizedBdAddress, (char *)json_object_get_string(desiredProperties, localTwinPtr->twinKey));
        
        // Mark it as inactive, this will be updated when we receive the first message from the new device
        localDevicePtr->isActive = false;
        Log_Debug("new mac: %s\n", (char *)localTwinPtr->twinVar);
    }
    else {
    
        // This authorizedMacN entry was just removed, update the authorized address with "", and mark it inactive
        strcpy((char *)localDevicePtr->authorizedBdAddress, "");
        localDevicePtr->isActive = false;

    }
    Log_Debug("Received device update. New %s is %s\n", localTwinPtr->twinKey, (char *)localDevicePtr->authorizedBdAddress);
    
    // Send the reported proptery to the IoTHub
    checkAndUpdateDeviceTwin(localTwinPtr->twinKey, (char *)localDevicePtr->authorizedBdAddress, TYPE_STRING, true);
}

void telemetryTimerDTFunction(void* thisTwinPtr, JSON_Object *desiredProperties){

        // Declare a local variable to point to the deviceTwin table entry and cast the incomming void* to a twin_t*
    twin_t *localTwinPtr = (twin_t*)thisTwinPtr;

    // Updte the variable referenced in the twin table
    *(int *)(twin_t*)localTwinPtr->twinVar = (int)json_object_get_number(desiredProperties, localTwinPtr->twinKey);
    
    // Modify the telemetry poll timer with the updated value
    
    // Define a new timespec variable for the timer and change the timer period
	struct timespec newAccelReadPeriod = { .tv_sec =*(int *)(twin_t*)localTwinPtr->twinVar,.tv_nsec = 0 };
     SetEventLoopTimerPeriod(sendTelemetryTimer, &newAccelReadPeriod);
    

    Log_Debug("Received device update. New %s is %d\n", localTwinPtr->twinKey, *(int *)localTwinPtr->twinVar);
    
    // Send the reported property to the IoTHub
    checkAndUpdateDeviceTwin(localTwinPtr->twinKey, localTwinPtr->twinVar, TYPE_INT, true);

}


///<summary>
///		Send a simple {"key": value} device twin reported property update.  
///     Use the data type imput to determine how to construct the JSON
///     ioTPnPFormat allows us to send non-PnP formatted device twin updates.  This is needed to send
///     read only PnP updates.
///</summary>
void checkAndUpdateDeviceTwin(char* property, void* value, data_type_t type, bool ioTPnPFormat)
{
	int nJsonLength = -1;

#ifdef USE_PNP
	char* resultTxt = "Property successfully updated";
#endif 

	char *pjsonBuffer = (char *)malloc(JSON_BUFFER_SIZE);
	if (pjsonBuffer == NULL) {
		Log_Debug("ERROR: not enough memory to report device twin changes.");
	}

	if (property != NULL) {

        // report current device twin data as reported properties to IoTHub

        switch (type) {

#ifdef USE_PNP

		case TYPE_BOOL:
			if(ioTPnPFormat){
				nJsonLength = snprintf(pjsonBuffer, JSON_BUFFER_SIZE, cstrDeviceTwinPnPJsonBool, property, *(bool*)value ? "true" : "false", 200, desiredVersion, resultTxt);	
			}
            else{
				nJsonLength = snprintf(pjsonBuffer, JSON_BUFFER_SIZE, cstrDeviceTwinJsonBool, property, *(bool*)value ? "true" : "false", desiredVersion);
            }
			break;
		case TYPE_FLOAT:
            if(ioTPnPFormat){
				nJsonLength = snprintf(pjsonBuffer, JSON_BUFFER_SIZE, cstrDeviceTwinPnPJsonFloat, property, *(float*)value, 200, desiredVersion, resultTxt);	
			}
            else {
				nJsonLength = snprintf(pjsonBuffer, JSON_BUFFER_SIZE, cstrDeviceTwinJsonFloat, property, *(float*)value, desiredVersion);
            }
			break;
		case TYPE_INT:
			if(ioTPnPFormat){
				nJsonLength = snprintf(pjsonBuffer, JSON_BUFFER_SIZE, cstrDeviceTwinPnPJsonInteger, property, *(int*)value, 200, desiredVersion, resultTxt);	
			}
            else {
				nJsonLength = snprintf(pjsonBuffer, JSON_BUFFER_SIZE, cstrDeviceTwinJsonInteger, property, *(int*)value, desiredVersion);
            }
			break;
 		case TYPE_STRING:
        	if(ioTPnPFormat){
				nJsonLength = snprintf(pjsonBuffer, JSON_BUFFER_SIZE, cstrDeviceTwinPnPJsonString, property, (char*)value, 200, desiredVersion, resultTxt);	
			}
            else {
				nJsonLength = snprintf(pjsonBuffer, JSON_BUFFER_SIZE, cstrDeviceTwinJsonString, property, (char*)value, desiredVersion);
            }
			break;
		}

#else // Not PnP

		// report current device twin data as reported properties to IoTHub
		case TYPE_BOOL:
			nJsonLength = snprintf(pjsonBuffer, JSON_BUFFER_SIZE, cstrDeviceTwinJsonBool, property, *(bool*)value ? "true" : "false", desiredVersion);
			break;
		case TYPE_FLOAT:
			nJsonLength = snprintf(pjsonBuffer, JSON_BUFFER_SIZE, cstrDeviceTwinJsonFloat, property, *(float*)value, desiredVersion);
			break;
		case TYPE_INT:
			nJsonLength = snprintf(pjsonBuffer, JSON_BUFFER_SIZE, cstrDeviceTwinJsonInteger, property, *(int*)value, desiredVersion);
			break;
 		case TYPE_STRING:
			nJsonLength = snprintf(pjsonBuffer, JSON_BUFFER_SIZE, cstrDeviceTwinJsonString, property, (char*)value, desiredVersion);
			break;
		}
#endif 

		if (nJsonLength > 0) {
			Log_Debug("[MCU] Updating device twin: %s\n", pjsonBuffer);
            TwinReportState(pjsonBuffer);
		}
	}
    
    if(pjsonBuffer != NULL){
        free(pjsonBuffer);
    }

}

/// <summary>
///     Callback invoked when a Device Twin update is received from Azure IoT Hub.
///     Use the device twin table to call the function to process each "key" in the message
/// </summary>
void DeviceTwinCallback(DEVICE_TWIN_UPDATE_STATE updateState, const unsigned char *payload,
                               size_t payloadSize, void *userContextCallback)
{

    // Statically allocate this for more predictable memory use patterns
    static char nullTerminatedJsonString[MAX_DEVICE_TWIN_PAYLOAD_SIZE + 1];

    if (payloadSize > MAX_DEVICE_TWIN_PAYLOAD_SIZE) {
        Log_Debug("ERROR: Device twin payload size (%u bytes) exceeds maximum (%u bytes).\n",
                  payloadSize, MAX_DEVICE_TWIN_PAYLOAD_SIZE);

        exitCode = ExitCode_PayloadSize_TooLarge;
        return;
    }

    // Copy the payload to local buffer for null-termination.
    memcpy(nullTerminatedJsonString, payload, payloadSize);

    // Add the null terminator at the end.
    nullTerminatedJsonString[payloadSize] = '\0';

    // Parse the payload and prepare it for the parson parser
    JSON_Value *rootProperties = NULL;
    rootProperties = json_parse_string(nullTerminatedJsonString);
    if (rootProperties == NULL) {
        Log_Debug("WARNING: Cannot parse the string as JSON content.\n");
        goto cleanup;
    }

    // Set a pointer to the desired property object
    JSON_Object *rootObject = json_value_get_object(rootProperties);
    JSON_Object *desiredProperties = json_object_dotget_object(rootObject, "desired");
    if (desiredProperties == NULL) {
        desiredProperties = rootObject;
    }

    // Pull the twin version out of the message.  We use this value when we echo the new setting
    // back to IoT Connect.
    if (json_object_has_value(desiredProperties, "$version") != 0) {
        desiredVersion = (int)json_object_get_number(desiredProperties, "$version");
    }

    // Traverse the twin table, if we find a key that's in the table, then call the function
    // defined for that key.
    for (int i = 0; i < twinArraySize; i++)
    {

        if (json_object_has_value(desiredProperties, twinArray[i].twinKey) != 0) {

            // Call the function from the table, pass in a pointer to this table entry and the
            // desired property pointer
            twinArray[i].twinHandler(&twinArray[i], desiredProperties);

        }
    }

cleanup:
    // Release the allocated memory.
    json_value_free(rootProperties);
}

/// <summary>
///     Using the device twin table, send up initial device twin values as reported properties
/// </summary>
void sendInitialDeviceTwinReportedProperties(void)
{
    for (int i = 0; i < (sizeof(twinArray) / sizeof(twin_t)); i++) {

        switch (twinArray[i].twinType) {
        case TYPE_BOOL:
            Log_Debug("Send twin update. New %s is %s\n", twinArray[i].twinKey,
                      *(bool *)twinArray[i].twinVar ? "true" : "false");
            checkAndUpdateDeviceTwin(twinArray[i].twinKey, twinArray[i].twinVar, TYPE_BOOL,
                                     true);
            break;
        case TYPE_FLOAT:
            Log_Debug("Received device update. New %s is %0.2f\n", twinArray[i].twinKey,
                      *(float *)twinArray[i].twinVar);
            checkAndUpdateDeviceTwin(twinArray[i].twinKey, twinArray[i].twinVar, TYPE_FLOAT,
                                     true);
            break;
        case TYPE_INT:
            Log_Debug("Received device update. New %s is %d\n", twinArray[i].twinKey,
                      *(int *)twinArray[i].twinVar);
            checkAndUpdateDeviceTwin(twinArray[i].twinKey, twinArray[i].twinVar, TYPE_INT,
                                     true);
            break;

        case TYPE_STRING:
            Log_Debug("Received device update. New %s is %s\n", twinArray[i].twinKey,
                      (char *)twinArray[i].twinVar);
            checkAndUpdateDeviceTwin(twinArray[i].twinKey, twinArray[i].twinVar, TYPE_STRING,
                                     true);
            break;
        }
    }
}

/// <summary>
///     Traverse the device twin table.  If the entry operates on a GPIO FD, then open the FD and
///     set it's value acording to the table
/// </summary>

void deviceTwinOpenFDs()
{

    // Traverse the twin Array and for each GPIO item in the list open the file descriptor
    for (int i = 0; i < twinArraySize; i++) {

        // Verify that this entry is a GPIO entry
        if (twinArray[i].twinGPIO != NO_GPIO_ASSOCIATED_WITH_TWIN) {

            *twinArray[i].twinFd = -1;

            // For each item in the data structure, initialize the file descriptor and open the GPIO
            // for output.  Initilize each GPIO to its specific inactive state.
            *twinArray[i].twinFd =
                (int)GPIO_OpenAsOutput(twinArray[i].twinGPIO, GPIO_OutputMode_PushPull,
                                       twinArray[i].active_high ? GPIO_Value_Low : GPIO_Value_High);

            if (*twinArray[i].twinFd < 0) {
                Log_Debug("ERROR: Could not open LED %d: %s (%d).\n", twinArray[i].twinGPIO,
                          strerror(errno), errno);
            }
        }
    }
}

/// <summary>
///     Close any file descriptors that are managed from the device twin table
/// </summary>
void deviceTwinCloseFDs(void)
{
    // Traverse the twin Array and for each GPIO item in the list the close the file descriptor
    for (int i = 0; i < twinArraySize; i++) {

        // Verify that this entry has an open file descriptor
        if (twinArray[i].twinGPIO != NO_GPIO_ASSOCIATED_WITH_TWIN) {

            CloseFdAndPrintError(*twinArray[i].twinFd, twinArray[i].twinKey);
        }
    }
}

#endif // RSL10_SALES_DEMO