#######################################################
#
#  2012 Linaro, Sangwook Lee <sangwook.lee@linaro.org>
#  Under GPLv2
#######################################################

CROSS_COMPILE ?= arm-linux-gnueabi-

CC	:= $(CROSS_COMPILE)gcc
CFLAGS	?=  -Wall -I./include  
LDFLAGS	?=  -L./lib -ljpeg

%.o : %.c
	$(CC) $(CFLAGS) -c -o $@ $<

all: linaroca genfw

linaroca: linaroca.o libjpeg.a
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

clean:
	-rm -f *.o
	-rm -f linaroca

