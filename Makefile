CC ?= gcc
CFLAGS ?= -Wall -Wextra -Wpedantic -std=c11 -Iinclude -Isrc
LDFLAGS ?=
LDLIBS ?= -lbluetooth -lpthread

APP_SRCS = \
	src/app/main.c \
	src/app/app_state.c \
	src/app/console_ui.c \
	src/ble/ble_bluez_nus.c \
	src/protocol/cap_protocol.c \
	src/protocol/cap_framing.c

APP_OBJS = $(APP_SRCS:.c=.o)

.PHONY: all clean

all: cap_ble_test

cap_ble_test: $(APP_OBJS)
	$(CC) $(CFLAGS) -o $@ $(APP_OBJS) $(LDFLAGS) $(LDLIBS)

clean:
	rm -f $(APP_OBJS) cap_ble_test
