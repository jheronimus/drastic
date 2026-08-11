CC ?= gcc
CFLAGS ?= -O2 -fPIC -Wall -Wextra
INCLUDES = -I.
SRCS = bionic_shim.c jni_mock.c drastic_libretro.c
OBJS = $(SRCS:.c=.o)
TARGET = drastic_libretro.so

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -shared $(OBJS) -o $@ $(LDFLAGS) -ldl

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
