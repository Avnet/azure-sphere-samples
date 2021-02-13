#include "rsl10.h"
#include "math.h"

// Send the telemetry message
#ifdef USE_IOT_CONNECT
#include "iotConnect.h"
#endif

// Global variables
char authorizedDeviceList[MAX_RSL10_DEVICES][RSL10_ADDRESS_LEN];
RSL10Device_t Rsl10DeviceList[MAX_RSL10_DEVICES];
int8_t currentRsl10DeviceIndex = -1;
int8_t numRsl10DevicesInList = 0;
char bdAddress[] = "  :  :  :  :  :  \0";

/// <summary>
///     Function to parse UART Rx messages and update global structures
/// </summary>
/// <param name="msgToParse">The message received from the UART</param>
void parseRsl10Message(char *msgToParse)
{

    #define MIN_RSL10_MSG_LENGTH 25

    // Do a sanity check to make sure that the message is large enough to be a valid message.
    // The battery message is the smallest message we expect, if this message is smaller than that,
    // then exit without doing any processing.
    if(strlen(msgToParse) < MIN_RSL10_MSG_LENGTH){
        Log_Debug("RSL10 message is not valid, message length = %d, minimum valid length is %d.\n", strlen(msgToParse), MIN_RSL10_MSG_LENGTH);
        return;
    }

    // Variable to hold the message identifier "ESD", "MSD" or "BAT"
    char messageID[3];

    // Generic message pointer for message ID and BdAddress
    RSL10MessageHeader_t *msgPtr;
    msgPtr = (RSL10MessageHeader_t*)msgToParse;

    // Index into device list for this message
    int8_t  tempRsl10Index = -1;

    // Pull the RSL10 ID from the message
    getBdMessageID(messageID, msgPtr);

    // Pull the RSL10 address from the message
    getBdAddress(bdAddress, msgPtr);

    // Check to see if this devcice's MAC address has been whitelisted
    if( !isDeviceAuthorized(bdAddress)){

        Log_Debug("Device %s is not authorized, discarding message data\n", bdAddress);
        Log_Debug("To authorize the device add it's MAC address as a authorizedMac<n> in the IoTHub device twin\n");
        return;
    }

    // Determine if we know about this RSL10 using the address
    currentRsl10DeviceIndex = getRsl10DeviceIndex(bdAddress);

    // Check to see if the device was found, not then add it!
    if (currentRsl10DeviceIndex == -1) {

        // We did not find this device in our list, add it!
        tempRsl10Index = addRsl10DeviceToList(bdAddress);

        if (tempRsl10Index != -1) {

            currentRsl10DeviceIndex = tempRsl10Index;
            Log_Debug("Add this device as index %d\n", currentRsl10DeviceIndex);
        } else {

            // Device could not be added!
            Log_Debug("ERROR: Could not add new device\n");
            return;
        }
    }

    // Next determine which message we received and call the appropriate rouitine to pull data 
    // from the message and copy that data to this RSL10's data structure
    
    // Is this a Movement message?
    if (strcmp(messageID, "MSD") == 0) {
        rsl10ProcessMovementMessage(msgToParse, currentRsl10DeviceIndex);
    } 

    // Is this a Environmental message?
    else if (strcmp(messageID, "ESD") == 0) {
        rsl10ProcessEnvironmentalMessage(msgToParse, currentRsl10DeviceIndex);
    }

    // Is this a Battery message?
    else if (strcmp(messageID, "BAT") == 0) {
        rsl10ProcessBatteryMessage(msgToParse, currentRsl10DeviceIndex);
    }

    else{
        Log_Debug("Unknown message ID\n");
    }
    return;
}

// Worker routine to convert a string to an integer
int stringToInt(char *stringData, size_t stringLength)
{

    char tempString[64];
    strncpy(tempString, stringData, stringLength);
    tempString[stringLength] = '\0';
    return (int)(strtol(tempString, NULL, 16));
}

// Worker routine to convert hex data to it's string representation
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

