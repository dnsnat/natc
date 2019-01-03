/*
 * =====================================================================================
 *
 *       Filename:  fsm.c
 *
 *    Description:  状态机模块
 *
 *        Version:  1.0
 *        Created:  01/03/2019 09:58:45 AM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  疯狂十六进制 
 *   Organization:  
 *
 * =====================================================================================
 */
#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <ctype.h>

#include <netdb.h>
#include <arpa/inet.h>
#include <net/if.h>    //struct ifreq
#include <sys/ioctl.h>   //ioctl、SIOCGIFADDR
#include <sys/socket.h>
#include <netinet/ether.h>  //ETH_P_ALL
#include <netpacket/packet.h> //struct sockaddr_ll

#include "fsm.h"
#include "natc.h"
#include "iface.h"
#include "./dhcp/dhclient.h"

#define ETH_P_NAT 0xF0F0
//int recv_min = 0;
extern char device[IFNAMSIZ];
extern unsigned char macaddr[6]; //ETH_ALEN（6）是MAC地址长度

struct ifreq ethreq;     //网络接口地址

int Ping(char *server_ip)
{
    pid_t pid;
    int i = 0;
    int stat;
    const int loopCnt = 1; //ping 3 times

    while(i < loopCnt)
    {
        if((pid = vfork()) < 0)
        {
            printf("vfork error!");
            return -1;
        }
        else if(pid == 0)
        {
            if( execlp("ping", "ping", "-c", "1","-W", "3", server_ip, (char *)0) < 0)
            {
                printf("execlp 'ping -c 1 %s' error!\r\n", server_ip);
                return -1;
            }
        }

        waitpid(pid , &stat, WUNTRACED);
        if (stat == 0)
        {
            printf("ping successful pid=%d\r\n", pid);
            return 0;
        }
        else
        {
            printf("ping error!, can not connect to %s\r\n", server_ip);
        }
        usleep(50);
        i++;
    }

    return -1;
}

int tap_cmd_send(struct tap_cmd_t *thread_t)
{
    printf("send cmd\n");

    memcpy(thread_t->request_real->l_mac, macaddr, sizeof(thread_t->request_real->l_mac));

    /*将网络接口赋值给原始套接字地址结构*/
    struct sockaddr_ll sll;     //原始套接字地址结构
    bzero(&sll, sizeof(sll));
    sll.sll_ifindex = ethreq.ifr_ifindex;

    ssize_t n = sendto(thread_t->sockfd, thread_t->request_real, 
            sizeof(struct tap_login_request_t), 0, 
            (struct sockaddr *)&sll, sizeof(sll));
    if (n < 0) {
        perror("sendto username and password");
        return -1;
    } else if (n < sizeof(struct tap_login_request_t)) {
        printf("[login send error] only sent %zd bytes to peer (out of %zd bytes)\n", n, sizeof(struct tap_login_request_t));
        return -1;
    }

    return 0;
}

/*登录认证*/
int tap_login_req(struct tap_cmd_t *thread_t)
{
    struct tap_login_respond_t respond;

    printf("send auth\r\n");
#if 0
    ssize_t n = send(thread_t->sockfd, thread_t->request_real, 
            sizeof(struct tap_login_request_t), 0);
    if (n < 0) {
        perror("sendto username and password");
        return -1;
    } else if (n < sizeof(struct tap_login_request_t)) {
        printf("[login send error] only sent %zd bytes to peer (out of %zd bytes)\n", n, sizeof(struct tap_login_request_t));
        return -1;
    }

    n = recv(thread_t->sockfd, &respond, sizeof(respond), 0);
    if(n != sizeof(struct tap_login_respond_t))
    {
        printf("get login status error n = %ld\n", (long)n);
        return -1;
    }
#else

    ssize_t n = tap_cmd_send(thread_t);
    if (n < 0) {
        printf("send login error n = %ld\n", (long)n);
        return -1;
    }

    n = recvfrom(thread_t->sockfd, &respond, sizeof(respond), 0, NULL, NULL);
    if(n != sizeof(struct tap_login_respond_t))
    {
        printf("get login status error n = %ld\n", (long)n);
        return -1;
    }
#endif

    if(respond.status == LOGIN_SUCCESS)
    {
        printf("auth success!\n");
        return 0;
    }
    else
    {
        printf("auth false !!!\n");
        return -1;
    }
}

