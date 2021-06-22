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

// This file implements the logic required to connect and interface with Avnet's IoTConnect platform

#include "iotConnect.h"
#include "../common/cloud.h"

#ifdef USE_IOT_CONNECT

// Declare global variables
char dtgGUID[GUID_LEN + 1];
char sidString[SID_LEN + 1];
bool IoTCConnected = false;

static EventLoopTimer *IoTCTimer = NULL;
static const int IoTCDefaultPollPeriodSeconds =
    15; // Wait for 15 seconds for IoT Connect to send first response

// Forwared function declarations
static void IoTCTimerEventHandler(EventLoopTimer *timer);
static void IoTCsendIoTCHelloTelemetry(void);
static IOTHUBMESSAGE_DISPOSITION_RESULT receiveMessageCallback(IOTHUB_MESSAGE_HANDLE, void *);

// Call when first connected to the IoT Hub
void IoTConnectConnectedToIoTHub(void)
{
    // Setup a callback for cloud to device messages.  This is how we'll receive the IoTConnect
    // hello response.
    IoTHubDeviceClient_LL_SetMessageCallback(iothubClientHandle, receiveMessageCallback, NULL);

    // Since we're going to be connecting or re-connecting to Azure
    // Set the IoT Connected flag to false
    IoTCConnected = false;

    // Send the IoT Connect hello message to inform the platform that we're on-line!
    IoTCsendIoTCHelloTelemetry();

    // Start the timer to make sure we see the IoT Connect "first response"
    const struct timespec IoTCHelloPeriod = {.tv_sec = IoTCDefaultPollPeriodSeconds, .tv_nsec = 0};
    SetEventLoopTimerPeriod(IoTCTimer, &IoTCHelloPeriod);
}

// Call from the main init function to setup periodic handler
ExitCode IoTConnectInit(void)
{

    // Create the timer to monitor the IoTConnect hello response status
    IoTCTimer = CreateEventLoopDisarmedTimer(eventLoop, &IoTCTimerEventHandler);
    if (IoTCTimer == NULL) {
        return ExitCode_Init_IoTCTimer;
    }

    return ExitCode_Success;
}

/// <summary>
/// IoTConnect timer event:  Check for response status and send hello message
/// </summary>
static void IoTCTimerEventHandler(EventLoopTimer *timer)
{

    if (ConsumeEventLoopTimerEvent(timer) != 0) {
        exitCode = ExitCode_IoTCTimer_Consume;
        return;
    }

    // If we're not connected to IoTConnect, then fall through to re-send
    // the hello message
    if (!IoTCConnected) {

        bool isNetworkReady = false;
        if (Networking_IsNetworkingReady(&isNetworkReady) != -1) {
            if (IsConnectionReadyToSendTelemetry()) {
                IoTCsendIoTCHelloTelemetry();
            }
        } else {
            Log_Debug("Failed to get Network state\n");
        }
    }
}

/// <summary>
///     Callback function invoked when a message is received from IoT Hub.
/// </summary>
/// <param name="message">The handle of the received message</param>
/// <param name="context">The user context specified at IoTHubDeviceClient_LL_SetMessageCallback()
/// invocation time</param>
/// <returns>Return value to indicates the message procession status (i.e. accepted, rejected,
/// abandoned)</returns>
static IOTHUBMESSAGE_DISPOSITION_RESULT receiveMessageCallback(IOTHUB_MESSAGE_HANDLE message,
                                                               void *context)
{
    Log_Debug("Received Cloud to device  message\n");

    // Use a flag to track if we rx the dtg value
    bool dtgFlag = false;

    const unsigned char *buffer = NULL;
    size_t size = 0;
    if (IoTHubMessage_GetByteArray(message, &buffer, &size) != IOTHUB_MESSAGE_OK) {
        Log_Debug("WARNING: failure performing IoTHubMessage_GetByteArray\n");
        return IOTHUBMESSAGE_REJECTED;
    }

    // 'buffer' is not null terminated, make a copy and null terminate it.
    unsigned char *str_msg = (unsigned char *)malloc(size + 1);
    if (str_msg == NULL) {
        Log_Debug("ERROR: could not allocate buffer for incoming message\n");
        abort();
    }

    memcpy(str_msg, buffer, size);
    str_msg[size] = '\0';

    Log_Debug("INFO: Received message '%s' from IoT Hub\n", str_msg);

    // Process the message.  We're expecting a specific JSON structure from IoT Connect
    //{
    //    "d": {
    //        "ec": 0,
    //        "ct": 200,
    //        "sid": "NDA5ZTMyMTcyNGMyNGExYWIzMTZhYzE0NTI2MTFjYTU=UTE6MTQ6MDMuMDA=",
    //        "meta": {
    //            "g": "0ac9b336-f3e7-4433-9f4e-67668117f2ec",
    //            "dtg": "9320fa22-ae64-473d-b6ca-aff78da082ed",
    //            "edge": 0,
    //            "gtw": "",
    //            "at": 1,
    //            "eg": "bdcaebec-d5f8-42a7-8391-b453ec230731"
    //        },
    //        "has": {
    //            "d": 0,
    //            "attr": 1,
    //            "set": 0,
    //            "r": 0,
    //            "ota": 0
    //        }
    //    }
    //}
    //
    // The code below will drill into the structure and pull out each piece of data and store it
    // into variables

    // Using the mesage string get a pointer to the rootMessage
    JSON_Value *rootMessage = NULL;
    rootMessage = json_parse_string(str_msg);
    if (rootMessage == NULL) {
        Log_Debug("WARNING: Cannot parse the string as JSON content.\n");
        goto cleanup;
    }

    // Using the rootMessage pointer get a pointer to the rootObject
    JSON_Object *rootObject = json_value_get_object(rootMessage);

    // Using the root object get a pointer to the d object
    JSON_Object *dProperties = json_object_dotget_object(rootObject, "d");
    if (dProperties == NULL) {
        Log_Debug("dProperties == NULL\n");
    }
    else{ // There is a "d" object, drill into it and pull the data

        // The d properties should have a "sid" key
        if (json_object_has_value(dProperties, "sid") != 0) {
            strncpy(sidString, (char *)json_object_get_string(dProperties, "sid"), SID_LEN);
            Log_Debug("sid: %s\n", sidString);

        } else {
            Log_Debug("sid not found!\n");
        }

        // Check to see if the object contains a "meta" object
        JSON_Object *metaProperties = json_object_dotget_object(dProperties, "meta");
        if (metaProperties == NULL) {
            Log_Debug("metaProperties not found\n");
        }
        else{

            // The meta properties should have a "dtg" key
            if (json_object_has_value(metaProperties, "dtg") != 0) {
                strncpy(dtgGUID, (char *)json_object_get_string(metaProperties, "dtg"), GUID_LEN);
                dtgFlag = true;
                Log_Debug("dtg: %s\n", dtgGUID);
            }
            else {
                Log_Debug("dtg not found!\n");
            }
        }

        // Check to see if we received all the required data we need to interact with IoTConnect
        if(dtgFlag){

            // Verify that the new dtg is a valid GUID, if not then we just received an empty dtg.
            if(GUID_LEN == strnlen(dtgGUID, GUID_LEN+1)){

                // Set the IoTConnect Connected flag to true
                IoTCConnected = true;
                Log_Debug("Set the IoTCConnected flag to true!\n");
            }
        }
        else{

            // Set the IoTConnect Connected flag to false
            IoTCConnected = false;
            Log_Debug("Did not receive all the required data from IoTConnect\n");
            Log_Debug("Set the IoTCConnected flag to false!\n");

        }
    }

cleanup:
    // Release the allocated memory.
    json_value_free(rootMessage);
    free(str_msg);

    return IOTHUBMESSAGE_ACCEPTED;
}

