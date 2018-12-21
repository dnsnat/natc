CFLAGS ?=
LDflag ?=

all:		libdhclient.a

libdhclient.a:	dhclient.o dhcp.o net.o
			$(AR) rcs "$@" $^

dhclient.o:	dhclient.c
			$(CC) $(CFLAGS) -c dhclient.c

dhcp.o:		dhcp.c dhcp.h
			$(CC) $(CFLAGS) -c dhcp.c

net.o:		net.c net.h
			$(CC) $(CFLAGS) -c net.c

clean:
			rm -f *.o

cleanall:   clean
			rm -f dhclient
