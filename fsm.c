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

int getip = 0;
int sock_raw_fd;
struct ifreq ethreq;     //网络接口地址

int tap_cmd_send(char *buf, int len)
{
    printf("send cmd\n");
    if(len < 14)
    {
        printf("cmd len error\n");
        return -1;
    }

    /*此处应该添加crc校验*/
    memcpy(buf + 6, macaddr, sizeof(macaddr));

    /*将网络接口赋值给原始套接字地址结构*/
    struct sockaddr_ll sll;     //原始套接字地址结构
    bzero(&sll, sizeof(sll));
    sll.sll_ifindex = ethreq.ifr_ifindex;
    ssize_t n = sendto(sock_raw_fd, buf, len, 0, 
            (struct sockaddr *)&sll, sizeof(sll));
    if (n < 0) {
        perror("sendto cmd");
        return -1;
    } else if (n < len) {
        printf("[cmd send error] only sent %zd bytes to peer (out of %d bytes)\n", n, len);
        return -1;
    }

    return 0;
}

int tap_cmd_recv(char *buf, int len)
{
    printf("recv cmd\n");
    int n = recv(sock_raw_fd, buf, len, 0);
    if(n <= 0 && n != -1)
        printf("recv cmd error n = %ld\n", (long)n);
    /*此处应该添加crc校验*/
    return n;
}

/*首次登录认证协议*/
int tap_login_req(struct tap_login_request_t *request)
{
    ssize_t n;
    struct tap_login_respond_t respond;

    printf("send auth\r\n");

    /*发送认证*/
    n = tap_cmd_send((char *)request, sizeof(struct tap_login_request_t));
    if (n < 0) 
    {
        printf("send login error n = %ld\n", (long)n);
        return -1;
    }

    /*接收回复*/
    n = tap_cmd_recv((char *)&respond, sizeof(respond));
    if(n != sizeof(struct tap_login_respond_t))
    {
        printf("get login status error n = %ld\n", (long)n);
        return -1;
    }

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

/*登录服务器*/
int fsm_login(struct args *args)
{
    /*发起认证*/
    struct tap_login_request_t request = {{0xFF,0xFF,0xFF,0xFF,0xFF,0xFF}, {0,0,0,0,0,0}, {0xF0, 0xF0}, LOGIN_REQUEST};
    sprintf(request.username, args->username);
    sprintf(request.password, args->password);
    sprintf(request.system, args->system);
    sprintf(request.release, args->release);

    while(tap_login_req(&request))
    {
        printf("login auth fail \r\n");
        if(!args->repeat)
            exit(0);
        sleep(3);
    }

    return 0;
}

/*创建协议socket*/
int fsm_socket() 
{
    /*创建协议的原始套接字, 走虚拟接口.
     * 协议走虚拟接口,不走udp接口好处是l2更加干净.
     * 自定义协议是没有以太网头部的 
     * 因为不知道如何填充mac信息
     * PF_PACKET也不支持connect
     * 只能用sendto*/
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

    /*bind后recvfrom可改为recv*/
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

    /*设置连接属性*/
    struct timeval timeout= {10, 0}; //10S 超时
    setsockopt(sock_raw_fd, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));


    return sock_raw_fd;
}

/*keepalive*/
void *fsm_keep_task(void *data)
{
    struct args *args = (struct args *)data;

    /*登录服务器*/
    fsm_login(args);

    /*获取到ip*/
    while(!getip)
    {
        if(get_dhcp_block(device) != 0)
        {
            sleep(1);
            continue;
        }

        sleep(1);

        int n = system("ping 10.10.0.1 -c 1 -w 3");
        printf("n == %d\n", n);
        if(n == 0)
        {
            getip = 1;
            printf("getip successful\r\n");
        }
    }

    /*心跳保活*/
    while(1)
    {
        /*nat超时-经过测试本地nat没到3分钟就超时了 arp 默认30秒*/
        /*二层认证并保持连接 每隔60S发送一次 三分钟没收到则认为超时*/
        struct tap_keepalive_t keep = {{0xFF,0xFF,0xFF,0xFF,0xFF,0xFF}, {0,0,0,0,0,0}, {0xF0, 0xF0}, KEEPLIVE_TIME};
        tap_cmd_send((char *)&keep, sizeof(keep));
        sleep(60);  
    }

    return NULL;
}

/*协议包全部在此接收*/
void *fsm_recv_task(void *data)
{
    struct args *args = (struct args *)data;
    char buf[128];
    while(1)
    {
        if(!getip)
        {
            sleep(1);
            continue;
        }

        int n = tap_cmd_recv(buf, sizeof(buf));
        if(n < 18)
            continue;

        int cmd = 0;
        memcpy(&cmd, buf + 14, sizeof(int));
        switch (cmd)
        {
            case LOGIN_ENQUIRE:
                printf("recv LOGIN_ENQUIRE\n");
                fsm_login(args);
                sleep(1);
                for(int i = 0; i < 10; i++)
                {
                    if(system("ping 10.10.0.1 -c 1 -w 3") == 0)
                        break;
                    sleep(1);
                }

                break;
            default:
                break;
        }
    }

    return NULL;
}

int fsm_init(struct args *args)
{
    /*创建socket*/
    sock_raw_fd = fsm_socket();

    /*启动心跳任务*/
    pthread_t tid1;
    if(pthread_create(&tid1, NULL, (void *)fsm_keep_task, (void *)args))
    {
        printf("create pthread error!\n");
        return -1; 
    }

    /*启动命令接收处理任务*/
    pthread_t tid2;
    if(pthread_create(&tid2, NULL, (void *)fsm_recv_task, (void *)args))
    {
        printf("create pthread error!\n");
        return -1; 
    }

    return 0;
}
