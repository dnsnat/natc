/*
 * =====================================================================================
 *
 *       Filename:  jsonrpc.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  05/21/2019 10:18:22 AM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  YOUR NAME (), 
 *   Organization:  
 *
 * =====================================================================================
 */

#ifndef _JSONRPC_H
#define _JSONRPC_H

#define JSONRPC_PORT 20003
#define MAXBUF       128

typedef enum 
{
    GET_STATUS, 

}ERPC;

struct rpc_request_t
{
    ERPC cmd;

    char crc[4];
}__attribute__((packed));

int jsonrpc_thread();

int http_jsonrpc_thread();

#endif
