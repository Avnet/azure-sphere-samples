/* Copyright (c) Microsoft Corporation. All rights reserved.
   Licensed under the MIT License. */

   // This minimal Azure Sphere app repeatedly toggles GPIO 8, which is the red channel of RGB
   // LED 1 on the MT3620 RDB. Use this app to test that device and SDK installation succeeded
   // that you can build, deploy, and debug a CMake app with Visual Studio.
   //
   // It uses the API for the following Azure Sphere application libraries:
   // - gpio (digital input for button)
   // - log (messages shown in Visual Studio's Device Output window during debugging)

#include <stdbool.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <stdio.h>
#include <math.h>
#include <unistd.h>
#include <stdlib.h>

#include <applibs/log.h>
#include <applibs/gpio.h>

// By default, this sample targets hardware that follows the MT3620 Reference
// Development Board (RDB) specification, such as the MT3620 Dev Kit from
// Seeed Studio.
//
// To target different hardware, you'll need to update CMakeLists.txt. See
// https://github.com/Azure/azure-sphere-samples/tree/master/Hardware for more details.
//
// This #include imports the sample_hardware abstraction from that hardware definition.
#include <hw/sample_hardware.h>

/// <summary>
/// Exit codes for this application. These are used for the
/// application exit code. They must all be between zero and 255,
/// where zero is reserved for successful termination.
/// </summary>
typedef enum {
    ExitCode_Success = 0,

    ExitCode_Main_Led = 1
} ExitCode;

#define MAX 50
#define BUFLEN 64
#define MAX_COUNT 15
#define NUMTHREAD 4      /* number of threads */

void* consumer(int* id);
void* producer(int* id);
void* blinker(int* id);

char buffer[BUFLEN];
char source[BUFLEN];
int rCount = 0, wCount = 0;
int buflen;
pthread_mutex_t count_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t nonEmpty = PTHREAD_COND_INITIALIZER;
pthread_cond_t full = PTHREAD_COND_INITIALIZER;
int thread_id[NUMTHREAD] = { 0,1,2,3 };
int i = 0, j = 0;
int fd = -1;

int main(void)
{
    Log_Debug("Starting CMake Hello World application...\n");

    fd = GPIO_OpenAsOutput(SAMPLE_LED, GPIO_OutputMode_PushPull, GPIO_Value_High);
    if (fd == -1) {
        Log_Debug(
            "Error opening GPIO: %s (%d). Check that app_manifest.json includes the GPIO used.\n",
            strerror(errno), errno);
        return ExitCode_Main_Led;
    }

    int i;
    /* define the type to be pthread */
    pthread_t thread[NUMTHREAD];

    strcpy(source, "using pthreads in Azure Sphere!");
    buflen = (int)strlen(source);

    /* create 4 threads*/
    /* create one consumer and two producers and a thread the blink the LED */
    pthread_create(&thread[0], NULL, (void*)consumer, &thread_id[0]);
    pthread_create(&thread[1], NULL, (void*)producer, &thread_id[1]);
    pthread_create(&thread[2], NULL, (void*)producer, &thread_id[2]);
    pthread_create(&thread[2], NULL, (void*)blinker, &thread_id[3]);

    for (i = 0; i < NUMTHREAD; i++)
    {
        pthread_join(thread[i], NULL);
    }
}

void* blinker(int* id) {

    const struct timespec sleepTime = { .tv_sec = 1, .tv_nsec = 0 };
    while (true) {
        GPIO_SetValue(fd, GPIO_Value_Low);
        nanosleep(&sleepTime, NULL);
        GPIO_SetValue(fd, GPIO_Value_High);
        nanosleep(&sleepTime, NULL);
    }

}

void* consumer(int* id)
{
    /* lock the variable */
    pthread_mutex_lock(&count_mutex);

    while (j < MAX)
    {
        /* wait for the buffer to have something in it */
        pthread_cond_wait(&nonEmpty, &count_mutex);

        /* take the char from the buffer and increment the rCount */
        Log_Debug("          consumed value is :%c: by %d\n", buffer[rCount], *id);
        rCount = (rCount + 1) % BUFLEN;
        fflush(stdout);
        j++;

        if (j < (MAX - 2))
            /* Last sleep might leave the condition un-processed.
             * So we prohibit sleep towards the end
             */
            if (rand() % 100 < 30)
                sleep(rand() % 3);

    }
    /* signal the producer that the buffer has been consumed */
    /* pthread_cond_signal(&full); */
    /*unlock the variable*/
    pthread_mutex_unlock(&count_mutex);
}

void* producer(int* id)
{

    while (i < MAX)
    {
        /* lock the variable */
        pthread_mutex_lock(&count_mutex);
        /* wait for the buffer to have space */
        /* pthread_cond_wait(&full, &count_mutex); */
        strcpy(buffer, "");
        buffer[wCount] = source[wCount % buflen];
        Log_Debug("%d produced :%c: by  :%d:\n", i, buffer[wCount], *id);
        fflush(stdout);
        wCount = (wCount + 1) % BUFLEN;
        i++;
        /* for the condition notify the thread */
        pthread_cond_signal(&nonEmpty);
        /*unlock the variable*/
        pthread_mutex_unlock(&count_mutex);

        if (i < (MAX - 2))
            /* Last sleep might leave the condition un-processed.
             * So we prohibit sleep towards the end
             */

            if (rand() % 100 >= 30)
                sleep(rand() % 3);

    }
}