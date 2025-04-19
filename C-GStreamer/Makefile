CC = gcc
CFLAGS = -Wall -g `pkg-config --cflags gstreamer-1.0 gstreamer-app-1.0`
LDFLAGS = -lgpiod -pthread `pkg-config --libs gstreamer-1.0 gstreamer-app-1.0`
TARGET = capture
SRC = HardwarePiGpioTrigger.c log.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(LDFLAGS)

clean:
	rm -f $(TARGET)
