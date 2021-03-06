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


Implementation notes:

The functionality implemented in this file. . . .

1. Implements a generic interface to 1 - MAX_REAL_TIME_APPS real time applications
2. Allows the user to include implementation compliant real time applications to a solution with
   minimal configuration.  For a collection of real time compliant applications see the repo
   at https://github.com/Avnet/azure-sphere-samples.  Real time applications can be found
   in the /RealTimeSamples directory.  Please feel free to submit pull requests to add your
   real time application to this repo.
   
3. Implements common methods to interact with compliant real time applications

Compliant real time applications implement logic for the following commands/responses

IC_HEARTBEAT

    The real time application will receive this command and respond to the high level application
    with the HEARTBEAT response.

IC_READ_SENSOR

    The real time application will receive this command, read its sensors (whatever they are) and
    return raw data to the high level application.

    Note: To utilize this command/response the developer must define the data being returned and 
    modify the high level application to do something meaninfull with the data.

    Update IC_RESPONSE_BLOCK to include the data that the real time application returns.  This file
    should be updated in both the high level and real time application(s) for proper operation.

IC_READ_SENSOR_RESPOND_WITH_TELEMETRY

    The real time application will receive this command, read its sensors (whatever they are) and
    return valid JSON that can be sent to the IoTHub as telemetry


IC_SET_SAMPLE_RATE 

    The real time application will read the value (in seconds) passed with this command and will automatically
    read sensors and return valid telemetry JSON to the high level application at the rate defined by the 
    sample rate value.  The application will continue to send JSON until the application restarts, or a new
    IC_SET_SAMPLE_RATE command is sent with a value of zero.

Instructions to add a real time application

1. Identify the real time application's component ID

    The ComponentId GUID is defined in the real time application's app_manifest.json.  Usually at the top of the file. 
        
2. Add the real time application's ComponentID (GUID) to the following high level application's project files

    a. app_manifest.json (update "AllowedApplicationConnections" list)
    b. launch.vs.json (update "PartnerApplication" list)
    c. .vscode\launch.json (update "PartnerApplication" list)
   
3. Define a m4_support_t object in the m4Array[] located in this file.  The following items are required . . .

    .m4Name (string): The name of the application, used for debug and to make the table more readable
    .m4RtComponentID (GUID string): The component ID of the M4 application
    .m4InitHandler (function name): The routine that will be called on startup for this real time application
    .m4Handler (function name): The handler that will be when data is received from the M4 application
    .m4RawDataHandler (function name) : The handler that knows how to process the M4 application's raw data structure
    .m4TelemetryHandler (function name): The routine that will be called to request telemetry from the real time application
    .m4Cleanup (function name): The routine that will be called when the A7 application exits
    .m4InterfaceVersion (INTER_CORE_IMPLEMENTATION_VERSION): The implementation version
*/

#include "m4_support.h"

#ifdef M4_INTERCORE_COMMS

m4_support_t m4Array[] = {

#ifdef ENABLE_GENERIC_RT_APP
    // The AvnetGenericRTApp demonstrates how to use this common interface
    {
        .m4Name="AvnetGenericRTApp",
        .m4RtComponentID="9f19b84b-d83c-442b-b8b8-ce095a3b9b33",
        .m4InitHandler=genericM4Init,
        .m4Handler=genericM4Handler,
        .m4rawDataHandler=referenceRawDataHandler,
        .m4CleanupHandler=genericM4Cleanup,
	    .m4TelemetryHandler=genericM4RequestTelemetry,
        .m4InterfaceVersion=V0
    },
#endif
};

static EventRegistration *rtAppEventReg = NULL;

// Calculate how many twin_t items are in the array.  We use this to iterate through the structure.
int m4ArraySize = sizeof(m4Array)/sizeof(m4_support_t);

// Declare a global command block.  We use this structure to send commands to the real time applications
IC_COMMAND_RESPONSE_BLOCK ic_command_block;

// Global variable for device twin item realTimeAutoTelemetryInterval
int realTimeAutoTelemetryInterval = 0;

/// <summary>
///     sendInterCoreCommand()
///
///     Helper function to send commands to the real time app
///
/// </summary>
int sendInterCoreCommand(INTER_CORE_CMD cmd, int fd){

	// Send the command to the real time application
 	ic_command_block.cmd = cmd;

  	Log_Debug("Sending Command ID: %d\n", ic_command_block.cmd);

    int bytesSent = send( fd, &ic_command_block, sizeof(ic_command_block), 0);
    if (bytesSent == -1)
    {
   		Log_Debug("ERROR: Unable to send message: %d (%s)\n", errno, strerror(errno));
           exitCode = ExitCode_Write_RT_Socket;
    }

    return bytesSent;
}