// Process a RSL10 Movement message
void rsl10ProcessMovementMessage(char* rxMessage, int8_t currentRsl10DeviceIndex)
{

    // Cast the pointer to reference this messages data structure
    Rsl10MotionMessage_t* msgPtr = (Rsl10MotionMessage_t*) rxMessage;

    // Call the routines to pull the data from the message.  This devices global structure is updated
    // by each routine.
    getRxRssi(&Rsl10DeviceList[currentRsl10DeviceIndex].lastRssi, &msgPtr->rssi[0]); 
    getSensorSettings(&Rsl10DeviceList[currentRsl10DeviceIndex], msgPtr);
    getAccelReadings(&Rsl10DeviceList[currentRsl10DeviceIndex], msgPtr);
    getOrientation(&Rsl10DeviceList[currentRsl10DeviceIndex], msgPtr);

    // Set the flag so we know that we have fresh data to send to IoTConnect
    Rsl10DeviceList[currentRsl10DeviceIndex].movementDataRefreshed = true;

#ifdef ENABLE_MSG_DEBUG
    Log_Debug("Rssi: %d\n", Rsl10DeviceList[currentRsl10DeviceIndex].lastRssi);
    Log_Debug("accel: %.4f, %.4f, %.4f\n", Rsl10DeviceList[currentRsl10DeviceIndex].lastAccel_raw_x, 
                                        Rsl10DeviceList[currentRsl10DeviceIndex].lastAccel_raw_y, 
                                        Rsl10DeviceList[currentRsl10DeviceIndex].lastAccel_raw_z);
    Log_Debug("Orientation: %.4f, %.4f, %.4f, %.4f\n", Rsl10DeviceList[currentRsl10DeviceIndex].lastOrientation_x, 
                                                       Rsl10DeviceList[currentRsl10DeviceIndex].lastOrientation_y,      
                                                       Rsl10DeviceList[currentRsl10DeviceIndex].lastOrientation_z,      
                                                       Rsl10DeviceList[currentRsl10DeviceIndex].lastOrientation_w);      
#endif 
}

// Process a RSL10 Environmental message
void rsl10ProcessEnvironmentalMessage(char* rxMessage, int8_t currentRsl10DeviceIndex)
{

    // Cast the pointer to reference this messages data structure
    Rsl10EnvironmentalMessage_t* msgPtr = (Rsl10EnvironmentalMessage_t*) rxMessage;

    // Call the routines to pull the data from the message.  This devices global structure is updated
    // by each routine.
    getRxRssi(&Rsl10DeviceList[currentRsl10DeviceIndex].lastRssi, msgPtr->rssi);
    getTemperature(&Rsl10DeviceList[currentRsl10DeviceIndex].lastTemperature, msgPtr);
    getHumidity(&Rsl10DeviceList[currentRsl10DeviceIndex].lastHumidity, msgPtr);
    getPressure(&Rsl10DeviceList[currentRsl10DeviceIndex].lastPressure, msgPtr);
    getAmbiantLight(&Rsl10DeviceList[currentRsl10DeviceIndex].lastAmbiantLight, msgPtr);

    // Set the flag so we know that we have fresh data to send to IoTConnect
    Rsl10DeviceList[currentRsl10DeviceIndex].environmentalDataRefreshed = true;

#ifdef ENABLE_MSG_DEBUG
            Log_Debug("RX rssi    : %d\n", Rsl10DeviceList[currentRsl10DeviceIndex].lastRssi);
            Log_Debug("Temperature: %.2f\n", Rsl10DeviceList[currentRsl10DeviceIndex].lastTemperature);
            Log_Debug("Humidity   : %.2f\n", Rsl10DeviceList[currentRsl10DeviceIndex].lastHumidity);
            Log_Debug("Pressure   : %.2f\n", Rsl10DeviceList[currentRsl10DeviceIndex].lastPressure);
#endif 

}

