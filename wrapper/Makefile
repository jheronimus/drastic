CC ?= gcc
CFLAGS ?= -O2 -fPIC -Wall -Wextra -I.
LDFLAGS ?= -shared

OBJS = bionic_shim.o jni_mock.o drastic_libretro.o
TARGET_SO = drastic_libretro.so
LIBLOG = liblog.so
LIBOPENSLES = libOpenSLES.so
PAK_DIR = dist/NDS.pak

.PHONY: all clean pak

all: $(TARGET_SO) pak

$(LIBLOG): bionic_shim.o
	$(CC) $(LDFLAGS) bionic_shim.o -o $@

$(LIBOPENSLES): bionic_shim.o
	$(CC) $(LDFLAGS) bionic_shim.o -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET_SO): $(OBJS) $(LIBLOG) $(LIBOPENSLES)
	$(CC) $(LDFLAGS) $(OBJS) -o $@ -L. -llog -lOpenSLES -ldl

pak: $(TARGET_SO) $(LIBLOG) $(LIBOPENSLES)
	mkdir -p $(PAK_DIR)
	cp $(TARGET_SO) $(LIBLOG) $(LIBOPENSLES) launch.sh default.cfg $(PAK_DIR)/
	@if [ -d "../libs/core/android/assets" ]; then \
		cp -r ../libs/core/android/assets/* $(PAK_DIR)/ 2>/dev/null || true; \
	fi
	@chmod +x $(PAK_DIR)/launch.sh
	cd dist && zip -r -q NDS.pak.zip NDS.pak
	@echo "[DraStic] NDS.pak created in $(PAK_DIR)/ and dist/NDS.pak.zip"

clean:
	rm -f $(OBJS) $(TARGET_SO) $(LIBLOG) $(LIBOPENSLES)
	rm -rf dist/
