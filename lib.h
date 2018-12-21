/*
 * =====================================================================================
 *
 *       Filename:  lib.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  08/15/2018 11:04:04 AM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  YOUR NAME (), 
 *   Organization:  
 *
 * =====================================================================================
 */
#ifndef _LIB_H
#define _LIB_H

int insert_addr(struct sockaddr_in *remote, char *data);
int obtain_addr(struct sockaddr_in *remote, char *data);

typedef enum 
{
    LOGIN_REQUEST, 
    LOGIN_SUCCESS,
    LOGIN_MISTAKE,
    KEEPLIVE_TIME,
}ELOGIN;

struct tap_login_request_t{
    unsigned char r_mac[6];
    unsigned char l_mac[6];
    char type[2];           //{0xF0, 0xF0}
    int version;
    ELOGIN status;
    char username[32];  
    char password[128];
    char tun_ip[32];
};

struct tap_login_respond_t{
    char r_mac[6];
    char l_mac[6];
    char type[2];
    int version;
    ELOGIN status;
};

struct tap_login_thread_t{
    int sockfd;
    struct sockaddr_in  remote_real;
    struct tap_login_request_t request_real;
};

#endif
