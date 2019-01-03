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
#ifndef _COMMON_H
#define _COMMON_H

typedef enum 
{
    LOGIN_REQUEST, 
    LOGIN_RESPOND,
    KEEPLIVE_TIME,
}ECMD;

typedef enum 
{
    LOGIN_FALSE, 
    LOGIN_SUCCESS,
}ELOGIN;

struct tap_login_request_t
{
    unsigned char r_mac[6];
    unsigned char l_mac[6];
    char type[2];           //{0xF0, 0xF0}

    ECMD cmd;
    char username[32];  
    char password[128];

    char crc[4];
};

struct tap_login_respond_t
{
    char r_mac[6];
    char l_mac[6];
    char type[2];

    ECMD cmd;
    ELOGIN status;

    char crc[4];
};

/*协议类型*/
struct login_req_t
{
    ELOGIN status;
    char username[32];  
    char password[128];
};

struct login_res_t
{
    ELOGIN status;
};

struct tap_keepalive_t
{
    char r_mac[6];
    char l_mac[6];
    char type[2];

    ELOGIN status;

    char crc[4];
};

#endif
