#define _GNU_SOURCE // Required for CPU affinity functions
#include "HardwareGPIOd.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <sched.h>
#include <stdatomic.h>
#include <stdbool.h>
#define DEFAULT_SAMPLE_RATE 1
#define THREAD_COUNT 3
pthread_barrier_t barrier;
long long tickGlobal;
atomic_bool tickReady = ATOMIC_VAR_INIT(true);
int trigger_counter = 0;
long long prev_tick = 0;
atomic_int recent_trigger_number = ATOMIC_VAR_INIT(0);
// Function to open the GPIO chip
struct gpiod_chip* open_gpio_chip() {
    struct gpiod_chip *chip = gpiod_chip_open(GPIO_CHIP);
    if (!chip) {
        perror("Failed to open GPIO chip");
        exit(EXIT_FAILURE);
    }
    return chip;
}

// Function to configure GPIO line for interrupts
struct gpiod_line* configure_gpio_interrupt(struct gpiod_chip *chip, int line_number) {
    struct gpiod_line *line = gpiod_chip_get_line(chip, line_number);
    if (!line) {
        perror("Failed to get GPIO line");
        gpiod_chip_close(chip);
        exit(EXIT_FAILURE);
    }

    // Configure GPIO interrupt request
    struct gpiod_line_request_config config;
    config.consumer = "gpio_interrupt";
    config.request_type = GPIOD_LINE_REQUEST_EVENT_BOTH_EDGES; // Corrected constant
    config.flags = 0;  // No additional flags

    int ret = gpiod_line_request(line, &config, 0);
    if (ret < 0) {
        perror("Failed to request GPIO line events");
        gpiod_chip_close(chip);
        exit(EXIT_FAILURE);
    }

    return line;
}
void set_cpu_affinity(int core_id) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset); // Assign thread to core_id
    int out = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t),&cpuset);
    if (out != 0) {
        printf("succ afinity fail!, error code: %d\n", out);
        fflush(stdout);
    }
    printf("succ afinity success!\n");
    fflush(stdout);
}
void *rgb_trigger(void *arg){
    int thread_id = *(int *)arg;
    set_cpu_affinity(thread_id);
    struct timespec ts;
    while(true){
      if (atomic_load(&tickReady) == false){
        long long tick = tickGlobal;
        int trigger_num = atomic_load(&recent_trigger_number);
        pthread_barrier_wait(&barrier);
        // fucking actually trigger the camera here (im not writing this)
        timespec_get(&ts,TIME_UTC);
        long long endTick = ((long long)ts.tv_sec * 1000LL + ts.tv_nsec / 1000000LL);
        atomic_store(&tickReady, true);
        // send image capture, start time, end time, trigger number associated with capture all together as a packet to httpserver
      }
    }
    return;
}
void *ir_trigger(void *arg){
    int thread_id = *(int *)arg;
    set_cpu_affinity(thread_id);
    struct timespec ts;
    while(true){
      if (atomic_load(&tickReady) == false){
        long long tick = tickGlobal;
        int trigger_num = atomic_load(&recent_trigger_number);
        pthread_barrier_wait(&barrier);
        // fucking actually trigger the ir camera here
        timespec_get(&ts,TIME_UTC);
        long long endTick = ((long long)ts.tv_sec * 1000LL + ts.tv_nsec / 1000000LL);
        atomic_store(&tickReady, true);
        // send image capture, start time(tick), end time(endTick), trigger number associated with capture all together as a packet to httpserver
      }
    }
    return;
}
void *sendToJetson(void *arg){
    int thread_id = *(int *)arg;
    set_cpu_affinity(thread_id);
    fflush(stdout);
    struct timespec ts;
    return;
}
void spawn_workers(pthread_t threads[THREAD_COUNT]){
    pthread_attr_t attr;
    struct sched_param param;
    pthread_attr_init(&attr);
    pthread_attr_setschedpolicy(&attr, SCHED_FIFO);
    param.sched_priority = sched_get_priority_max(SCHED_FIFO);
    pthread_attr_setschedparam(&attr, &param);
    pthread_barrier_init(&barrier, NULL, 2);
    int x = 0;
    if (pthread_create(&threads[0], &attr, rgb_trigger, &x) != 0) {
        printf("Failed to create thread\n");
        return;
    }
    usleep(1000);
    int y = 1;
    if (pthread_create(&threads[1], &attr, ir_trigger, &y) != 0){
        printf("Failed to create thread\n");
        return;
    }
    usleep(1000);
    int z = 2;
    if (pthread_create(&threads[2], NULL, sendToJetson, &z) != 0){
        printf("Failed to create thread\n");
        return;
    }
    pthread_attr_destroy(&attr);
    return;
}

