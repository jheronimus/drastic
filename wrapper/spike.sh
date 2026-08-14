#!/bin/sh
# Milestone 1 gating spike: verify that the Bionic-built DraStic core can be
# dlopen'd on aarch64 musl after remapping its DT_NEEDED libs to the musl
# loader and providing Bionic/OpenSLES shims.
#
# Intended to run INSIDE the Minime Alpine builder container (aarch64 musl):
#   make -C minime/targets/alpine shell    # or your podman wrapper
#   src/drastic/wrapper/spike.sh <libs-dir>
# where <libs-dir> is the checked-out private libs repo (src/drastic/libs/core/android/arm64).
#
# Exits 0 (all JNI symbols resolve) or 1 (dlopen or symbol resolution failed).

set -e

WRAPPER_DIR="$(cd "$(dirname "$0")" && pwd)"

LIBS_DIR="$1"
test -n "$LIBS_DIR" || {
	echo "usage: $0 <libs-dir> (arm64 .so dir)"
	exit 2
}

ARM64="libdrastic_arm64.so"
CPU="libdrastic_cpu.so"
WORK="$(mktemp -d /tmp/drastic-spike.XXXXXX)"
trap 'rm -rf "$WORK"' EXIT

echo "== work dir: $WORK =="

# Locate musl loader name.
MUSL_LD="$(ls /lib/ld-musl-aarch64.so.1 2>/dev/null || true)"
test -n "$MUSL_LD" || MUSL_LD="ld-musl-aarch64.so.1"
echo "== musl loader: $MUSL_LD =="

# Remap the core's Android DT_NEEDED entries onto musl/real libs.
cp "$LIBS_DIR/$ARM64" "$WORK/"
patchelf \
	--replace-needed libc.so "$MUSL_LD" \
	--replace-needed libm.so "$MUSL_LD" \
	--replace-needed libdl.so "$MUSL_LD" \
	"$WORK/$ARM64"

# libstdc++: the core imports no C++ runtime symbols, so resolve to the real one.
if [ -e /usr/lib/libstdc++.so.6 ]; then
	patchelf --replace-needed libstdc++.so libstdc++.so.6 "$WORK/$ARM64"
fi

# libz: resolve to the runtime zlib.
if [ -e /usr/lib/libz.so.1 ]; then
	patchelf --replace-needed libz.so libz.so.1 "$WORK/$ARM64"
fi

echo "== NEEDED after patch =="
patchelf --print-needed "$WORK/$ARM64"

# Build the Bionic shim as liblog.so (LD_PRELOAD'd) and an empty libOpenSLES.so
# stub (the core imports OpenSLES symbols; the wrapper's shim defines them).
gcc -O2 -fPIC -shared -o "$WORK/liblog.so" "$WRAPPER_DIR/bionic_shim.c" -ldl
gcc -O2 -fPIC -shared -o "$WORK/libOpenSLES.so" "$WRAPPER_DIR/bionic_shim.c" -ldl

# CPU core (dlopen'd by the main core at runtime) -- needs the same remap.
cp "$LIBS_DIR/$CPU" "$WORK/"
patchelf \
	--replace-needed libc.so "$MUSL_LD" \
	--replace-needed libm.so "$MUSL_LD" \
	--replace-needed libdl.so "$MUSL_LD" \
	"$WORK/$CPU" || true

# Build the dlopen harness.
gcc -O2 -o "$WORK/spike" "$WRAPPER_DIR/spike.c" -ldl

echo "== running spike (LD_PRELOAD=liblog.so, LD_LIBRARY_PATH=$WORK) =="
LD_PRELOAD="$WORK/liblog.so" \
	LD_LIBRARY_PATH="$WORK" \
	"$WORK/spike" "$WORK/$ARM64"
