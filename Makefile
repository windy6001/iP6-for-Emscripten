# This is a makefile for the Unix/X version of iP6, PC-6000/
# PC-6600 series emulator. Following #defines can be customized:
#
# UNIX      - Should always be present in the Unix version.
# LSB_FIRST - Use this if your CPU has least-significant-byte-first
#             architecture (Intel 80x86, Alpha, etc.).
# DEBUG     - Define this option if you have a little runtime
#             debugger to be compiled into the emulator. It
#             slows down the emulation though.
# MITSHM    - Put this if you are going to use MIT Shared Memory
#             Extensions for XWindows. Using them greatly speeds
#             up screen updates, although they can't be used on
#             remote X-terminals. Note that MITSHM can always
#             be disabled from the command line using -noshm
#             option. 
# SOUND     - Use this option if you have /dev/dsp [present in
#             Linux and FreeBSD] or /dev/audio [SunOS and Solaris]
#             and would like to have sound.
# LINUX     - Define this if you compile sound support on a Linux
#             machine.
# SUN_AUDIO - Define this if you are compiling sound support via
#             SunOS-compatible /dev/audio.

# These are the compiler name, #defines, and the flags used to
# build iP6. Always set flags for the highest possible speed
# optimization. For GCC compiler, it will be
#
#        -O3 -fomit-frame-pointer -funroll-loops
#
# If you are getting errors about not found X11 header files,
# change the -I/usr/X11R6/include to the directory where X11
# headers are located on your system.
# If you are getting linker errors about not found X11 functions,
# change the -L/usr/X11R6/lib to the directory where X11
# libraries libX11.* and libXext.* are located on your system.

TARGET=SDL

ifeq ($(TARGET),SDL)
CC = g++
else
CC = emcc
endif

# --- DEFINES ---
DEFINES = -DUNIX -DLSB_FIRST -DDEBUG
# XAW_CFLAGS  = -I/usr/X11/include

# --- CFLAGS ---
ifeq ($(TARGET),SDL)
# --- SDL -------
CFLAGS = -g -O3 -fomit-frame-pointer -funroll-loops 	`sdl2-config --cflags` ${DEFINES} ${XAW_CFLAGS}
else
# --- emscripten ---
CFLAGS = -s USE_SDL=2 --preload-file rom ${DEFINES}
endif

CXXFLAGS = ${CFLAGS}

# --- LDLIBS ---
# --- SDL ------
LDLIBS  = 	`sdl2-config --libs`

OBJECTS = iP6.o P6.o Z80.o Debug.o Unix.o Refresh.o Sound.o sdl.o mmu.o \
		crtc.o sub.o audio.o 

ifeq ($(TARGET),SDL)
OUTPUTFILE= iP6
else
OUTPUTFILE= iP6.html
endif

# Make the standard distribution: iP6
all:	${OUTPUTFILE}	


${OUTPUTFILE}:	${OBJECTS}
	${CC} ${CFLAGS} -o ${OUTPUTFILE} ${OBJECTS} ${LDLIBS}

# Clean up.
clean:
	rm -f *.o *~ iP6 iP6.html iP6.data iP6.js iP6.wasm

# Dependencies for the object files.
iP6.o:		iP6.cpp P6.h Z80.h Help.h
P6.o:		P6.cpp P6.h Z80.h
Z80.o:		Z80.c Z80.h Codes.h CodesED.h CodesCB.h CodesXX.h Tables.h CodesXCB.h
Unix.o:		Unix.c P6.h Z80.h Keydef.h
Refresh.o:	Refresh.cpp Unix.h
Sound.o:	Sound.c P6.h
Debug.o:	Debug.cpp Z80.h
#Xconf.o:	Xconf.c Xconf.h Xconfdef.h P6.h
sdl.o:	sdl.cpp Keydef.h
mmu.o:	mmu.cpp mmu.h
sub.o:	sub.cpp sub.h
crtc.o: crtc.cpp crtc.h

