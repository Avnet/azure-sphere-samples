#include "rsl10.h"
#include "math.h"

// Send the telemetry message
#ifdef USE_IOT_CONNECT
#include "iotConnect.h"
#endif

// Global variables
RSL10Device_t Rsl10DeviceList[MAX_RSL10_DEVICES];
char authorizedDeviceList[MAX_RSL10_DEVICES][RSL10_ADDRESS_LEN];
int currentRsl10DeviceIndex = -1;
uint8_t numRsl10DevicesInList = 0;
char bdAddress[] = "  -  -  -  -  -  \0";
char rxRssi[] = "-xx\0";
float temperature;
float humidity;
float pressure;
uint16_t ambiantLight;

/// <summary>
///     Function to parse UART Rx messages and send to IoT Hub.
/// </summary>
/// <param name="msgToParse">The message received from the UART</param>
void parseAndSendToAzure(char *msgToParse)
{
    // This is a big ugly function, basically what it does is . . . 
    // 1. check to see if this is a advertisement message
    // 2. Pull the Address, recordNumber and flags from the message
    // 3. Make sure that the mac for this device was authorized in the device twin
    // 4. Check to see if we've already created an object for this device using the addres
    // 4.1 If not, then create one (just populate a static array)

    // Message pointer
    Rsl10Message_t *msgPtr;

    int tempRsl10Index = -1;

    Log_Debug("msgToParse lentgh: %d\n", strlen(msgToParse));

    // Check to see if this is a RSL10 Advertisement message
    if (strlen(msgToParse) > 32) {

        // Cast the message to the correct type so we can index into the string
        msgPtr = (Rsl10Message_t *)msgToParse;

        // Pull the RSL10 address from the message
        getBdAddress(bdAddress, msgPtr);


        // Check to see if this devcice's MAC address has been whitelisted
        if( !isDeviceAuthorized(bdAddress)){

            Log_Debug("Device %s , discarding message data\n", bdAddress);
            Log_Debug("To authorize the device add it's MAC address as a authorizedMac<n> in the IoTHub device twin\n");
            return;
        }

        // Determine if we know about this RSL10 using the address
        currentRsl10DeviceIndex = getRsl10DeviceIndex(bdAddress);

        // Check to see if the device was found, not then add it!
        if (currentRsl10DeviceIndex == -1) {

            // We did not find this device in our list, add it!
            tempRsl10Index = addRsl10DeviceToList(bdAddress, msgPtr);

            if (tempRsl10Index != -1) {

                currentRsl10DeviceIndex = tempRsl10Index;
                Log_Debug("Add this device as index %d\n", currentRsl10DeviceIndex);
            } else {

                // Device could not be added!
                Log_Debug("ERROR: Could not add new device\n");
            }
        }

        // Else the device was found and currentRsl10DeviceIndex now holds the index to this
        // device's struct

        // Pull the rssi number from the end of the message
        getRxRssi(rxRssi, msgPtr);
        Rsl10DeviceList[currentRsl10DeviceIndex].lastRssi = atoi(rxRssi);

        getTemperature(&temperature, msgPtr);
        Rsl10DeviceList[currentRsl10DeviceIndex].lastTemperature = temperature;

        getHumidity(&humidity, msgPtr);
        Rsl10DeviceList[currentRsl10DeviceIndex].lastHumidity = humidity;

        getPressure(&pressure, msgPtr);
        Rsl10DeviceList[currentRsl10DeviceIndex].lastPressure = pressure;

        getAmbiantLight(&ambiantLight, msgPtr);
        Rsl10DeviceList[currentRsl10DeviceIndex].lastAmbiantLight = ambiantLight;

#ifdef ENABLE_MSG_DEBUG
            Log_Debug("RSL10 Device: %s is captured in index %d\n", bdAddress, currentRsl10DeviceIndex);
            Log_Debug("RX rssi    : %s\n", rxRssi);
            Log_Debug("Temperature: %.2f\n", temperature);
            Log_Debug("Humidity: %.2f\n", humidity);
            Log_Debug("Pressure: %.2f\n", pressure);
#endif 
    }
}

int stringToInt(char *stringData, size_t stringLength)
{

    char tempString[64];
    strncpy(tempString, stringData, stringLength);
    tempString[stringLength] = '\0';
    return (int)(strtol(tempString, NULL, 16));
}

void textFromHexString(char *hex, char *result, int strLength)
{
    char temp[3];

    temp[2] = '\0';
    for (int i = 0; i < strLength; i += 2) {
        strncpy(temp, &hex[i], 2);
        *result = (char)strtol(temp, NULL, 16);
        result++;
    }
    *result = '\0';
}

