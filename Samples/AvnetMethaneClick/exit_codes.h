#ifndef _EXIT_CODES_H
#define _EXIT_CODES_H

/// <summary>
/// Exit codes for this application. These are used for the
/// application exit code. They must all be between zero and 255,
/// where zero is reserved for successful termination.
/// </summary>
typedef enum {
    ExitCode_Success = 0,

    ExitCode_TermHandler_SigTerm = 1,

    ExitCode_Main_EventLoopFail = 2,

    ExitCode_ButtonTimer_Consume = 3,

    ExitCode_AzureTimer_Consume = 4,

    ExitCode_Init_EventLoop = 5,
    ExitCode_Init_ButtonA = 6,
    ExitCode_Init_ButtonB = 7,
    ExitCode_Init_ButtonPollTimer = 8,
    ExitCode_Init_AzureTimer = 9,

    ExitCode_IsButtonPressed_GetValue = 10,

    ExitCode_Validate_ConnectionType = 11,
    ExitCode_Validate_ScopeId = 12,
    ExitCode_Validate_Hostname = 13,
    ExitCode_Validate_IoTEdgeCAPath = 14,
    ExitCode_InterfaceConnectionStatus_Failed = 15,

    ExitCode_IoTEdgeRootCa_Open_Failed = 16,
    ExitCode_IoTEdgeRootCa_LSeek_Failed = 17,
    ExitCode_IoTEdgeRootCa_FileSize_Invalid = 18,
    ExitCode_IoTEdgeRootCa_FileSize_TooLarge = 19,
    ExitCode_IoTEdgeRootCa_FileRead_Failed = 20,

    ExitCode_PayloadSize_TooLarge = 21,
    ExitCode_Init_sensorPollTimer = 22,

    ExitCode_SetGPIO_Failed = 23,
    ExitCode_Button_Telemetry_Malloc_Failed = 24,
    ExitCode_RebootDevice_Malloc_failed = 25,
    ExitCode_SetPollTime_Malloc_failed = 26,
    ExitCode_NoMethodFound_Malloc_failed = 27,
    ExitCode_DirectMethod_InvalidPayload_Malloc_failed = 28,
    ExitCode_DirectMethod_RebootExectued = 29,
    ExitCode_Init_OledUpdateTimer = 30,

    // IoTConnect exit codes
    ExitCode_IoTCTimer_Consume = 31,
    ExitCode_Init_IoTCTimer = 32,
    ExitCode_IoTCMalloc_Failed = 33,

    // Reboot exit codes
    ExitCode_TriggerReboot_Success = 34,
    ExitCode_UpdateCallback_Reboot = 35,

    // ADC Methanne Click codes
    ExitCode_Init_AdcOpen = 36,
    ExitCode_Init_GetBitCount = 37,
    ExitCode_Init_UnexpectedBitCount = 38,
    ExitCode_Init_SetRefVoltage = 39,
    ExitCode_Init_AdcPollTimer = 40,
    ExitCode_AdcTimerHandler_Poll = 41


} ExitCode;



#endif 