void IoTCsendIoTCHelloTelemetry(void)
{

    // Send the IoT Connect hello message to inform the platform that we're on-line!
    JSON_Value *rootValue = json_value_init_object();
    JSON_Object *rootObject =
    
    json_value_get_object(rootValue);
    json_object_dotset_number(rootObject, "mt", 200);
    json_object_dotset_number(rootObject, "v", IOT_CONNECT_API_VERSION);
    
    char *serializedTelemetryUpload = json_serialize_to_string(rootValue);
    AzureIoT_Result aziotResult = AzureIoT_SendTelemetry(serializedTelemetryUpload, NULL);
    Cloud_Result result = AzureIoTToCloudResult(aziotResult);
    
    if(result != Cloud_Result_OK){
        Log_Debug("IoTCHello message send error: %s\n", "error");
    }
    
    json_free_serialized_string(serializedTelemetryUpload);   
    json_value_free(rootValue);
}

// Construct a new message that contains all the required IoTConnect data and the original telemetry
// message Returns false if we have not received the first response from IoTConnect, or if the
// target buffer is not large enough
bool FormatTelemetryForIoTConnect(const char *originalJsonMessage, char *modifiedJsonMessage,
                                  size_t modifiedBufferSize)
{

    // Define the Json string format for sending telemetry to IoT Connect, note that the
    // actual telemetry data is inserted as the last string argument
    static const char IoTCTelemetryJson[] =
        "{\"sid\":\"%s\",\"dtg\":\"%s\",\"mt\": 0,\"d\":[{\"d\":%s}]}";

    // Verify that we've received the initial handshake response from IoTConnect, if not return
    // false
    if (!IoTCConnected) {
        Log_Debug(
            "Can't construct IoTConnect Telemetry message because application has not received the "
            "initial IoTConnect handshake\n");
        return false;
    }

    // Determine the largest message size needed.  We'll use this to validate the incomming target
    // buffer is large enough
    size_t maxModifiedMessageSize = strlen(originalJsonMessage) + IOTC_TELEMETRY_OVERHEAD;

    // Verify that the passed in buffer is large enough for the modified message
    if (maxModifiedMessageSize > modifiedBufferSize) {
        Log_Debug(
            "\nERROR: FormatTelemetryForIoTConnect() modified buffer size can't hold modified "
            "message\n");
        Log_Debug("                 Original message size: %d\n", strlen(originalJsonMessage));
        Log_Debug("Additional IoTConnect message overhead: %d\n", IOTC_TELEMETRY_OVERHEAD);
        Log_Debug("           Required target buffer size: %d\n", maxModifiedMessageSize);
        Log_Debug("             Actural target buffersize: %d\n\n", modifiedBufferSize);
        return false;
    }

    // Build up the IoTC message and insert the telemetry JSON
    snprintf(modifiedJsonMessage, maxModifiedMessageSize, IoTCTelemetryJson, sidString, dtgGUID,
             originalJsonMessage);

    //    Log_Debug("Original message: %s\n", originalJsonMessage);
    //    Log_Debug("Returning message: %s\n", modifiedJsonMessage);

    return true;
}

bool IoTConnectIsConnected(void){
    return IoTCConnected;
}

#endif //  USE_IOT_CONNECT