// Set the global RSL10 address variable
void getBdAddress(char *bdAddress, Rsl10Message_t *rxMessage)
{
    bdAddress[0] = rxMessage->BdAddress[10];
    bdAddress[1] = rxMessage->BdAddress[11];
    bdAddress[3] = rxMessage->BdAddress[8];
    bdAddress[4] = rxMessage->BdAddress[9];
    bdAddress[6] = rxMessage->BdAddress[6];
    bdAddress[7] = rxMessage->BdAddress[7];
    bdAddress[9] = rxMessage->BdAddress[4];
    bdAddress[10] = rxMessage->BdAddress[5];
    bdAddress[12] = rxMessage->BdAddress[2];
    bdAddress[13] = rxMessage->BdAddress[3];
    bdAddress[15] = rxMessage->BdAddress[0];
    bdAddress[16] = rxMessage->BdAddress[1];
}

// Set the global rssi variable from the end of the message
void getRxRssi(char *rxRssi, Rsl10Message_t *rxMessage)
{
    // BW Fix me
    rxRssi[0] = rxMessage->rssi[0];
    rxRssi[1] = rxMessage->rssi[1];
    rxRssi[2] = rxMessage->rssi[2];
}

void getTemperature(float *temperature, Rsl10Message_t *rxMessage){

    uint16_t temp =  (uint16_t)((stringToInt(&rxMessage->temperature[2], 2) << 8) |
                                (stringToInt(&rxMessage->temperature[0], 2) << 0));
    *temperature = (float)(temp / 100.0);
    return;
}

void getHumidity(float *humidity, Rsl10Message_t *rxMessage){

    uint16_t temp =  (uint16_t)((stringToInt(&rxMessage->humidity[2], 2) << 8) |
                                (stringToInt(&rxMessage->humidity[0], 2) << 0));
    *humidity = (float)(temp / 100.0);

    return;
}
void getPressure(float *pressure, Rsl10Message_t *rxMessage){
    
    uint32_t temp =  (uint32_t)((stringToInt(&rxMessage->pressure[4], 2) << 16) |
                                (stringToInt(&rxMessage->pressure[2], 2) << 8) |
                                (stringToInt(&rxMessage->pressure[0], 2) << 0));
    *pressure = (float)(temp / 100.0);
    return;

}
void getAmbiantLight(uint16_t *ambiantLight, Rsl10Message_t *rxMessage){

    return;
}


int getRsl10DeviceIndex(char *Rsl10DeviceID)
{

    for (int8_t i = 0; i < numRsl10DevicesInList; i++) {
        if (strncmp(Rsl10DeviceList[i].bdAddress, Rsl10DeviceID, strlen(Rsl10DeviceID)) == 0) {
            return i;
        }
    }

    // If we did not find the device return -1
    return -1;
}
int addRsl10DeviceToList(char *newRsl10Address, Rsl10Message_t *newRsl10Device)
{

    // check the whitelist first!
    if( !isDeviceAuthorized(bdAddress)){
        Log_Debug("Device not authorized, not adding to list\n");
        return -1;
    }

    Log_Debug("Device IS authorized\n");

    // Check to make sure the list is not already full, if so return -1 (failure)
    if (numRsl10DevicesInList == MAX_RSL10_DEVICES) {
        return -1;
    }
    // Increment the number of devices in the list, then fill in the new slot
    numRsl10DevicesInList++;

    // Define the return value as the index into the array for the new element
    int newDeviceIndex = numRsl10DevicesInList - 1;

    // Update the structure for this device
    strncpy(Rsl10DeviceList[newDeviceIndex].bdAddress, newRsl10Address, strlen(newRsl10Address));
//    strncpy(Rsl10DeviceList[newDeviceIndex].bt510Name, deviceName, strlen(deviceName));
    Rsl10DeviceList[newDeviceIndex].lastTemperature = NAN;
    Rsl10DeviceList[newDeviceIndex].lastAmbiantLight = NAN;
    Rsl10DeviceList[newDeviceIndex].lastHumidity = NAN;
    Rsl10DeviceList[newDeviceIndex].lastPressure = NAN;
    Rsl10DeviceList[newDeviceIndex].lastRssi = NAN;

    // Send up this devices specific details to the device twin
    #define JSON_TWIN_BUFFER_SIZE 512
    char deviceTwinBuffer[JSON_TWIN_BUFFER_SIZE];

//    snprintf(deviceTwinBuffer, sizeof(deviceTwinBuffer), bt510DeviceTwinsonObject, deviceName, deviceName, deviceName, bdAddress, deviceName,
//                 firmwareVersion, deviceName, bootloaderVersion);
//    TwinReportState(deviceTwinBuffer);

    Log_Debug("Add new device to list at index %d!\n", newDeviceIndex);

    // Return the index into the array where we added the new device
    return newDeviceIndex;
}



// Check to see if the devices MAC has been authorized
bool isDeviceAuthorized(char* deviceToCheck){

    for(int i = 0; i < MAX_RSL10_DEVICES; i++){
        
        if(strncmp(&authorizedDeviceList[i][0], deviceToCheck, RSL10_ADDRESS_LEN) == 0){
            return true;
        }
    }
    return false;
}

