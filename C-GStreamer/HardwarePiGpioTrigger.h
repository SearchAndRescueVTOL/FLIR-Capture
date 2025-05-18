#ifndef HARDWARE_PI_GPIO_TRIGGER_H
#define HARDWARE_PI_GPIO_TRIGGER_H

#define _GNU_SOURCE

#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <signal.h>
#include <sched.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <gpiod.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "log.h"
#include <mosquitto.h>

// Constants
#define OUTPUT_FILE_NAME "output.txt"
#define GPIO_CHIP "/dev/gpiochip4"
#define GPIO_LINE 26
#define TIMEOUT_SEC 5
// #define MQTT_HOST "localhost"
// #define MQTT_PORT 1883
// #define MQTT_TOPIC "FLIR/logs"

// Global variables
GstElement *appsink;
FILE *fd;
long long trigger_counter;
double time_in_seconds;
char session_dir[256];
FILE* logfile;
// struct mosquitto *mosq;
// int rc;
// Function declarations
void set_cpu_affinity(int core_id);
void capture_frame();
void *handle_gpio_interrupt(void *arg);
// int log_to_mosq(char *msg);

#endif // HARDWARE_PI_GPIO_TRIGGER_H
