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

#define OUTPUT_FILE_NAME "output.txt"
#define GPIO_CHIP "/dev/gpiochip4"
#define GPIO_LINE 17  // BCM pin for GPIO17
#define TIMEOUT_SEC 5

GstElement *appsink = NULL;
GstElement *pipeline;
FILE *fd;
long long trigger_counter = 0;
double time_in_seconds = 0;
struct timespec ts2;
void set_cpu_affinity(int core_id) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset); // Assign thread to core_id
    if (pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t),&cpuset) != 0) {
      printf("Error getting affinity!\n");
    }
}
void capture_frame() {
    GstSample *sample = gst_app_sink_pull_sample(GST_APP_SINK(appsink));
    if (!sample) {
        g_printerr("Failed to capture sample\n");
        return;
    }
    
    // // Get current time with clock_gettime
    // if (clock_gettime(CLOCK_REALTIME, &ts2) == -1) {
    //     perror("clock_gettime");
    //     return;
    // }

    // // Convert to decimal seconds
    // double time2 = ts2.tv_sec + ts2.tv_nsec / 1e9;
    GstBuffer *buffer = gst_sample_get_buffer(sample);
    
    GstMapInfo map;
    if (gst_buffer_map(buffer, &map, GST_MAP_READ)) {
        char filename[20];
        snprintf(filename, sizeof(filename), "capture%lld.raw", trigger_counter);
        GstClockTime pts = GST_BUFFER_PTS(buffer);
        double pts_seconds = (double)pts / GST_SECOND;
        fprintf(fd, "%f \n", pts_seconds - time_in_seconds);
        fflush(fd);

        // FILE *out = fopen(filename, "wb");
        // fwrite(map.data, 1, map.size, out);
        // fclose(out);

        g_print("Frame captured to capture.raw (%zu bytes)\n", map.size);
        gst_buffer_unmap(buffer, &map);
    }

    gst_sample_unref(sample);
    trigger_counter += 1;
}

void *handle_gpio_interrupt(void *arg) {
    struct gpiod_line *line = (struct gpiod_line *)arg;
    struct gpiod_line_event event;
    struct timespec timeout;
    struct timespec ts;
    set_cpu_affinity(3);
    while (1) {
        timeout.tv_sec = TIMEOUT_SEC;
        timeout.tv_nsec = 0;

        int ret = gpiod_line_event_wait(line, &timeout);
        if (ret < 0) {
            perror("Error waiting for GPIO event");
            break;
        } else if (ret == 0) {
            continue;
        }

        timespec_get(&ts, TIME_UTC);
        ret = gpiod_line_event_read(line, &event);
        if (ret < 0) {
            perror("Error reading GPIO event");
            break;
        }

        if (event.event_type == GPIOD_LINE_EVENT_RISING_EDGE) {
        // Get current time with clock_gettime
            gint64 pos;
            gst_element_query_position(pipeline, GST_FORMAT_TIME, &pos);
            time_in_seconds = (double)pos / GST_SECOND;
            capture_frame();
        }
    }

    return NULL;
}

int main(int argc, char *argv[]) {

    gst_init(&argc, &argv);
    pipeline = gst_parse_launch(
        "v4l2src device=/dev/video0 ! "
        "video/x-raw,format=GRAY16_LE,width=640,height=512,framerate=9/1 ! "
        "appsink name=sink", NULL);

    appsink = gst_bin_get_by_name(GST_BIN(pipeline), "sink");
    g_object_set(G_OBJECT(appsink), "drop", TRUE, "max-buffers", 1, "sync", FALSE, NULL);

    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) == -1) {
        perror("clock_gettime");
        return -1;
    }
    // time_in_seconds = ts.tv_sec + ts.tv_nsec / 1e9;
    time_in_seconds = 0;
    gst_element_set_state(pipeline, GST_STATE_PLAYING);
    g_usleep(10000000);  // Let the pipeline warm up

    for (int i = 0; i < 4; i++) {
        GstSample *flush_sample = gst_app_sink_pull_sample(GST_APP_SINK(appsink));
        if (flush_sample) gst_sample_unref(flush_sample);
        g_print("Flushed frame %d\n", i + 1);
    }

    fd = fopen(OUTPUT_FILE_NAME, "w");
    if (!fd) {
        perror("Failed to open output file");
        return 1;
    }

    // Set up gpiod interrupt
    struct gpiod_chip *chip = gpiod_chip_open(GPIO_CHIP);
    struct gpiod_line *line = gpiod_chip_get_line(chip, GPIO_LINE);
    struct gpiod_line_request_config config = {
        .consumer = "thermal_trigger",
        .request_type = GPIOD_LINE_REQUEST_EVENT_RISING_EDGE,
        .flags = 0
    };
    if (gpiod_line_request(line, &config, 0) < 0) {
        perror("GPIO line request failed");
        return 1;
    }
    pthread_attr_t attr;
    struct sched_param param;
    pthread_attr_init(&attr);
    pthread_attr_setschedpolicy(&attr, SCHED_FIFO);
    param.sched_priority = sched_get_priority_max(SCHED_FIFO);
    pthread_attr_setschedparam(&attr, &param);
    pthread_t gpio_thread;
    if (pthread_create(&gpio_thread, &attr, handle_gpio_interrupt, line) != 0) {
        printf("Failed to create thread\n");
        return;
    }

    // Wait for GPIO thread
    pthread_join(gpio_thread, NULL);
    pthread_attr_destroy(&attr);
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(appsink);
    gst_object_unref(pipeline);
    fclose(fd);

    gpiod_line_release(line);
    gpiod_chip_close(chip);
    return 0;
}
