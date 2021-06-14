#ifndef DEFERRED_UPDATES_H
#define DEFERRED_UPDATES_H

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

#include "../common/exitcodes.h"
#include <stdbool.h>
#include <stddef.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <stdio.h>
#include <ctype.h>

#include <sys/timerfd.h>

#include "uart_support.h"
#include "device_twin.h"
#include "../common/applibs_versions.h"
#include "../common/eventloop_timer_utilities.h"
#include "../common/cloud.h"
#include <applibs/log.h>
#include <applibs/gpio.h>
#include <applibs/eventloop.h>
#include <applibs/sysevent.h>

// Define the structre we use to write/read to/from mutable storage
typedef struct {
    int OTATargetUtcHour;
    int OTATargetUtcMinute;
    bool ACCEPTOtaUpdate;
} delayTimeUTC_t;

// Variables to hold a target UTC time to apply updates.  Initialize to invalid values.
//extern char otaTargetUtcTime[];

ExitCode deferredOtaUpdate_Init(void);
void deferredOtaUpdate_Cleanup(void);
ExitCode WaitForSigTerm(time_t);

// Functions to manage OTA udates at runtime
void delayOtaUpdates(uint16_t pausePeriod);
void allowOtaUpdates(void);

// Functions to poll current OTA status
bool OtaUpdateIsInProgress(void);
bool OtaUpdateIsPending(void);

// Device Twin handler
void setOtaTargetUtcTime(void* thisTwinPtr, JSON_Object *desiredProperties);

#endif // DEFERRED_UPDATES_H