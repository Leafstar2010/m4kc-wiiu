#!/bin/sh

# m4kc Wii U build system

VERSION="0.2"
TARGET="m4kc"
SOURCES="src"
OBJ_PATH="wiiu/o"
OUT_PATH="wiiu/bin"

# Wii U specific environment
export DEVKITPRO="/c/devkitpro"
export DEVKITPPC="$DEVKITPRO/devkitPPC"
export PATH="$PATH:$DEVKITPPC/bin"

WUT_ROOT="$DEVKITPRO/wut"
PORTLIBS_ROOT="$DEVKITPRO/portlibs/wiiu"

CC="$DEVKITPPC/bin/powerpc-eabi-gcc"
ELF2RPL="/c/devkitpro/tools/bin/elf2rpl.exe"

# Compiler flags: includes from wut (for coreinit, sndcore2, vpad) and SDL2
CFLAGS="-g -O3 -Wall -std=c11 -D__WIIU__ -I$WUT_ROOT/include -I$PORTLIBS_ROOT/include"

# Linker flags: link against wut (which contains coreinit, sndcore2, vpad), SDL2, and math
LDFLAGS="-L$WUT_ROOT/lib -L$PORTLIBS_ROOT/lib"
LIBS="-lSDL2 -lwut -lm"

# Ensure elf2rpl exists
if [ ! -f "$ELF2RPL" ]; then
    echo "ERROR: elf2rpl.exe not found at $ELF2RPL" >&2
    exit 1
fi

echo "Cleaning up old versions"
rm -rf "M4KCU.rpx"
rm -rf "M4KCU.wuhb"

# Create output directories
mkdir -p "$OBJ_PATH"
mkdir -p "$OUT_PATH"

echo "=== Building $TARGET for Wii U ==="

# Compile all .c files in src/
for file in $SOURCES/*.c; do
    if [ ! -f "$file" ]; then
        echo "No .c files found in $SOURCES/" >&2
        exit 1
    fi
    obj="$OBJ_PATH/$(basename "${file%.*}").o"
    echo "Compiling $(basename "$file")"
    $CC $CFLAGS -w -c "$file" -o "$obj"
done

# Link object files into an ELF
echo "Linking .o Files Into ELF"
$CC -specs="$WUT_ROOT/share/wut.specs" $OBJ_PATH/*.o -o "$OUT_PATH/$TARGET.elf" $LDFLAGS $LIBS

# Convert ELF to RPX
echo "Converting ELF To RPX"
$ELF2RPL "$OUT_PATH/$TARGET.elf" "M4KCU.rpx"

echo "RPX Built Successfully"

rm -rf "wiiu"
echo "Removed Unnecessary (Temporary) Files"

echo "Converting RPX to WUHB"

wuhbtool.exe "M4KCU.rpx" "M4KCU.wuhb" --name="M4KCU" --short-name="Minecraft 4K C Wii U" --icon="icons/iconLowres.png"
echo "WUHB Built Successfully"

echo "Done!"