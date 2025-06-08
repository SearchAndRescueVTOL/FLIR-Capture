#include "HardwarePiGpioTrigger.h"

char *get_timestamp_string() {
    static char buffer[100];
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    struct tm *local = localtime(&ts.tv_sec);
    snprintf(buffer, sizeof(buffer), "%02d_%02d_%04d_%02d_%02d_%02d_%03ld",
             local->tm_mon + 1, local->tm_mday, local->tm_year + 1900,
             local->tm_hour, local->tm_min, local->tm_sec, ts.tv_nsec / 1000000);
    return buffer;
}

void setup_session_directory_and_log() {
    char *timestamp = get_timestamp_string();
    snprintf(session_dir, sizeof(session_dir), "/home/sarv-pi/IR/%s", timestamp);
    mkdir("/home/sarv-pi/IR", 0755); // Ensure parent directory exists
    mkdir(session_dir, 0755);
    char log_path[320];
    snprintf(log_path, sizeof(log_path), "logs/%s.txt", timestamp);
    logfile = fopen(log_path, "a+");
    if (!logfile) {
        fprintf(stderr, "----------------------------------\033[31mFailed to open log file\033[0m\n");
        exit(1);
    }
    fprintf(logfile, "HardwarePiGpioTrigger Program Started!\n");
    fflush(logfile);
    // log_add_fp(logfile, LOG_TRACE);
    // log_info("HardwarePiGpioTrigger Program started");
}

void capture_frame() {
    GstSample *sample = gst_app_sink_pull_sample(GST_APP_SINK(appsink));
    if (!sample) {
        g_printerr("----------------------------------\033[31mFailed to capture sample\033[0m\n");
        return;
    }

    GstBuffer *buffer = gst_sample_get_buffer(sample);
    GstMapInfo map;
    if (gst_buffer_map(buffer, &map, GST_MAP_READ)) {
        char filename[700];
        trigger_counter += 1;

        char *current_time = get_timestamp_string();
        snprintf(filename, sizeof(filename),
                 "%s/%s_ir_#%lld.raw",
                 session_dir, current_time, trigger_counter);

        FILE *out = fopen(filename, "wb");
        fwrite(map.data, 1, map.size, out);
        fclose(out);
        fprintf(logfile, "Saved %s\n", filename);
        fflush(logfile);
        gst_buffer_unmap(buffer, &map);
        printf("Saved: %s\n", filename);
        fflush(stdout);
    }

    gst_sample_unref(sample);
}
void set_cpu_affinity(int core_id) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset); // Assign thread to core_id
    if (pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t),&cpuset) != 0) {
      printf("----------------------------------\033[31mError getting affinity!\033[0m\n");
    }
}

void *handle_gpio_interrupt(void *arg) {
    struct gpiod_line *line = (struct gpiod_line *)arg;
    struct gpiod_line_event event;
    struct timespec timeout;
    struct timespec ts;
    // set_cpu_affinity(3);
    while (1) {
        timeout.tv_sec = TIMEOUT_SEC;
        timeout.tv_nsec = 0;

        int ret = gpiod_line_event_wait(line, &timeout);
        if (ret < 0) {
            perror("----------------------------------\033[31mError waiting for GPIO event\033[0m");
            break;
        } else if (ret == 0) {
            continue;
        }

        timespec_get(&ts, TIME_UTC);
        ret = gpiod_line_event_read(line, &event);
        if (ret < 0) {
            perror("----------------------------------\033[31mError reading GPIO event\033[0m");
            break;
        }

        if (event.event_type == GPIOD_LINE_EVENT_RISING_EDGE) {
            // log_to_mosq("GPIO Event on line 26!");
            capture_frame();
        }
    }

    return NULL;
}
// int log_to_mosq(char *msg){
//     rc = mosquitto_publish(mosq, NULL, MQTT_TOPIC, strlen(msg), msg, 0, false);
//     if (rc != MOSQ_ERR_SUCCESS) {
//         log_trace("Failed to log over mosquitto!\n");
//         return -1;
//     } else {
//         return 0;
//     }