// Process a RSL10 Battery message
void rsl10ProcessBatteryMessage(char* rxMessage, int8_t currentRsl10DeviceIndex)
{

    // Cast the pointer to reference this messages data structure
    Rsl10BatteryMessage_t* msgPtr = (Rsl10BatteryMessage_t*) rxMessage;

    // Call the routines to pull the data from the message.  This devices global structure is updated
    // by each routine.
    getRxRssi(&Rsl10DeviceList[currentRsl10DeviceIndex].lastRssi, &msgPtr->rssi[0]);
    getBattery(&Rsl10DeviceList[currentRsl10DeviceIndex].lastBattery, msgPtr);

    // Set the flag so we know that we have fresh data to send to IoTConnect
    Rsl10DeviceList[currentRsl10DeviceIndex].batteryDataRefreshed = true;


#ifdef ENABLE_MSG_DEBUG
    Log_Debug("RX rssi    : %d\n", Rsl10DeviceList[currentRsl10DeviceIndex].lastRssi);
    Log_Debug("Battery    : %.2f V\n", Rsl10DeviceList[currentRsl10DeviceIndex].lastBattery);
#endif 
}


void getBdMessageID(char *messageID, RSL10MessageHeader_t *rxMessage){

    messageID[0] = rxMessage->msgSendRxId[0];
    messageID[1] = rxMessage->msgSendRxId[1];
    messageID[2] = rxMessage->msgSendRxId[2];
    messageID[3] = '\0';
}

// Set the global RSL10 address variable
void getBdAddress(char *bdAddress, RSL10MessageHeader_t *rxMessage)
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
void getRxRssi(int16_t* rssiVariable, char *rxMessage)
{
    char tempRssi[3];
    tempRssi[0] = rxMessage[0];
    tempRssi[1] = rxMessage[1];
    tempRssi[2] = rxMessage[2];
    *rssiVariable  = (int16_t)atoi(tempRssi);
}

void getTemperature(float *temperature, Rsl10EnvironmentalMessage_t *rxMessage){

    uint16_t temp =  (uint16_t)((stringToInt(&rxMessage->temperature[2], 2) << 8) |
                                (stringToInt(&rxMessage->temperature[0], 2) << 0));
    *temperature = (float)(temp / 100.0);
    return;
}

void getHumidity(float *humidity, Rsl10EnvironmentalMessage_t *rxMessage){

    uint16_t temp =  (uint16_t)((stringToInt(&rxMessage->humidity[2], 2) << 8) |
                                (stringToInt(&rxMessage->humidity[0], 2) << 0));
    *humidity = (float)(temp / 100.0);

    return;
}
void getPressure(float *pressure, Rsl10EnvironmentalMessage_t *rxMessage){
    
    uint32_t temp =  (uint32_t)((stringToInt(&rxMessage->pressure[4], 2) << 16) |
                                (stringToInt(&rxMessage->pressure[2], 2) << 8) |
                                (stringToInt(&rxMessage->pressure[0], 2) << 0));
    *pressure = (float)(temp / 100.0);
    return;

}
void getAmbiantLight(uint16_t *ambiantLight, Rsl10EnvironmentalMessage_t *rxMessage){

    return;
}


int8_t getRsl10DeviceIndex(char *Rsl10DeviceID)
{

    for (int8_t i = 0; i < numRsl10DevicesInList; i++) {
        if (strncmp(Rsl10DeviceList[i].bdAddress, Rsl10DeviceID, strlen(Rsl10DeviceID)) == 0) {
            return i;
        }
    }

    // If we did not find the device return -1
    return -1;
}

void getBattery(float *battery, Rsl10BatteryMessage_t *rxMessage){

    // Read the voltage level and covert it to Volts
    *battery =  (float)(((stringToInt(&rxMessage->battery[0], 2) << 8) |
                        (stringToInt(&rxMessage->battery[2], 2) << 0)))/1000;
}

void getSensorSettings(RSL10Device_t* currentDevPtr, Rsl10MotionMessage_t* rxMessage){

    uint8_t sensorSettings = 0;
    sensorSettings = (uint8_t)stringToInt((char*)&rxMessage->SensorSetting, 2);
    currentDevPtr->lastsampleRate = sensorSettings >> 4 & 0x0F;
    currentDevPtr->lastAccelRange = sensorSettings >> 2 & 0x03;
    currentDevPtr->lastDataType = sensorSettings & 0x03;
}

