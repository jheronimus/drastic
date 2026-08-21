CC ?= gcc
CFLAGS ?= -O2 -fPIC -Wall -Wextra -I.
LDFLAGS ?= -shared

OBJS = bionic_shim.o jni_mock.o drastic_libretro.o
TARGET_SO = drastic_libretro.so
LIBLOG = liblog.so
LIBOPENSLES = libOpenSLES.so
LIBASHMEM = libashmem.so
LIBANDROID = libandroid.so
SHIMS = $(LIBLOG) $(LIBOPENSLES) $(LIBASHMEM) $(LIBANDROID)
PAK_DIR = dist/NDS.pak

.PHONY: all clean pak

all: $(TARGET_SO) pak

$(LIBLOG): bionic_shim.o
	$(CC) $(LDFLAGS) bionic_shim.o -o $@

$(LIBOPENSLES): bionic_shim.o
	$(CC) $(LDFLAGS) bionic_shim.o -o $@

$(LIBASHMEM): libashmem.c
	$(CC) $(CFLAGS) -shared -o $@ $< -ldl

$(LIBANDROID): libandroid.c
	$(CC) $(CFLAGS) -shared -o $@ $<

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET_SO): $(OBJS) $(SHIMS)
	$(CC) $(LDFLAGS) $(OBJS) -o $@ -L. -llog -lOpenSLES -ldl

pak: $(TARGET_SO) $(SHIMS)
	mkdir -p $(PAK_DIR)
	cp $(TARGET_SO) $(SHIMS) launch.sh default.cfg $(PAK_DIR)/
	@if [ -d "../libs/core/android/assets" ]; then \
		cp -r ../libs/core/android/assets/* $(PAK_DIR)/ 2>/dev/null || true; \
	fi
	@chmod +x $(PAK_DIR)/launch.sh
	cd dist && zip -r -q NDS.pak.zip NDS.pak
	@echo "[DraStic] NDS.pak created in $(PAK_DIR)/ and dist/NDS.pak.zip"

clean:
	rm -f $(OBJS) $(TARGET_SO) $(SHIMS)
	rm -rf dist/