/// <summary>
///     InitM4Socket(void)
///
///     This routine should be called from InitPeripheralsAndHandlers() in main.c 
///
///     Traverse the direct method table and call the init routine for each application 
///     defined in the array
///
/// </summary>
sig_atomic_t InitM4Interfaces(void){

    ExitCode result = ExitCode_Success;

    // Verify we have defined a maximum of MAX_REAL_TIME_APPS real time applications (MT3620 constraint)
    if(m4ArraySize > MAX_REAL_TIME_APPS){
        return ExitCode_Init_Invalid_Number_Real_Time_Apps;
    }

    // Traverse the M4 table, call the init routine for each entry
    for (int i = 0; i < m4ArraySize; i++)
    {
        // Each entry must have an init routine
        result = m4Array[i].m4InitHandler(&m4Array[i]);
        if(result != ExitCode_Success){
            return result;
        }
    }
    return result;
}

/// <summary>
///     CleanupM4Resources()
///
///     This routine should be called from ClosePeripheralsAndHandlers() in main.c 
///
///     Traverse the direct method table and call the cleanup routine for each application, 
///     defined in the array
///
/// </summary>
void CleanupM4Resources(void){

    // Traverse the m4 table, call the cleanup routine if defined
    for (int i = 0; i < m4ArraySize; i++)
    {
        // If this entry has a cleanup routine call it
        if (m4Array[i].m4CleanupHandler != NULL) {
            m4Array[i].m4CleanupHandler(&m4Array[i]);
        }
    }
}

/// <summary>
///  genericM4Init()
///
///  This routine can be specified for most real time applications
///
///  The generic M4 init function will . . .
///  1.  Open a intercore communication socket
///  2.  Update the current entry file descriptor
///  3.  Set the handler function for the real time application
///
/// </summary>

sig_atomic_t genericM4Init(void* thisM4Entry){

    // Cast the void pointer so we can index into the structure
    m4_support_t* m4Entry = (m4_support_t*)thisM4Entry;
    Log_Debug("%s M4 initFunction Called\n", m4Entry->m4Name);
    
    // Init the file descriptor to an invalid value
    m4Entry->m4Fd = -1;

	// Open connection to real-time capable application.
	m4Entry->m4Fd = Application_Connect(m4Entry->m4RtComponentID);
	if (m4Entry->m4Fd == -1) 
	{
		Log_Debug("ERROR: Unable to create socket: %d (%s)\n", errno, strerror(errno));
		Log_Debug("Real Time Core disabled or Component Id is not correct.\n");
        return ExitCode_Init_Open_Socket;
        
    }
	else
	{
		// Set timeout, to handle case where real-time capable application does not respond.
		static const struct timeval recvTimeout = { .tv_sec = 5,.tv_usec = 0 };
		int result = setsockopt(m4Entry->m4Fd, SOL_SOCKET, SO_RCVTIMEO, &recvTimeout, sizeof(recvTimeout));
		if (result == -1)
		{
			Log_Debug("ERROR: Unable to set socket timeout: %d (%s)\n", errno, strerror(errno));
			return ExitCode_Init_Open_Socket;
		}

    	// Register handler for incoming messages from real-time capable application.
        rtAppEventReg = EventLoop_RegisterIo(eventLoop, m4Entry->m4Fd, EventLoop_Input, m4Entry->m4Handler, NULL);
        if (rtAppEventReg == NULL) {
            return ExitCode_Init_RegisterIo;
        }

        // Send the Heartbeat command to the real time application
        sendInterCoreCommand(IC_HEARTBEAT, m4Entry->m4Fd);

    }
    
    return ExitCode_Success;
}

