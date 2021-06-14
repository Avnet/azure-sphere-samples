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
// If SEND_OTA_STAUS_TELEMETRY is enabled the application sends additional telemetry to capture the OTA
// events and parameters in the cloud.
//
// TYPE_INT {"otaUpdateDelayPeriod", newDelayTime}                     // Deferal time in minutes
// TYPE_STRING {"otaUpdateType", UpdateTypeToString(data.update_type)} // System (OS) or application
// TYPE_STRING {"otaUpdateStatus", EventStatusToString(status)}        // "Pending", "Final", "Deferred", "Completed";
// TYPE_INT {"otaMaxDeferalTime", data.max_deferral_time_in_minutes)}  // Max allowable deferment time from the OS
//
//////////////////////////////////////////////////////////////////////////////////////////////////

#include "deferred_updates.h"

extern EventLoop *eventLoop;
extern volatile sig_atomic_t exitCode;

// When deferredOtaUpdateTime==zero and acceptOtaUpdate==true then the logic will use the otaTargetUtcTime setting to 
// calculate a deferral time period
#define DEFAULT_OTA_DEFER_PERIOD_MINUTES 0
static int deferredOtaUpdateTime = DEFAULT_OTA_DEFER_PERIOD_MINUTES; // minutes

// Variables to hold a target UTC time to apply updates.  Initialize to invalid values.
static int otaTargetUtcHour;
static int otaTargetUtcMinute;

// Status flags
// Allows code to poll OTA status is pending
static bool pendingOtaUpdate;

// Used to allow code to poll if OTA update is in progress.  This is usefull if your application
// sleeps or powers down frequently.  You don't want to power down while an update is being applied.
static bool otaUpdateInProgress;

// Used to defer updates (true) or apply updates right away.
static bool acceptOtaUpdate;

// Application update events are received via an event loop.
static EventRegistration *otaUpdateEventReg = NULL;

// Forward declarations
static void deferredOtaUpdateCallback(SysEvent_Events event, SysEvent_Status status, const SysEvent_Info *info,
                           void *context);
static const char *EventStatusToString(SysEvent_Status status);
static const char *UpdateTypeToString(SysEvent_UpdateType updateType);
static bool WriteDelayTimeUTCToMutableFile(delayTimeUTC_t dataToWrite);
static bool ReadDelayTimeUTCFromMutableFile(delayTimeUTC_t* readData);

/// <summary>
///     Initialize system resources for deferring OTA updates
/// </summary>
ExitCode deferredOtaUpdate_Init(void){

    // Attempt to read deferred update values from mutable storage.  If this call returns false, then
    // either the data did not exist or there was some error.
    delayTimeUTC_t readData;
    if (ReadDelayTimeUTCFromMutableFile(&readData)){
        otaTargetUtcHour = readData.OTATargetUtcHour;
        otaTargetUtcMinute = readData.OTATargetUtcMinute;
        acceptOtaUpdate = readData.ACCEPTOtaUpdate;
    }
    else{

        // Initialize the interface variables and flags.
        otaTargetUtcHour = 0;
        otaTargetUtcMinute = 0;
        acceptOtaUpdate = true;
    }

    // Start in a known state
    deferredOtaUpdateTime = DEFAULT_OTA_DEFER_PERIOD_MINUTES;
    pendingOtaUpdate = false;
    otaUpdateInProgress = false;

    // Register a system event that fires when a OS or application OTA update is about to be applied
    otaUpdateEventReg = SysEvent_RegisterForEventNotifications(
        eventLoop, SysEvent_Events_UpdateReadyForInstall, deferredOtaUpdateCallback, NULL);
    if (otaUpdateEventReg == NULL) {
        Log_Debug("ERROR: could not register update event: %s (%d).\n", strerror(errno), errno);
        return ExitCode_SetUpSysEvent_RegisterEvent;
    }

    return ExitCode_Success;
}