// }
int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <uvc-device-path>\n", argv[0]);
        return 1;
    }
    setup_session_directory_and_log();
    const char *device_path = argv[1];
    char pipeline_str[512];
    snprintf(pipeline_str, sizeof(pipeline_str),
        "v4l2src device=%s ! "
        "video/x-raw,format=I420,width=640,height=512,framerate=9/1 ! "
        "appsink name=sink", device_path);

    /////////////
    // log_add_fp(logfile, LOG_TRACE);  // Log everything (TRACE and above)
    // log_info("HardwarePiGpioTrigger Program started");
    /////////////
    //Initialize mosquitto lib logs
    // mosquitto_lib_init();

    // // Create a new Mosquitto client instance
    // mosq = mosquitto_new(NULL, true, NULL);
    // if (!mosq) {
    //     fprintf(stderr, "Failed to create Mosquitto instance.\n");
    //     return 1;
    // }
    // rc = mosquitto_connect(mosq, MQTT_HOST, MQTT_PORT, 60);
    // if (rc != MOSQ_ERR_SUCCESS) {
    //     fprintf(stderr, "Could not connect to Broker. Error: %s\n", mosquitto_strerror(rc));
    //     mosquitto_destroy(mosq);
    //     return 1;
    // }
    // End initialization
    // Initialize g-streamer
    gst_init(&argc, &argv);
    GstElement *pipeline = gst_parse_launch(pipeline_str, NULL);
    appsink = gst_bin_get_by_name(GST_BIN(pipeline), "sink");
    g_object_set(G_OBJECT(appsink), "drop", TRUE, "max-buffers", 1, "sync", FALSE, NULL);

    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) == -1) {
        perror("clock_gettime");
        fclose(logfile);
        // mosquitto_disconnect(mosq);
        // mosquitto_destroy(mosq);
        // mosquitto_lib_cleanup(); 
        gst_element_set_state(pipeline, GST_STATE_NULL);
        gst_object_unref(appsink);
        gst_object_unref(pipeline);   
        return -1;
    }
    // time_in_seconds = ts.tv_sec + ts.tv_nsec / 1e9;
    time_in_seconds = 0;
    gst_element_set_state(pipeline, GST_STATE_PLAYING);
    g_usleep(1000000);  // Let the pipeline warm up
    for (int i = 0; i < 4; i++) {
        GstSample *flush_sample = gst_app_sink_pull_sample(GST_APP_SINK(appsink));
        if (flush_sample) gst_sample_unref(flush_sample);
    }
    // End initialize gstreamer


    fd = fopen(OUTPUT_FILE_NAME, "w");
    if (!fd) {
        perror("----------------------------------\033[31mFailed to open output file\033[0m");
        // mosquitto_disconnect(mosq);
        // mosquitto_destroy(mosq);
        // mosquitto_lib_cleanup();
        fclose(logfile); 
        gst_element_set_state(pipeline, GST_STATE_NULL);
        gst_object_unref(appsink);
        gst_object_unref(pipeline);  
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
        perror("----------------------------------\033[31mGPIO line request failed\033[0m");
        // mosquitto_disconnect(mosq);
        // mosquitto_destroy(mosq);
        // mosquitto_lib_cleanup(); 
        fclose(fd);
        fclose(logfile);
        gst_element_set_state(pipeline, GST_STATE_NULL);
        gst_object_unref(appsink);
        gst_object_unref(pipeline);  
        return 1;
    }
    // Spawn gpio listener threads
    pthread_attr_t attr;
    struct sched_param param;
    pthread_attr_init(&attr);
    pthread_attr_setschedpolicy(&attr, SCHED_FIFO);
    param.sched_priority = sched_get_priority_max(SCHED_FIFO);
    pthread_attr_setschedparam(&attr, &param);
    pthread_t gpio_thread;
    if (pthread_create(&gpio_thread, NULL, handle_gpio_interrupt, line) != 0) {
        printf("----------------------------------\033[31mFailed to create thread\033[0m\n");
        // mosquitto_disconnect(mosq);
        // mosquitto_destroy(mosq);
        // mosquitto_lib_cleanup(); 
        gst_element_set_state(pipeline, GST_STATE_NULL);
        gst_object_unref(appsink);
        gst_object_unref(pipeline);  
        fclose(fd);
        fclose(logfile);
        gpiod_line_release(line);
        gpiod_chip_close(chip);
        return;
    }
    // Wait for GPIO thread
    pthread_join(gpio_thread, NULL);
    // mosquitto_disconnect(mosq);
    // mosquitto_destroy(mosq);
    // mosquitto_lib_cleanup();
    pthread_attr_destroy(&attr);
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(appsink);
    gst_object_unref(pipeline);
    fclose(fd);
    fclose(logfile);
    gpiod_line_release(line);
    gpiod_chip_close(chip);
    return 0;
}