/// <summary>
///     genericM4Handler()
/// 
///     Handle socket event by reading incoming data from real-time capable application.
///
///     This generic handler assumes that the real time application is sending events as
///     defined in the INTER_CORE_CMD structure. 
///
///     typedef enum
///     {
///         IC_UNKNOWN, 
///         IC_HEARTBEAT,
///         IC_READ_SENSOR,    
///         IC_READ_SENSOR_RESPOND_WITH_TELEMETRY, 
///	        IC_SET_SAMPLE_RATE 
///     } INTER_CORE_CMD;
///
/// </summary>
void genericM4Handler(EventLoop *el, int fd, EventLoop_IoEvents events, void *context)
{
    IC_COMMAND_RESPONSE_BLOCK *responsePtr;
    int thisM4ArrayIndex = -1;

    // Read messages from real-time capable application.
    // If the RTApp has sent more than 265 bytes, then truncate.
    uint8_t rxBuf[MAX_RT_MESSAGE_SIZE];
    int bytesReceived = recv(fd, rxBuf, sizeof(rxBuf), 0);

    if (bytesReceived == -1) {
        Log_Debug("ERROR: Unable to receive message: %d (%s)\n", errno, strerror(errno));
        return;
    }

    // Cast the response message so we can index into the data
    responsePtr = (IC_COMMAND_RESPONSE_BLOCK*)rxBuf;

    switch (responsePtr->cmd)
    
    {
        // If the real time application sends this response message, then the payload contains
        // valid JSON telemetry.  Pull the JSON data out of the response, validate it and send
        // it to the IoTHub as telemetry.  Note that if configured, the real time application
        // can send this message with telemetry data automatically.
	    case IC_READ_SENSOR_RESPOND_WITH_TELEMETRY:

            // Sanity check the data, is this valid JSON?  If so, send it up as telemety.
            // If not print an error and exit.  The incomming JSON should already be NULL terminated

            // Null terminate the string before processing
            rxBuf[bytesReceived] = '\0';
            Log_Debug("RX: %s\n", &rxBuf[1]);

            // We need to handle two different cases here.  
            // 1. IoTHub or IoTCentral application: In this case we can just pass the already formatted JSON
            //    telemetry to the AzureIoT_SendTelemetry() routine.
            //
            // 2. IoTConnect application:  In this case we need to build out the required IoTConnect telemetry
            //    wrapper (my term), and insert the properly formatted JSON telemetry into the payload.

            JSON_Value *rootProperties = NULL;

            // Parse the payload and prepare it for the parson parser.  Note the json_parse_string() call 
            // allocates memory that must be freed with json_vlaue_free();
            rootProperties = json_parse_string(&rxBuf[1]);

#if (defined(IOT_HUB_APPLICATION) && !defined(USE_IOT_CONNECT)) // Case 1
            
            // If rootProperties == NULL, then the JSON is invalid
            if (rootProperties != NULL) {

                // Call the routine to send the JSON as telemetry
                 AzureIoT_SendTelemetry(&rxBuf[1], NULL);
            }
            else{
                Log_Debug("WARNING: Cannot parse the string as JSON content.\n");
            }

#elif defined(USE_IOT_CONNECT)

            // If this is an IoT Connect build, then we need to apply the IoT Connect JSON
            // wrapper around the received telemetry JSON.

            // Allocate a buffer 
            char *ioTConnectTelemetryBuffer;
            size_t ioTConnectMessageSize = bytesReceived + IOTC_TELEMETRY_OVERHEAD;

            ioTConnectTelemetryBuffer = malloc(ioTConnectMessageSize);
            if (ioTConnectTelemetryBuffer == NULL) {
                exitCode = ExitCode_IoTCMalloc_Failed;
                return;
            }

            // Call the routine to add the IoTConnect wrapper, this routine will return false if we're not currently
            // connected to IoTConnect.
            if(FormatTelemetryForIoTConnect(&rxBuf[1], ioTConnectTelemetryBuffer,
                                      ioTConnectMessageSize)){

                // Call the routine to send the JSON as telemetry
                AzureIoT_SendTelemetry(ioTConnectTelemetryBuffer, NULL);
            }

            // Free the memory
            free(ioTConnectTelemetryBuffer);

#endif 
            // Release the allocated memory.
            json_value_free(rootProperties);
            break;


        // If the real time application sends this response message, then the payload contains
        // an ACK that the new sample rate data was received and set in the real time application.
        case IC_SET_SAMPLE_RATE:

            Log_Debug("RealTime App sample rate set to %d seconds\n", (int)responsePtr->sensorSampleRate);
            break;

        // If the real time application sends this response message, then the payload contains
        // raw data as defined by the real time application.  Find the M4Array entry and call its
        // m4RawDataHandler, passing in the received response message.  
        case IC_READ_SENSOR:

            // Find the array index
            thisM4ArrayIndex = findArrayIndexByFd(fd);
            
            if(thisM4ArrayIndex != -1){
                // Call the specific handler for this real time application, pass in the response pointer
                if(m4Array[thisM4ArrayIndex].m4rawDataHandler != NULL){
                   m4Array[thisM4ArrayIndex].m4rawDataHandler(responsePtr);
                }
            }
            break;

        case IC_HEARTBEAT:
            Log_Debug("RealTime App responded with Heartbeat response\n");        
            break;

        case IC_UNKNOWN:
        default:
            Log_Debug("Warning: Unknown response from real time application\n");
            break;
    }
}

/// <summary>
///  genericM4Cleanup()
///
///  This handler is called at system exit to cleanup/release any system resources
///
/// </summary>
void genericM4Cleanup(void* thisM4Entry){

    m4_support_t* m4Entry = (m4_support_t*)thisM4Entry;
    Log_Debug("%s M4 cleanup called\n", m4Entry->m4Name);

    // Add logic if your implementation opened interfaces that shold be cleaned up

}