/// <summary>
///     Cleanup any system resources associated with the OTA update defer implementation
/// </summary>
void deferredOtaUpdate_Cleanup(void){

    SysEvent_UnregisterForEventNotifications(otaUpdateEventReg);
}

/// <summary>
///     This function matches the SysEvent_EventsCallback signature, and is invoked
///     from the event loop when the system wants to perform an application or system update.
///     See <see cref="SysEvent_EventsCallback" /> for information about arguments.
/// </summary>
static void deferredOtaUpdateCallback(SysEvent_Events event, SysEvent_Status status, const SysEvent_Info *info,
                           void *context)
{
#ifdef ENABLE_OTA_DEBUG_TO_UART            
    // message pointer for uart debug
    char *message;
#endif // ENABLE_OTA_DEBUG_TO_UART            

    // Verify that we received the expected event, if not throw an error
    if (event != SysEvent_Events_UpdateReadyForInstall) {
        Log_Debug("ERROR: unexpected event: 0x%x\n", event);
        exitCode = ExitCode_UpdateCallback_UnexpectedEvent;
        return;
    }

    // Pull the event details from the system
    SysEvent_Info_UpdateData data;
    int result = SysEvent_Info_GetUpdateData(info, &data);

    // Verify the result data is valid
    if (result == -1) {
        Log_Debug("ERROR: SysEvent_Info_GetUpdateData failed: %s (%d).\n", strerror(errno), errno);
        exitCode = ExitCode_UpdateCallback_GetUpdateEvent;
        return;
    }

    // Print details about received message.
    Log_Debug("INFO: Update type: %s(%u)\n", UpdateTypeToString(data.update_type), data.update_type);
    Log_Debug("INFO: Status: %s (%u)\n", EventStatusToString(status), status);
    Log_Debug("INFO: Max deferal time: %d Minutes\n", data.max_deferral_time_in_minutes);

#if defined(SEND_OTA_STATUS_TELEMETRY) && defined(IOT_HUB_APPLICATION)
    // Send the OTA event details to the IoTHub in a telemetry message.  Note that if the OTA event
    // comes in before the device is connected to the IoTHub (likely in a power on scenario), this 
    // message my not be sent.  Consider enabling the ENABLE_TELEMETRY_RESEND_LOGIC build flag so 
    // that this telemetry message will be queued up and sent as soon as the IoTHub connection is 
    // established.
    Cloud_SendTelemetry(true, 3*ARGS_PER_TELEMETRY_ITEM, 
                              TYPE_STRING, "otaUpdateType", UpdateTypeToString(data.update_type),
                              TYPE_STRING, "otaUpdateStatus", EventStatusToString(status),
                              TYPE_INT, "otaMaxDeferalTime", data.max_deferral_time_in_minutes);

#endif // defined(SEND_OTA_STATUS_TELEMETRY) && defined(IOT_HUB_APPLICATION)

#ifdef ENABLE_OTA_DEBUG_TO_UART        
    // Build and send a debug message to the serial port
    message = malloc(256);
    snprintf(message, 256, "deferredOtaCallback(): %s\n\r", EventStatusToString(status));
    SendUartMessage(message);
    free(message);
#endif // ENABLE_OTA_DEBUG_TO_UART        

    // Process the event
    switch (status) {
        
        // There is a update pending
        case SysEvent_Status_Pending:
        
            // If the application is accepting updates then update the status flag.
            if (acceptOtaUpdate) {
                otaUpdateInProgress = true;
                pendingOtaUpdate = false;
                Log_Debug("INFO: Allowing update.\n");
#ifdef ENABLE_OTA_DEBUG_TO_UART        
                SendUartMessage("INFO: Allowing update.\n\r");
#endif // ENABLE_OTA_DEBUG_TO_UART                     
            // If the application is deferring updates, then determine how long to defer the update
            // set status flags and defer the update.
            } else {
        
                otaUpdateInProgress = false;
                pendingOtaUpdate = true;
        
                // Determine how long we need to defer the update.  There are two cases to consider
                // 1. There is a non-zero delay time in the deferredOtaUpdateTime variable.  In this 
                //    case just use the value in the variable.
                //
                // 2. If deferredOtaUpdateTime is zero, then the application has set a UTC time of day.
                //    Determine how many minutes from now until the defined UTC time and use that number 
                //    as the delay time (minutes)..

                // Declare a local variable for code efficiency
                int newDelayTime = deferredOtaUpdateTime;

                // If the defferedOTAUpdateTime is zero, then determine delay based on target UTC tod (Hr:Min)
                // calculate the delay time from now until the target time
                if(newDelayTime == 0){
    
                    time_t timeNow, targetTime;
                    struct tm tNow, tTarget;

                    // Get current time, and initialize the targetTime variable
                    timeNow = time(NULL);
                    targetTime = timeNow;

                    // Convert to tm structs so we can easily peek into the times
                    memcpy(&tNow, gmtime(&timeNow), sizeof(struct tm));
                    memcpy(&tTarget, gmtime(&targetTime), sizeof(struct tm));

                    // Set the target Hour and minute from the global variables
                    tTarget.tm_hour = otaTargetUtcHour;
                    tTarget.tm_min = otaTargetUtcMinute;
                    tTarget.tm_sec = 0;

                    // If our target hour is < the current hour, then we need to target
                    // tomorrow for the update.
                    if(tTarget.tm_hour < tNow.tm_hour) {
                        
                        // Advance to a next day
                        ++tTarget.tm_mday;
                    }

                    // call mktime on the struct to manage any end of month wrapping issues
                    mktime(&tTarget);

                    // Convert the tTarget struct data back to a time_t value so we can do some math
                    targetTime = mktime(&tTarget);

                    // Do the math and convert from seconds to minutes
                    newDelayTime = (targetTime - timeNow)/60;
                    Log_Debug("%d minutes until %s\n", newDelayTime, asctime(&tTarget));

                }
                // Else (newDelayTime > 0) and we use that value

                // Determine if we requested a delay > data.max_deferral_time_in_minutes.  If so, the set the 
                // delay to the maximum deferral time.  This logic should not come into play unless the application
                // has put off OTA updates over and over again.  In that case eventually the Azure Sphere OTA deferral
                // limts will be reached and this logic will fire.
                if( newDelayTime > data.max_deferral_time_in_minutes){
                    newDelayTime = data.max_deferral_time_in_minutes;
                    Log_Debug("INFO: Requested delay time > max deferral time, setting delay to max allowed time %d minutes\n", newDelayTime);
                }

                Log_Debug("INFO: Deferring update for %d minutes.\n", newDelayTime);

#ifdef ENABLE_OTA_DEBUG_TO_UART        
                // Build and send a debug message to the serial port
                char *message;
                message = malloc(64);
                snprintf(message, 64, "INFO: Deferring update for %d minutes.\n\r", newDelayTime);
                SendUartMessage(message);
                free(message);
#endif // ENABLE_OTA_DEBUG_TO_UART

                result = SysEvent_DeferEvent(SysEvent_Events_UpdateReadyForInstall, newDelayTime);
    
                if (result == -1) {
                    Log_Debug("ERROR: SysEvent_DeferEvent: %s (%d).\n", strerror(errno), errno);
#ifdef ENABLE_OTA_DEBUG_TO_UART                    
                    SendUartMessage("ERROR: SysEvent_DeferEvent: BW1\n\r");
#endif // ENABLE_OTA_DEBUG_TO_UART                    
                    exitCode = ExitCode_UpdateCallback_DeferEvent;
                }

#if defined(SEND_OTA_STATUS_TELEMETRY) && defined(IOT_HUB_APPLICATION)

                // Send the delay details to the IoTHub in a telemetry message.  Note that if the OTA event
                // comes in before the device is connected to the IoTHub (likely in a power on scenario), this 
                // message my not be sent.  Consider enabling the ENABLE_TELEMETRY_RESEND_LOGIC build flag so 
                // that this telemetry message will be queued up and sent as soon as the IoTHub connection is 
                // established.
                Cloud_SendTelemetry(true, 1*ARGS_PER_TELEMETRY_ITEM, 
                              TYPE_INT, "otaUpdateDelayPeriod", newDelayTime);

#endif // defined(SEND_OTA_STATUS_TELEMETRY) && defined(IOT_HUB_APPLICATION)
            }

        break;

        case SysEvent_Status_Final:
            pendingOtaUpdate = false;
            otaUpdateInProgress = false;
            Log_Debug("INFO: Final update. App will update in 10 seconds.\n");
#ifdef ENABLE_OTA_DEBUG_TO_UART        
            SendUartMessage("INFO: Final update. App will update in 10 seconds.\n\r");
#endif // ENABLE_OTA_DEBUG_TO_UART        
            // Terminate app before it is forcibly shut down and replaced.
            // The application may be restarted before the update is applied.
            exitCode = ExitCode_UpdateCallback_FinalUpdate;
            break;

        case SysEvent_Status_Deferred:
            Log_Debug("INFO: Update deferred.\n");
#ifdef ENABLE_OTA_DEBUG_TO_UART            
            SendUartMessage("INFO: Update deferred.\n\r");
#endif // ENABLE_OTA_DEBUG_TO_UART            
        
            // Set the flags to reflect the current state
            pendingOtaUpdate = true;
            otaUpdateInProgress = false;

            // We just deferred the update, we need to set the acceptOtaUpdate flag to 
            // true to allow the update proceede once the deferal time expires
            acceptOtaUpdate = true;
            break;

        case SysEvent_Status_Complete:

            Log_Debug("INFO: OTA Update completed!\n");
#ifdef ENABLE_OTA_DEBUG_TO_UART            
            SendUartMessage("INFO: OTA Update completed!\n\r");
#endif // ENABLE_OTA_DEBUG_TO_UART            
            break;

        default:
            Log_Debug("ERROR: Unexpected status %d.\n", status);
#ifdef ENABLE_OTA_DEBUG_TO_UART            
            SendUartMessage("ERROR: Unexpected status: BW2\n\r");
#endif // ENABLE_OTA_DEBUG_TO_UART            
            exitCode = ExitCode_UpdateCallback_UnexpectedStatus;
            break;
    }
}

