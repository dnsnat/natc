CC ?= gcc
AR ?= ar

machine := $(shell $(CC) -dumpmachine)

#-L./ -Wl,--whole-archive -lresource -Wl,--no-whole-archive
LDFLAGS := -lonion_static -ljansson -lpthread
CFLAGS  := 
#LDFLAGS := -Wl,-dn -lonion_static -ljansson -Wl,-dy -lpthread #--static 
#CFLAGS  := -std=gnu99 -Wall -Werror  -O3 -fvisibility=hidden -s -Wno-unused-result -Wno-maybe-uninitialized -ffunction-sections -fdata-sections  -Wl,-gc-sections 

C_SRCS  := $(wildcard *.c)
C_OBJS  := $(C_SRCS:.c=.o) ./dhcp/libdhclient.a ./app/jsonrpc/libjsonrpc.a ./app/ssh/libssh.a

$(machine)-natc: $(C_OBJS) 
		$(CC) $^ -o "$@" $(LDFLAGS) $(CFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c "$<"

./dhcp/libdhclient.a: ./dhcp
	make -C ./dhcp/ CFLAGS="$(CFLAGS)"

./app/jsonrpc/libjsonrpc.a: ./app/jsonrpc
	make -C $< CFLAGS="$(CFLAGS)"

./app/ssh/libssh.a: ./app/ssh
	make -C $< CFLAGS="$(CFLAGS)"

#libresource.a: www/*
#	./bintoobj www

.PHONY: clean
clean:
	rm -f *.o
	rm -f *.a
	rm -f  $(machine)-natc
	make -C ./dhcp/ clean
	make -C ./app/jsonrpc/ clean
	make -C ./app/ssh/ clean

#A=b  基本变量
#A:=b 立即变量  覆盖之前的值
#A?=b 延时变量  不覆盖之前的值
#A+=b  A=a A+=b A==a b 


