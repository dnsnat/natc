#ifndef NATC_H
#define NATC_H

#include <stdint.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include "common.h"

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
    char httpd_port[32];
    char httpd_path[256];
    char username[128];
    char password[128];
    char system[128];
    char release[128];
};

struct frame 
{
    size_t len;
    char data[MAX_MTU + ETHERNET_HEADER];
};

struct tap_cmd_t{
    int sockfd;
    struct sockaddr_in *remote_real;
    struct sockaddr_ll *remote_ll;
    struct tap_login_request_t *request_real;
};

int run_tunnel(struct args *args, sigset_t *orig_mask);
#endif