/// <summary>
///     Convert the supplied system event status to a human-readable string.
/// </summary>
/// <param name="status">The status.</param>
/// <returns>String representation of the supplied status.</param>
static const char *EventStatusToString(SysEvent_Status status)
{
    switch (status) {
    case SysEvent_Status_Invalid:
        return "Invalid";
    case SysEvent_Status_Pending:
        return "Pending";
    case SysEvent_Status_Final:
        return "Final";
    case SysEvent_Status_Deferred:
        return "Deferred";
    case SysEvent_Status_Complete:
        return "Completed";
    default:
        return "Unknown";
    }
}

/// <summary>
///     Convert the supplied update type to a human-readable string.
/// </summary>
/// <param name="updateType">The update type.</param>
/// <returns>String representation of the supplied update type.</param>
static const char *UpdateTypeToString(SysEvent_UpdateType updateType)
{
    switch (updateType) {
    case SysEvent_UpdateType_Invalid:
        return "Invalid";
    case SysEvent_UpdateType_App:
        return "Application";
    case SysEvent_UpdateType_System:
        return "System";
    default:
        return "Unknown";
    }
}

/// <summary>
///     Wait for SIGTERM (or timeout)
/// </summary>
/// <param name="timeoutSecs">Timeout period in seconds</param>
ExitCode WaitForSigTerm(time_t timeoutSecs)
{
    sigset_t sigtermSet, oldMask;

    sigemptyset(&sigtermSet);
    sigaddset(&sigtermSet, SIGTERM);

    // Block SIGTERM - disables the existing SIGTERM handler
    if (sigprocmask(SIG_BLOCK, &sigtermSet, &oldMask) == -1) {
        Log_Debug("ERROR: Could not set process signal mask: %d (%s)", errno, strerror(errno));
        return ExitCode_SigTerm_SetSigMaskFailure;
    }

    struct timespec timeout = {.tv_sec = timeoutSecs, .tv_nsec = 0};

    int result = sigtimedwait(&sigtermSet, NULL, &timeout);

    switch (result) {
    case SIGTERM:
        Log_Debug("INFO: SIGTERM received; exiting.\n");
        return ExitCode_Success;
    case -1:
        if (errno == EAGAIN) {
            Log_Debug("ERROR: Timed out waiting for SIGTERM\n");
            return ExitCode_SigTerm_Timeout;
        } else {
            Log_Debug("ERROR: Waiting for SIGTERM: %d (%s)\n", errno, strerror(errno));
            return ExitCode_SigTerm_OtherFailure;
        }
    default:
        Log_Debug("WARNING: Unexpected signal received when waiting for SIGTERM: %d\n", result);
        return ExitCode_SigTerm_UnexpectedSignal;
    }
}

