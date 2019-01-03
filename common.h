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
    LOGIN_SUCCESS,
    LOGIN_MISTAKE,
    KEEPLIVE_TIME,
}ELOGIN;

struct tap_login_request_t
{
    unsigned char r_mac[6];
    unsigned char l_mac[6];
    char type[2];           //{0xF0, 0xF0}

    ELOGIN status;
    char username[32];  
    char password[128];

    char crc[4];
};

struct tap_login_respond_t
{
    char r_mac[6];
    char l_mac[6];
    char type[2];

    ELOGIN status;

    char crc[4];
};

struct tap_keepalive_t
{
    unsigned char r_mac[6];
    unsigned char l_mac[6];
    char type[2];           //{0xF0, 0xF0}

    ELOGIN status;

    char crc[4];
};

#endif