// Function to handle GPIO interrupts in a non-blocking way
void *handle_gpio_interrupt(void *arg) {
    struct gpiod_line *line = (struct gpiod_line *)arg;
    struct gpiod_line_event event;
    struct timespec timeout;
    struct timespec ts;
    int ret;
    pthread_t threads[THREAD_COUNT];
    set_cpu_affinity(3);
    usleep(1000);
    spawn_workers(threads);
    pthread_t rgb = threads[0];
    pthread_t ir = threads[1];
    pthread_t consume = threads[2];
    FILE *fd = fopen("output.txt", "w");
    if (fd == NULL){
	    perror("Failed to open file");
    }
//fprintf(fd, "Listening for GPIO %d interrupts (non-blocking)...\n", GPIO_LINE);
    fflush(fd);
    
    while (1) {
        timeout.tv_sec = TIMEOUT_SEC;
        timeout.tv_nsec = 0;


       // struct tm *delta = gmtime(&ts.tv_nsec);

        ret = gpiod_line_event_wait(line, &timeout);
        
        if (ret < 0) {
            perror("Error waiting for GPIO event");
            break;
        } else if (ret == 0) {
            //fprintf(fd, "No GPIO event detected within %d seconds. Continuing...\n", TIMEOUT_SEC);
            // fflush(fd);
            continue;
        }
        timespec_get(&ts,TIME_UTC);
        ret = gpiod_line_event_read(line, &event);
        if (ret < 0) {
            perror("Error reading GPIO event");
            break;
        }

        if (event.event_type == GPIOD_LINE_EVENT_RISING_EDGE) {
            //printf("GPIO %d triggered! Type: RISING EDGE at time %ld \n", GPIO_LINE, ts.tv_nsec);
            long long tick =  ((long long)ts.tv_sec * 1000000000LL + ts.tv_nsec);
            fprintf(fd, "%lld \n", tick);
	        fflush(fd);
            trigger_counter += 1;
            bool exp = true;
            bool desired = false;
            if (atomic_compare_exchange_strong(&tickReady, &exp, desired)){
              tickGlobal = tick;
              atomic_store(&recent_trigger_number, trigger_counter);
              continue;
            }
            else{
                continue;
            }
        } //else if (event.event_type == GPIOD_LINE_EVENT_FALLING_EDGE) {
        //     printf("GPIO %d triggered! Type: FALLING EDGE at time %ld \n", GPIO_LINE, ts.tv_nsec);
        // }
    }
    pthread_join(rgb, NULL);
    pthread_join(ir, NULL);
    pthread_join(consume, NULL);

}

// Function to clean up GPIO resources
void cleanup_gpio(struct gpiod_chip *chip, struct gpiod_line *line) {
    gpiod_line_release(line);
    gpiod_chip_close(chip);
    printf("GPIO cleanup completed.\n");
}


int main(void) {
    
    struct gpiod_chip *chip = open_gpio_chip();
    struct gpiod_line *line = configure_gpio_interrupt(chip, GPIO_LINE);
    pthread_t con;
    pthread_attr_t attr;
    struct sched_param param;
    pthread_attr_init(&attr);
    pthread_attr_setschedpolicy(&attr, SCHED_FIFO);
    param.sched_priority = sched_get_priority_max(SCHED_FIFO);
    pthread_attr_setschedparam(&attr, &param);
	if (pthread_create(&con, &attr, handle_gpio_interrupt, line) != 0){
		printf("Error\n");
		return 1;
	}
    pthread_attr_destroy(&attr);
	pthread_join(con, NULL);
    cleanup_gpio(chip, line);
    return 0;
}