///<summary>
///     
///     Delay an upcomming OTA update for the given period.
///
///     When to use this routine:  If your application is performing a critical task and it can't be interrupted,
///     then you can call this routine to defer an update if it comes in while in your critical section.  You should
///     call allowOtaUpdates() when your code exits the critical section.
///
///     If the given period is larger than the max allowed period, it will be truncated
///     
///</summary>
void delayOtaUpdates(uint16_t pausePeriod){

    // Set the flag to defer ota updates and set the delay time to the incomming value.
    acceptOtaUpdate = false;
    deferredOtaUpdateTime = pausePeriod;
}

///<summary>
///     
///     Set the status to allow OTA updates to occur
///
///     The routine will inform the system that OTA events can proceed.  If this
///     routine is called when 
///     
///</summary>
void allowOtaUpdates(void){

    acceptOtaUpdate = true;
    deferredOtaUpdateTime = 0;

    // Inform the system that we no longer need to defer any pending updates
    SysEvent_ResumeEvent(SysEvent_Events_UpdateReadyForInstall);
}

///<summary>
///     Ask if the system is currently applying an OTA update
///</summary>
bool OtaUpdateIsInProgress(void){
    return otaUpdateInProgress;
}

///<summary>
///     Ask if there is a OTA update pending, but defered
///</summary>
bool OtaUpdateIsPending(void){
    return pendingOtaUpdate;
}