void getAccelReadings(RSL10Device_t* currentDevPtr, Rsl10MotionMessage_t* rxMessage){

    #define RAW_TO_MPS_SQUARED 32768*9.81f
    #define MPS_SQUARED_TO_G 0.102f

    int16_t rawAccel;

    // Read and calculate the x component
    rawAccel =  (int16_t)(stringToInt(&rxMessage->accel_raw_x[2], 2) << 8) | (int16_t)(stringToInt(&rxMessage->accel_raw_x[0], 2) << 0);
    currentDevPtr->lastAccel_raw_x = (float)rawAccel/RAW_TO_MPS_SQUARED*(currentDevPtr->lastAccelRange*4)*MPS_SQUARED_TO_G;

    // Read and calculate the y component
    rawAccel =  (int16_t)(stringToInt(&rxMessage->accel_raw_y[2], 2) << 8) | (int16_t)(stringToInt(&rxMessage->accel_raw_y[0], 2) << 0);
    currentDevPtr->lastAccel_raw_y = (float)rawAccel/RAW_TO_MPS_SQUARED*(currentDevPtr->lastAccelRange*4)*MPS_SQUARED_TO_G;

    // Read and calculate the z component
    rawAccel =  (int16_t)(stringToInt(&rxMessage->accel_raw_z[2], 2) << 8) | (int16_t)(stringToInt(&rxMessage->accel_raw_z[0], 2) << 0);
    currentDevPtr->lastAccel_raw_z = (float)rawAccel/RAW_TO_MPS_SQUARED*(currentDevPtr->lastAccelRange*4)*MPS_SQUARED_TO_G;
}

void getOrientation(RSL10Device_t* currentDevPtr, Rsl10MotionMessage_t* rxMessage){

    #define ORIENTATION_DIVISOR 128.0f

    // Read and calculate the x component
    currentDevPtr->lastOrientation_x = (float)((int8_t)(stringToInt(&rxMessage->orientation_x[0], 2) << 0))/ORIENTATION_DIVISOR;

    // Read and calculate the y component
    currentDevPtr->lastOrientation_y = (float)((int8_t)(stringToInt(&rxMessage->orientation_y[0], 2) << 0))/ORIENTATION_DIVISOR;

    // Read and calculate the z component
    currentDevPtr->lastOrientation_z = (float)((int8_t)(stringToInt(&rxMessage->orientation_z[0], 2) << 0))/ORIENTATION_DIVISOR;

    // Read and calculate the z component
    currentDevPtr->lastOrientation_w = (float)((int8_t)(stringToInt(&rxMessage->orientation_w[0], 2) << 0))/ORIENTATION_DIVISOR;
}

int8_t  addRsl10DeviceToList(char *newRsl10Address)
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
    int8_t newDeviceIndex = numRsl10DevicesInList - (int8_t)1;

    // Update the structure for this device
    strncpy(Rsl10DeviceList[newDeviceIndex].bdAddress, newRsl10Address, strlen(newRsl10Address));

    // Clear the flags that we use to know if we have fresh data to send up as telemetry
    Rsl10DeviceList[newDeviceIndex].movementDataRefreshed = false;
    Rsl10DeviceList[newDeviceIndex].environmentalDataRefreshed = false;
    Rsl10DeviceList[newDeviceIndex].batteryDataRefreshed = false;

    // If we need to add any process when we receive the first message from the device, then add it here
    Log_Debug("Add new device to list at index %d!\n", newDeviceIndex);

    // Return the index into the array where we added the new device
    return newDeviceIndex;
}

// Check to see if the devices MAC has been authorized
bool isDeviceAuthorized(char* deviceToCheck){

    // For now, authorize all devices.  This is a limitation of the current IoTConect model
    return true;

/*
    for(int i = 0; i < MAX_RSL10_DEVICES; i++){
        if(strncmp(&authorizedDeviceList[i][0], deviceToCheck, RSL10_ADDRESS_LEN) == 0){
            return true;
        }
    }
    return false;
*/    
}

