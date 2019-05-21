/*
 * =====================================================================================
 *
 *       Filename:  jsonrpc.c
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  05/21/2019 10:03:37 AM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  YOUR NAME (), 
 *   Organization:  
 *
 * =====================================================================================
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "jsonrpc.h"

#if 0
int jsonrpc_socket()
{
    int sockfd;
    struct sockaddr_in servaddr;

    sockfd = socket(AF_INET, SOCK_DGRAM, 0); /* create a socket */

    /* init servaddr */
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family      = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port        = htons(JSONRPC_PORT);

    /* bind address and port to socket */
    if(bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) == -1)
    {
        perror("bind error");
        exit(1);
    }

    return sockfd;
}

int jsonrpc_task(int sockfd)
{
    int n;
    socklen_t len;
    char mesg[MAXLINE];
    struct sockaddr client_fd;

    while(1)
    {
        len = clilen;
        /* waiting for receive data */
        n = recvfrom(sockfd, mesg, MAXLINE, 0, client_fd, &len);
        /* sent data back to client */
        sendto(sockfd, mesg, n, 0, pcliaddr, len);
    }

    return 0;
}

int jsonrcp_thread()
{
    int ret=0;
    pthread_t tid1;
    //pthread_mutex_init(&rpc_lock, NULL);  
    ret = pthread_create(&tid1, NULL, jsonrpc_task, NULL);
    if(ret)
    {
        printf("create file rpc error!\n");
        return -1; 
    }

    printf("create file rpc successfully!\n");
    return ret;
}

#endif
