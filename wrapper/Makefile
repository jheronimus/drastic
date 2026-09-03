CC ?= gcc
CFLAGS ?= -O2 -fPIC -Wall -Wextra -I.
LDFLAGS ?= -shared -Wl,-rpath,'$$ORIGIN'
PATCHELF ?= patchelf

OBJS = jni_mock.o drastic_libretro.o
TARGET_SO = drastic_libretro.so
LIBLOG = liblog.so
LIBOPENSLES = libOpenSLES.so
LIBASHMEM = libashmem.so
LIBANDROID = libandroid.so
SHIMS = $(LIBLOG) $(LIBOPENSLES) $(LIBASHMEM) $(LIBANDROID)
PAK_DIR = dist/NDS.pak

.PHONY: all clean pak

all: $(TARGET_SO) pak

$(LIBLOG): liblog.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<

$(LIBOPENSLES): bionic_shim.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<

$(LIBASHMEM): libashmem.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< -ldl

$(LIBANDROID): libandroid.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET_SO): $(OBJS) $(SHIMS)
	$(CC) $(LDFLAGS) $(OBJS) -o $@ -L. -llog -lOpenSLES -ldl

pak: $(TARGET_SO) $(SHIMS)
	mkdir -p $(PAK_DIR)
	cp $(TARGET_SO) $(SHIMS) launch.sh default.cfg $(PAK_DIR)/
	@if [ -f "../libs/core/android/arm64/libdrastic_arm64.so" ]; then \
		cp ../libs/core/android/arm64/libdrastic_arm64.so $(PAK_DIR)/; \
		cp ../libs/core/android/arm64/libdrastic_cpu.so $(PAK_DIR)/; \
		$(PATCHELF) \
			--set-rpath '$$ORIGIN' \
			--replace-needed libc.so ld-musl-aarch64.so.1 \
			--replace-needed libm.so ld-musl-aarch64.so.1 \
			--replace-needed libdl.so ld-musl-aarch64.so.1 \
			--replace-needed libstdc++.so libstdc++.so.6 \
			--replace-needed libz.so libz.so.1 \
			--replace-needed libGLESv2.so libGLESv2.so.2 \
			$(PAK_DIR)/libdrastic_arm64.so; \
		$(PATCHELF) \
			--set-rpath '$$ORIGIN' \
			--replace-needed libc.so ld-musl-aarch64.so.1 \
			--replace-needed libm.so ld-musl-aarch64.so.1 \
			--replace-needed libdl.so ld-musl-aarch64.so.1 \
			--replace-needed libstdc++.so libstdc++.so.6 \
			$(PAK_DIR)/libdrastic_cpu.so; \
	fi
	@if [ -d "../libs/core/android/assets" ]; then \
		cp -r ../libs/core/android/assets/* $(PAK_DIR)/ 2>/dev/null || true; \
	fi
	for so in $(TARGET_SO) $(SHIMS); do \
		$(PATCHELF) --replace-needed libc.so ld-musl-aarch64.so.1 $(PAK_DIR)/$$so 2>/dev/null || true; \
	done
	$(PATCHELF) --replace-needed ./libOpenSLES.so libOpenSLES.so $(PAK_DIR)/$(TARGET_SO) 2>/dev/null || true
	$(PATCHELF) --replace-needed ./liblog.so liblog.so $(PAK_DIR)/$(TARGET_SO) 2>/dev/null || true
	@chmod +x $(PAK_DIR)/launch.sh
	cd dist && zip -r -q NDS.pak.zip NDS.pak
	@echo "[DraStic] NDS.pak created in $(PAK_DIR)/ and dist/NDS.pak.zip"

check: $(TARGET_SO) $(SHIMS)
	$(CC) $(CFLAGS) -I. -o tests/frontend_smoke tests/frontend_smoke.c -ldl
	./tests/frontend_smoke

clean:
	rm -f $(OBJS) $(TARGET_SO) $(SHIMS) tests/frontend_smoke
	rm -rf dist/
