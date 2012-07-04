CROSS_COMPILE ?= arm-linux-gnueabi-

CC	:= $(CROSS_COMPILE)gcc
CFLAGS	?= -O2 -W -Wall 
LDFLAGS	?= --static 

%.o : %.c
	$(CC) $(CFLAGS) -c -o $@ $<

all: linaroca

linaroca: linaroca.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

clean:
	-rm -f *.o
	-rm -f linaroca