void rsl10SendTelemetry(void) {
    
    // Assume we'll be sending a message to Azure and allocate a buffer
    #define JSON_BUFFER_SIZE 256
    char telemetryBuffer[JSON_BUFFER_SIZE];

    // Iterate over all connected RSL10 devices to send telemetry
    for(int currentDevice = 0; currentDevice < numRsl10DevicesInList; currentDevice++){

        // Check to see if the current device has fresh motion data, if so send the telemetry
        if(Rsl10DeviceList[currentDevice].movementDataRefreshed){

            // Define the Json string format for movement messages, the
            // actual telemetry data is inserted as the last string argument
            static const char Rsl10MotionTelemetryJson[] =
                "{\"RSL10Sensors\":{\"address\":\"%s\",\"rssi\":%d,\"acc_x\":%0.4f,\"acc_y\":%0.4f,\"acc_z\":%0.4f,\"orient_x\":%0.4f,\"orient_y\":%0.4f,\"orient_z\":%0.4f,\"orient_w\":%0.4f}}";

            snprintf(telemetryBuffer, sizeof(telemetryBuffer), Rsl10MotionTelemetryJson,
                                                               Rsl10DeviceList[currentDevice].bdAddress,
                                                               Rsl10DeviceList[currentDevice].lastRssi,
                                                               Rsl10DeviceList[currentDevice].lastAccel_raw_x,
                                                               Rsl10DeviceList[currentDevice].lastAccel_raw_y,
                                                               Rsl10DeviceList[currentDevice].lastAccel_raw_z,
                                                               Rsl10DeviceList[currentDevice].lastOrientation_x,
                                                               Rsl10DeviceList[currentDevice].lastOrientation_y,
                                                               Rsl10DeviceList[currentDevice].lastOrientation_z,
                                                               Rsl10DeviceList[currentDevice].lastOrientation_w);
            // Send the telemetry message
            SendTelemetry(telemetryBuffer, true);

            // Clear the flag so we don't send this data again
            Rsl10DeviceList[currentDevice].movementDataRefreshed = false;

        }
        // Check to see if the current device has fresh environmental data, if so send the telemetry
        if (Rsl10DeviceList[currentDevice].environmentalDataRefreshed){

            // actual telemetry data is inserted as the last string argument
            static const char Rsl10EnvironmentalTelemetryJson[] =
                "{\"RSL10Sensors\":{\"address\":\"%s\",\"rssi\":%d,\"temperature\":%0.2f,\"humidity\": %0.2f,\"pressure\": %0.2f, \"light\": %d}}";

            snprintf(telemetryBuffer, sizeof(telemetryBuffer), Rsl10EnvironmentalTelemetryJson,
                                                               Rsl10DeviceList[currentDevice].bdAddress,
                                                               Rsl10DeviceList[currentDevice].lastRssi,
                                                               Rsl10DeviceList[currentDevice].lastTemperature,
                                                               Rsl10DeviceList[currentDevice].lastHumidity,
                                                               Rsl10DeviceList[currentDevice].lastPressure,
                                                               Rsl10DeviceList[currentDevice].lastAmbiantLight);
            // Send the telemetry message
            SendTelemetry(telemetryBuffer, true);

            // Clear the flag so we don't send this data again
            Rsl10DeviceList[currentDevice].movementDataRefreshed = false;


            // Clear the flag so we don't send this data again
            Rsl10DeviceList[currentDevice].environmentalDataRefreshed = false;
        }
        // Check to see if the current device has fresh battery data, if so send the telemetry
        if (Rsl10DeviceList[currentDevice].batteryDataRefreshed){

            // Define the Json string format for battery messages, the
            // actual telemetry data is inserted as the last string argument
            static const char Rsl10BatteryTelemetryJson[] = "{\"RSL10Sensors\":{\"address\":\"%s\",\"rssi\":%d,\"battery\":%0.2f}}";

            snprintf(telemetryBuffer, sizeof(telemetryBuffer), Rsl10BatteryTelemetryJson,
                                                               Rsl10DeviceList[currentDevice].bdAddress,
                                                               Rsl10DeviceList[currentDevice].lastRssi,
                                                               Rsl10DeviceList[currentDevice].lastBattery);
            // Send the telemetry message
            SendTelemetry(telemetryBuffer, true);



            // Clear the flag so we don't send this data again
            Rsl10DeviceList[currentDevice].batteryDataRefreshed = false;

        }
     }
}