/*创建服务器连接*/
int fsm_connect(struct args *args, int sockfd)
{
    /*发起认证*/
    struct tap_login_request_t request = {{0xFF,0xFF,0xFF,0xFF,0xFF,0xFF}, {0,0,0,0,0,0}, {0xF0, 0xF0}, LOGIN_REQUEST};
    sprintf(request.username, args->username);
    sprintf(request.password, args->password);

    struct tap_cmd_t thread_t;
    thread_t.sockfd = sockfd;
    //thread_t.remote_real = remote;
    thread_t.request_real = &request;

    while(tap_login_req(&thread_t))
    {
        printf("login auth fail \r\n");
        if(!args->repeat)
            exit(0);
        sleep(3);
    }

    return 0;
}

void *fsm_connected(void *data)
{
    struct args *args = (struct args *)data;
    /*创建通信用的原始套接字*/
    int sock_raw_fd = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_NAT));

    strncpy(ethreq.ifr_name, device, IFNAMSIZ);   //指定网卡名称
    if(-1 == ioctl(sock_raw_fd, SIOCGIFINDEX, &ethreq)) //获取网络接口
    {
        perror("ioctl");
        close(sock_raw_fd);
        exit(-1);
    }

#if 0
    ethreq.ifr_flags |= IFF_PROMISC;
    if(-1 == ioctl(sock_raw_fd, SIOCGIFINDEX, &ethreq))	//网卡设置混杂模式
    {
        perror("ioctl");
        close(sock_raw_fd);
        exit(-1);
    }
#endif

#if 0
    //获取接口的MAC地址
    if(-1 == ioctl(sock_raw_fd, SIOCGIFHWADDR, &ethreq))
    {
        perror("get dev MAC addr error:");
        exit(1);
    }

    unsigned char src_mac[ETH_ALEN]={0};
    memcpy(src_mac, ethreq.ifr_hwaddr.sa_data, ETH_ALEN);
    printf("MAC :%02X-%02X-%02X-%02X-%02X-%02X\n",src_mac[0],src_mac[1],src_mac[2],src_mac[3],src_mac[4],src_mac[5]);

#endif
    struct sockaddr_ll fromaddr;
    bzero(&fromaddr, sizeof(fromaddr));

    fromaddr.sll_family   = AF_PACKET;
    fromaddr.sll_ifindex  = ethreq.ifr_ifindex;
    fromaddr.sll_protocol = htons(ETH_P_NAT);
    //fromaddr.sll_hatype   = ARPHRD_ETHER;
    //fromaddr.sll_pkttype  = PACKET_HOST;
    //fromaddr.sll_halen    = ETH_ALEN;
    //memcpy(fromaddr.sll_addr,src_mac, ETH_ALEN);

    bind(sock_raw_fd, (struct sockaddr*)&fromaddr,sizeof(struct sockaddr));

    /*连接服务器*/
    fsm_connect(args, sock_raw_fd);

    /*此处必须获取到ip?*/
    int getip = 0;
    while(!getip)
    {
        if(get_dhcp_block(device) != 0)
        {
            sleep(1);
            continue;
        }

        getip = 1;

        sleep(1);
        //if(Ping("10.10.0.1") == 0)
        //{
        //    printf("getip successful\r\n");
        //}
        
        Ping("10.10.0.1");
    }


    /*将网络接口赋值给原始套接字地址结构*/
    struct sockaddr_ll sll;     //原始套接字地址结构
    bzero(&sll, sizeof(sll));
    sll.sll_ifindex = ethreq.ifr_ifindex;

    /*认证和心跳*/
    struct tap_login_request_t request = {{0xFF,0xFF,0xFF,0xFF,0xFF,0xFF}, {0,0,0,0,0,0}, {0xF0, 0xF0}, KEEPLIVE_TIME};
    memcpy(request.l_mac, macaddr, sizeof(request.l_mac));

    struct tap_cmd_t thread_t;
    thread_t.sockfd = sock_raw_fd;
    thread_t.request_real = &request;
    thread_t.remote_ll = &sll;

    /*nat超时-经过测试本地nat没到3分钟就超时了 arp 默认30秒*/
    while(1)
    {
        /*二层认证并保持连接 每隔60S发送一次 三分钟没收到则认为超时*/
        tap_cmd_send(&thread_t);
        sleep(60);  
    }

    return 0;
}

int fsm_init(struct args *args)
{
    /*启动心跳任务*/
    pthread_t tid1;
    if(pthread_create(&tid1, NULL, (void *)fsm_connected, (void *)args))
    {
        printf("create pthread error!\n");
        return -1; 
    }

    /*启动命令接收处理任务*/
#if 0
    pthread_t tid2;
    if(pthread_create(&tid2, NULL, (void *)cmd_recv_handle, NULL))
    {
        printf("create pthread error!\n");
        return -1; 
    }
#endif

    return 0;
}