///<summary>
///     Handler to process otaTargetUtcTime device twin
///     The routine is expecting a string variable "HH:MM:xx", and will update the global variables and write them
///     to mutable storage. 
///       otaTargetUtcHour
///       otaTargetUtcMinute
///
///      Note: When valid HH:MM:xx data is received the handler will set the flag to defer updates.  To disable
///      deferred updtes, send the empty string "".
///
///      I'm not proud of this function, but it works
///</summary>
void setOtaTargetUtcTime(void* thisTwinPtr, JSON_Object *desiredProperties){

    char tempTargetTime[16];
    char returnString[9] = {'\0'};

    // Declare a local variable to point to the deviceTwin table entry and cast the incomming void* to a twin_t*
    twin_t *localTwinPtr = (twin_t*)thisTwinPtr;

    // check to see if we have data
    if(json_object_get_string(desiredProperties, localTwinPtr->twinKey) != NULL){                

        // The string is NOT empty, move the string into the local variable that we'll manuipulate.  Make a clean copy to return with the 
        // reported property message.
        strncpy((char *)tempTargetTime, (char *)json_object_get_string(desiredProperties, localTwinPtr->twinKey), 16);
        strncpy(returnString, tempTargetTime, strlen(tempTargetTime));

        // The incomming data must be in the following format: "HH:MM:xx"
        //
        //  We'll perform the following checks then pull the HH (hour) and MM (minute) data.  We ignore the xx or seconds data
        //  since the defer update interface only supports minute resolutions.
        // 
        //  * String lengh == 8
        //  * tempTargetTime[2] == tempTargetTime[5] == ':'
        //  * HH is in the range (0 -23)
        //  * MM is in the range (0 -59)
        //
        //  If the data checks pass, then the global variables otaTargetUtcHour and otaTargetUtcMinute are updated with the new value,
        //  acceptOtaUpdate is set to false.  The data is written to mutable storage (only if the data in mutable storage different).
        //
        //  If a empty string is received, then deferred updates are disabled and acceptOtaUpdate is set to false.  The data is written 
        //  to mutable storage (only if the data in mutable storage different).


        // Verify that the incomming device twin string is the correct length
        size_t stringSize = strnlen(tempTargetTime,16);
        if (stringSize == 8){

            // Verify there are two ':'s in the correct locations . . .
            if ((tempTargetTime[2] == ':') && (tempTargetTime[5] == ':')){

                // Verify that the HH and MM data are all digits
                if (isdigit(tempTargetTime[0]) && isdigit(tempTargetTime[1]) && isdigit(tempTargetTime[3]) && isdigit(tempTargetTime[4])){

                    // Overwrite the ':'s with '\0' so we can use strtoumax to pull the data and convert it to integers
                    tempTargetTime[2] = tempTargetTime[5] = '\0';

                    // Pull the first number the hour
                    uint8_t tempHH = strtoumax(&tempTargetTime[0], NULL, 10);

                    if(tempHH > 23 ){
                        Log_Debug("ERROR: Hour out of range: %d\n", tempHH);
                        return;
                    }

                    // Pull the second number the minutes
                    uint8_t tempMM = strtoumax(&tempTargetTime[3], NULL, 10);

                    if(tempMM > 59){
                        Log_Debug("ERROR: Minute out of range: %d\n", tempMM);
                        return;
                    }

                    // The data is in the correct ranges, update the global variables
                    otaTargetUtcHour = tempHH;
                    otaTargetUtcMinute = tempMM;

                    // Update the flags to indicate that we want to defer OTA updates until the new desired time.
                    // These values will control the logic to calculate minutes between the target time and when the 
                    // OTA update event comes in.  The update will be defered for the calculated period of time.
                    acceptOtaUpdate = false;
                    deferredOtaUpdateTime = 0;

                    // Overwrite the SS data with 0's just in case some yahoo decided that this is a bug . . .
                    // Valid HH:MM data will always send a reported property HH:MM:00
                    returnString[6] = returnString[7] = '0';

                    // Fall through to // Fall through to write these values to mutable storage
                }
                // Data is not numbers
                else{
                    Log_Debug("ERROR: Input string contains non-digit data!\n");
                    return;
                }
            }
            else{
                    Log_Debug("ERROR: Input string not formatted correctly!\n");
                    return;
            }
        }
        // The string is empty, disable the deferred update logic
        else if (stringSize == 0){
            Log_Debug("Empty string, disable deferring OTA updates!\n");
            otaTargetUtcHour = 0;
            otaTargetUtcMinute = 0;
            acceptOtaUpdate = true;

            // Fall through to write these values to mutable storage
        }
        // The string is the incorrect length and therefore can't be processed
        else{
            Log_Debug("ERROR: String is incorrect length: %d!\n", stringSize);
            return;
        }
    }

    // Declare a varible and use the current settings to initialize it.  Write the data mutable storage
    // so that if the device resets, it can defer the update.
    delayTimeUTC_t dataToWrite;
    dataToWrite.OTATargetUtcHour = otaTargetUtcHour;
    dataToWrite.OTATargetUtcMinute = otaTargetUtcMinute;
    dataToWrite.ACCEPTOtaUpdate = acceptOtaUpdate;
    WriteDelayTimeUTCToMutableFile(dataToWrite);

    if(pendingOtaUpdate){

#ifdef ENABLE_OTA_DEBUG_TO_UART
        SendUartMessage("Update is pending, relese the hounds!!!!\n\r");
#endif // ENABLE_OTA_DEBUG_TO_UART        
        // Inform the system that we no longer need to defer any pending updates.  The ota callback
        // will get called again and the logic will set the delay based on the updated settings.
        SysEvent_ResumeEvent(SysEvent_Events_UpdateReadyForInstall);
    }

    Log_Debug("Defering next OTA update until %d:%d UTC\n", otaTargetUtcHour, otaTargetUtcMinute);
#ifdef ENABLE_OTA_DEBUG_TO_UART    
    SendUartMessage("Defering next OTA update until.\n\r");
#endif // ENABLE_OTA_DEBUG_TO_UART
    Log_Debug("Received device update. New %s is %s\n", localTwinPtr->twinKey, returnString);

    // Send the reported proptery to the IoTHub
    updateDeviceTwin(true, ARGS_PER_TWIN_ITEM*1, TYPE_STRING, localTwinPtr->twinKey, returnString);
}