/// <summary>
///     RequestRawData()
/// 
///     This routine can be called from the main application when it wants to read the real time
///     applications sensor(s) and receive the raw sensor data.  Each real time application will 
///     receive this message, read it's sensor(s) and return data corresponding with the data structure
///     defined for the realtime application.  Refer to the real time applications readme.md for the
///     data structure it operates with.
///
/// </summary>
void RequestRawData(void) {

    // Traverse the DM table, call the raw data 
    for (int i = 0; i < m4ArraySize; i++)
    {
        // For each real time application call the routine to request raw data
        // Only send the request if there is handler defined to process the response
        if (m4Array[i].m4rawDataHandler != NULL) {
 
    	    // Send the Read Sensor command to the real time application
            sendInterCoreCommand(IC_READ_SENSOR, m4Array[i].m4Fd);   
        }
    }
}

/// <summary>
///     RequestRealTimeTelemetry()
/// 
///     This routine can be called from the main application when it wants to read the real time
///     applications sensor(s).  Each real time application will receive this message, read it's sensor(s) 
///     and return a valid json telemetry response.  The high level application will receive this json 
///     and pass it directly to the IoT Hub if connected.
///
/// </summary>
void RequestRealTimeTelemetry(void) {

    // Traverse the DM table, call the temeletry handler
    for (int i = 0; i < m4ArraySize; i++)
    {
        // For each real time application call the routine to request telemetry
        if (m4Array[i].m4TelemetryHandler != NULL) {
            m4Array[i].m4TelemetryHandler(&m4Array[i]);
        }
    }
}

/// <summary>
///     genericM4RequestTelemetry()
/// 
///     This routine will send the IC_READ_SENSOR_RESPOND_WITH_TELEMETRY command
///     to the real time application
///
/// </summary>
void genericM4RequestTelemetry(void* thisM4Entry){

    m4_support_t* m4Entry = (m4_support_t*)thisM4Entry;

    // Send the Read Telemetry command to the real time application
    sendInterCoreCommand(IC_READ_SENSOR_RESPOND_WITH_TELEMETRY, m4Entry->m4Fd);

}

/// <summary>
///     setRealTimeTelemetryInterval()
/// 
///     Send a new telemetry sample rate to each real time application
///
/// </summary>
void sendRealTimeTelemetryInterval(INTER_CORE_CMD cmd, uint32_t newInterval)
{

	// Send the command to the real time application
 	ic_command_block.cmd = cmd;
    ic_command_block.sensorSampleRate = newInterval;

    // Traverse the m4 array, send the new interval to each real time application
    for (int i = 0; i < m4ArraySize; i++)
    {

      	Log_Debug("Sending Command ID: %d\n", ic_command_block.cmd);

        int bytesSent = send( m4Array[i].m4Fd, &ic_command_block, sizeof(ic_command_block), 0);
        if (bytesSent == -1)
        {
   		    Log_Debug("ERROR: Unable to send message: %d (%s)\n", errno, strerror(errno));
            exitCode = ExitCode_Write_RT_Socket;
        }
    }
}

/// <summary>
/// findArrayIndexByFd()
/// 
/// Use the file descriptor to identify the m4Array index for the passed in fd
///
/// </summary>
int findArrayIndexByFd(int fd){
    for(int i = 0; i < m4ArraySize; i++){
        if(fd == m4Array[i].m4Fd){
            return i;
        }
    }
    return -1;
}

///////////////////////////////////////////////////////////////////////////////////////////////
//
//  Raw Data Handlers
//
//  These handlers are enabled from build_options.h
//
///////////////////////////////////////////////////////////////////////////////////////////////

#ifdef ENABLE_GENERIC_RT_APP
/// <summary>
///  referenceRawDataHandler()
///
/// This handler is called when the high level application receives a raw data read response from the
/// AvnetGenericRT real time application.
///
///  This handler is included as a refeence for your own custom raw data handler.
///
/// </summary>
void referenceRawDataHandler(void* msg){

    // Define the expected data structure.  Note this struct came from the AvnetGroveGPS real time application code
    typedef struct
    {
    	INTER_CORE_CMD cmd;
	    uint32_t sensorSampleRate;
	    uint8_t rawData8bit;
	    float rawDataFloat; 
    } IC_COMMAND_BLOCK_GENERIC_RT_APP;

    IC_COMMAND_BLOCK_GENERIC_RT_APP *messageData = (IC_COMMAND_BLOCK_GENERIC_RT_APP*) msg;
    Log_Debug("RX Raw Data: rawData8bit: %d, rawDataFloat: %.2f\n",
                            messageData->rawData8bit, messageData->rawDataFloat);

    // Add message structure and logic to do something with the raw data from the 
    // real time application

}
#endif 
#endif // M4_INTERCORE_COMMS