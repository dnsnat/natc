#ifndef IFACE_H
#define IFACE_H

#include <net/if.h>
int create_tap(char *name, char return_name[IFNAMSIZ], unsigned int mtu);
void get_if_mac(char *device);

#endif