/// <summary>
/// Write a delayTimeUTC_t to the device's persistent data file
/// Only write the data if it's different that what's currently in mutable storage
/// </summary>
static bool WriteDelayTimeUTCToMutableFile(delayTimeUTC_t dataToWrite)
{
    bool returnValue = true;

    // Check to see if the data already in mutable storage is the same as the data we indend to write.
    // Is so, return true!
    delayTimeUTC_t readData;
    if (ReadDelayTimeUTCFromMutableFile(&readData)){
        if(readData.ACCEPTOtaUpdate == dataToWrite.ACCEPTOtaUpdate &&
           readData.OTATargetUtcHour == dataToWrite.OTATargetUtcHour &&
           readData.OTATargetUtcMinute == dataToWrite.OTATargetUtcMinute){
               Log_Debug("Data to write is already in mutable storage!\n");
               return true;
        }
    }

    int fd = Storage_OpenMutableFile();
    if (fd == -1) {
        Log_Debug("ERROR: Could not open mutable file:  %s (%d).\n", strerror(errno), errno);
        exitCode = ExitCode_WriteFile_OpenMutableFile;
        return false;
    }
 
    ssize_t ret = write(fd, &dataToWrite, sizeof(delayTimeUTC_t));
    if (ret == -1) {
        // If the file has reached the maximum size specified in the application manifest,
        // then -1 will be returned with errno EDQUOT (122)
        Log_Debug("ERROR: An error occurred while writing to mutable file:  %s (%d).\n",
                  strerror(errno), errno);
        exitCode = ExitCode_WriteFile_Write;
        returnValue = false;
    } else if (ret < sizeof(delayTimeUTC_t)) {
        // For simplicity, this sample logs an error here. In the general case, this should be
        // handled by retrying the write with the remaining data until all the data has been
        // written.
        Log_Debug("ERROR: Only wrote %d of %d bytes requested\n", ret, (int)sizeof(delayTimeUTC_t));
        returnValue = false;
    }
    close(fd);
    return returnValue;
}

/// </summary>
///  Read delayTimeUTC_t data from mutable storage
/// <summary>
static bool ReadDelayTimeUTCFromMutableFile(delayTimeUTC_t* readData)
{
    int fd = Storage_OpenMutableFile();
    if (fd == -1) {
        Log_Debug("ERROR: Could not open mutable file:  %s (%d).\n", strerror(errno), errno);
        exitCode = ExitCode_ReadFile_OpenMutableFile;
        return false;
    }
    ssize_t ret = read(fd, readData, sizeof(delayTimeUTC_t));
    if (ret == -1) {
        Log_Debug("ERROR: An error occurred while reading file:  %s (%d).\n", strerror(errno),
                  errno);
        exitCode = ExitCode_ReadFile_Read;
    }
    close(fd);

    if (ret < sizeof(delayTimeUTC_t)) {
        return false;
    }

    return true;
}
