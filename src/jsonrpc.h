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
#include "onion/onion.h"
#include "onion/dict.h"
#include "onion/block.h"
#include "onion/request.h"
#include "onion/response.h"
#include "onion/url.h"
#include "onion/shortcuts.h"
#include "onion/log.h"
#include "onion/types.h"
#include "onion/types_internal.h"

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

onion_connection_status strip_rpc(void *_, onion_request * req, onion_response * res);
onion_connection_status http_jsonrpc_status(void *_, onion_request * req, onion_response * res);

#endif
