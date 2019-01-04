/*
 * =====================================================================================
 *
 *       Filename:  common.h
 *
 *    Description:  协议公共报文定义
 *
 *        Version:  1.0
 *        Created:  08/15/2018 11:04:04 AM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  疯狂十六进制 
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
    LOGIN_ENQUIRE,
    KEEPLIVE_TIME,
}ECMD;

typedef enum 
{
    LOGIN_FALSE, 
    LOGIN_SUCCESS,
}ELOGIN;

/*隧道协议报文列表*/
struct tap_login_request_t
{
    unsigned char r_mac[6];
    unsigned char l_mac[6];
    char type[2];           //{0xF0, 0xF0}

    ECMD cmd;
    char username[32];  
    char password[128];

    char crc[4];
}__attribute__((packed));

struct tap_login_respond_t
{
    char r_mac[6];
    char l_mac[6];
    char type[2];

    ECMD cmd;
    ELOGIN status;

    char crc[4];
}__attribute__((packed));

struct tap_login_enquire_t
{
    char r_mac[6];
    char l_mac[6];
    char type[2];

    ECMD cmd;

    char crc[4];
}__attribute__((packed));

struct tap_keepalive_t
{
    char r_mac[6];
    char l_mac[6];
    char type[2];

    ECMD cmd;

    char crc[4];
}__attribute__((packed));

#endif
