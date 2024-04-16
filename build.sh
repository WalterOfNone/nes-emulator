#!/bin/sh

set -xe

[ $CC ] || CC=gcc
[ $NAME ] || NAME="nesEmu"
CFLAGS="$CFLAGS -O2 -Wall -Wextra -Wpedantic -std=gnu99"
# I'm probably not using rpath correctly lmao
LDFLAGS="$LDFLAGS -Wall -Wextra -Wpedantic -Wl,-rpath $(pwd)/build/ -L$(pwd)/build/"
DEFINES="$DEFINES"
LIBS="-lSDL2"

CFILES="$(find src/ -name "*.c")"
OBJS=""

SDL_VERSION="2.30.2"
if ! [ -d "SDL2-$SDL_VERSION" ]; then
	if ! [ -f "SDL2-$SDL_VERSION.tar.gz" ]; then
		wget "https://github.com/libsdl-org/SDL/releases/download/release-$SDL_VERSION/SDL2-$SDL_VERSION.tar.gz"
	fi
	tar -xavf SDL2-$SDL_VERSION.tar.gz
fi

if ! [ -f "SDL2-$SDL_VERSION/build/.libs/libSDL2-2.0.so.0" ]; then
	ORIGIN_DIR="$(pwd)"
	cd "$ORIGIN_DIR/SDL2-$SDL_VERSION"
	./configure
	make -j$(nproc)
	cd "$ORIGIN_DIR"
fi

rm -rf build/ obj/

mkdir build/ obj/

cp SDL2-$SDL_VERSION/build/.libs/libSDL2-2.0.so.0 build/

for f in $CFILES; do
	OBJNAME="$(echo $f | sed -e "s/\.c/\.o/" -e "s/src/obj/")"
	$CC $CFLAGS $DEFINES -c $f -o $OBJNAME &
	OBJS="$OBJS $OBJNAME"
done

wait

$CC $LDFLAGS $OBJS -o build/$NAME $LIBS
