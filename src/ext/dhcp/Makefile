CFLAGS ?=
LDflag ?=

C_SRCS  := $(wildcard *.c)
C_OBJS  := $(C_SRCS:.c=.o)

libdhclient.a: $(C_OBJS) 
	$(AR) rcs "$@" $^

%.o: %.c
	$(CC) $(CFLAGS) -c "$<"

clean:
	rm -f *.o
	rm -f libdhclient.a

