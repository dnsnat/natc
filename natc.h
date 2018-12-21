#ifndef NATC_H
#define NATC_H

#include <netinet/in.h>
#include <stdint.h>

#define MAX_MTU 1500
#define ETHERNET_HEADER 14

struct args 
{
    char *iface;
    char *remote;
    char *up_script;
    char *down_script;
    int uid;
    int gid;
    unsigned int mtu;
    unsigned int repeat;
    char username[32];
    char password[128];
};

struct frame 
{
    size_t len;
    char data[MAX_MTU + ETHERNET_HEADER];
};

int run_tunnel(struct args *args, sigset_t *orig_mask);
#endif
