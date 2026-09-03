#!/bin/sh
# shellcheck disable=SC3045
ulimit -c unlimited 2>/dev/null || true
killall -9 minarch.elf 2>/dev/null || true

EMU_EXE=drastic
CORES_PATH=$(dirname "$0")

###############################

EMU_TAG=$(basename "$(dirname "$0")" .pak)
ROM="$1"
LOGS_PATH="${LOGS_PATH:-/mnt/sdcard/.userdata/minime/logs}"
BIOS_PATH="${BIOS_PATH:-/mnt/sdcard/Bios}"
SAVES_PATH="${SAVES_PATH:-/mnt/sdcard/Saves}"
USERDATA_PATH="${USERDATA_PATH:-/mnt/sdcard/.userdata/minime}"
SYSTEM_PATH="${SYSTEM_PATH:-/mnt/sdcard/.system/minime}"

mkdir -p "$LOGS_PATH"
mkdir -p "$BIOS_PATH/$EMU_TAG"
mkdir -p "$SAVES_PATH/$EMU_TAG"
CONFIG_DIR="$USERDATA_PATH/$EMU_TAG-$EMU_EXE"
mkdir -p "$CONFIG_DIR"
if [ ! -f "$CONFIG_DIR/minarch.cfg" ] && [ -f "$CORES_PATH/default.cfg" ]; then
    cp "$CORES_PATH/default.cfg" "$CONFIG_DIR/minarch.cfg"
fi
HOME="$USERDATA_PATH"
cd "$HOME" || exit 1
[ -x /usr/share/minime/scripts/audio.sh ] && /usr/share/minime/scripts/audio.sh init >/dev/null 2>&1 || true
amixer -q -c 1 sset 'Playback Mux' HP >/dev/null 2>&1 || amixer -q sset 'Playback Mux' HP >/dev/null 2>&1 || true
amixer -q -c 1 sset 'Internal Speakers' on >/dev/null 2>&1 || amixer -q sset 'Internal Speakers' on >/dev/null 2>&1 || true
export SDL_AUDIODRIVER="${SDL_AUDIODRIVER:-alsa}"
export LD_LIBRARY_PATH="$CORES_PATH:$SYSTEM_PATH/lib:${LD_LIBRARY_PATH:-/usr/lib:/lib}"
LD_PRELOAD="$CORES_PATH/libashmem.so" PATH="$SYSTEM_PATH/bin:$PATH" minarch.elf "$CORES_PATH/${EMU_EXE}_libretro.so" "$ROM" >"$LOGS_PATH/$EMU_TAG.txt" 2>&1
