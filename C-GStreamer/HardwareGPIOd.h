#ifndef GPIO_INTERRUPT_H
#define GPIO_INTERRUPT_H

#include <gpiod.h>

// Define GPIO chip and pin
#define GPIO_CHIP "/dev/gpiochip0"
#define GPIO_LINE 27
#define TIMEOUT_SEC 1 // Timeout in seconds for non-blocking

// Function prototypes
struct gpiod_chip* open_gpio_chip();
struct gpiod_line* configure_gpio_interrupt(struct gpiod_chip *chip, int line_number);


void *handle_gpio_interrupt(void *arg);

void cleanup_gpio(struct gpiod_chip *chip, struct gpiod_line *line);

#endif // GPIO_INTERRUPT_H
