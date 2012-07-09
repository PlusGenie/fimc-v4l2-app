CROSS_COMPILE ?= arm-linux-gnueabi-

CC	:= $(CROSS_COMPILE)gcc
CFLAGS	?=  -Wall -I./include  
LDFLAGS	?=  -static -L./lib -ljpeg

%.o : %.c
	$(CC) $(CFLAGS) -c -o $@ $<

all: linaroca

linaroca: linaroca.o libjpeg.a
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

clean:
	-rm -f *.o
	-rm -f linaroca