void rsl10SendTelemetry(void){

    /*

    // Define a flag to see if we found new telemetry data to send
    bool updatedValuesFound = false;

    // Define a flag to use for each device to determine if we need to append the rssi telemetry
    bool deviceWasUpdated = false;

    // Dynamically build the telemetry JSON document to handle from 1 to 10 BT510 devices.  
    // For example the JSON for 1 BT510 . . . 
    // {"tempDev1":24.3, "batDev1": 3.23, "rssiDev1": -71}
    //
    // The JSON for two BT510s . . .
    // {23.3,"tempDev1":24.3, "batDev1": 3.23, "tempDev2":24.3, "batDev2": 3.23, "rssiDev1": -70, , "rssiDev2": -65}
    // Where Dev1 and Dev2 are dynamic names pulled from each BT510s advertised name
    
    // If we don't have any devices in the list, then bail.  Nothing to see here, move along . . . 
    if(numBT510DevicesInList == 0){
        return;
    }

    // Allocate enough memory to hold the dynamic JSON document, we know how large the object is, but we need to add additional
    // memory for the device name and the data that we'll be adding to the telemetry message
    char *telemetryBuffer = calloc((size_t)(numBT510DevicesInList * (               // Multiply the size for one device by the number of devices we have
                                   (size_t)strlen(bt510TemperatureJsonObject) +     // Size of the temperature json template
                                   (size_t)strlen(bt510BatteryJsonObject) +         // Size of the battery json template
                                   (size_t)strlen(bt510RssiJsonObject) +            // Size of the rssi json template
                                   (size_t)32 +                                     // Allow for the temperature and battery data (the numbers)
                                   (size_t)MAX_NAME_LENGTH)),                       // Allow for the device name i.e., "Basement + Coach"
                                   sizeof(char));

    // Verify we got the memory requested
    if(telemetryBuffer == NULL){
        exitCode = ExitCode_Init_TelemetryCallocFailed;
        return;
    }

    // Declare an array that we use to construct each of the different telemetry parts, we populate this string then add it to the dynamic string
    char newTelemetryString[16 + MAX_NAME_LENGTH];

    // Start to build the dynamic telemetry message.  This first string contains the opening '{'
    newTelemetryString[0] = '{';
    // Add it to the telemetry message
    strcat(telemetryBuffer,newTelemetryString);

    for(int i = 0; i < numBT510DevicesInList; i++){
        
        // Add temperature data for the current device, if it's been updated
        if(!isnan(BT510DeviceList[i].lastTemperature)){

            snprintf(newTelemetryString, sizeof(newTelemetryString), bt510TemperatureJsonObject, 
                                                                     BT510DeviceList[i].bt510Name, 
                                                                     BT510DeviceList[i].lastTemperature);
            // Add it to the telemetry message
            strcat(telemetryBuffer,newTelemetryString);

            // Mark the temperature variable with the NAN value so we can determine if it gets updated
            BT510DeviceList[i].lastTemperature = NAN;

            // Set the flag that tells the logic to send the message to Azure
            updatedValuesFound = true;
            deviceWasUpdated = true;

        }

        // Add battery data for the current device, if it's been updated
        if(!isnan(BT510DeviceList[i].lastBattery)){

            snprintf(newTelemetryString, sizeof(newTelemetryString), bt510BatteryJsonObject, 
                                                                     BT510DeviceList[i].bt510Name, 
                                                                     BT510DeviceList[i].lastBattery);
            // Add it to the telemetry message
            strcat(telemetryBuffer,newTelemetryString);

            // Mark the battery variable with the NAN value so we can determine if it gets updated
            BT510DeviceList[i].lastBattery = NAN;

            // Set the flag that tells the logic to send the message to Azure
            updatedValuesFound = true;
            deviceWasUpdated = true;

        }

        // If we found updated values to send, then tack on the current rssi reading
        if(deviceWasUpdated){

            snprintf(newTelemetryString, sizeof(newTelemetryString), bt510RssiJsonObject, 
                                                                     BT510DeviceList[i].bt510Name, 
                                                                     BT510DeviceList[i].lastRssi);
            // Add it to the telemetry message
            strcat(telemetryBuffer,newTelemetryString);
            
            // Clear the flag, we only want to send the rssi if we've received an update, otherwise it's old data
            deviceWasUpdated = false;
        }
    }

    // Find the last location of the constructed string (will contain the last ',' char), and overwrite it with a closing '}'
    telemetryBuffer[strlen(telemetryBuffer)-1] = '}';
    
    // Null terminate the string
    telemetryBuffer[strlen(telemetryBuffer)] = '\0';

    if(updatedValuesFound){
        
        Log_Debug("Telemetry message: %s\n", telemetryBuffer);
        // Send the telemetry message
        SendTelemetry(telemetryBuffer);
    }
    else{
        Log_Debug("No new data found, not sending telemetry update\n");
    }

    free(telemetryBuffer);
    */
}