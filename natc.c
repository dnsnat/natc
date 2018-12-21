#define _GNU_SOURCE

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

#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>

#include "natc.h"
#include "iface.h"
#include "lib.h"
#include "./dhcp/dhclient.h"

#define RECV_QUEUE 1024
#define SEND_QUEUE 1024

//int recv_min = 0;
char device[IFNAMSIZ];

extern unsigned char macaddr[6]; //ETH_ALEN（6）是MAC地址长度
extern unsigned char if_ip[32];

extern char exit_wanted;
extern int received_signal;

int setup_socket(in_addr_t bind_addr, uint16_t bind_port) 
{
    int sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sockfd < 0) {
        perror("socket");
        return sockfd;
    }

    struct sockaddr_in local;
    memset(&local, 0, sizeof local);
    local.sin_family = AF_INET;
    local.sin_port = htons(bind_port);
    local.sin_addr.s_addr = bind_addr;

    int bind_result = bind(sockfd, (struct sockaddr *) &local, sizeof local);
    if (bind_result < 0) {
        perror("bind");
        return bind_result;
    }

    struct timeval timeout= {10, 0}; //10S 超时
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));

    return sockfd;
}

int run_updown(char *script, char *device) 
{
    pid_t pid = fork();
    if (pid == 0) {  // child
        execlp(script, script, device, NULL);
        // exec failed
        perror("exec");
        exit(1);
    } else if (pid > 0) {  // parent
        int ret;
        waitpid(pid, &ret, 0);
        return WEXITSTATUS(ret);
    } else {
        perror("fork");
        // TODO: exit from here is bad
        exit(1);
    }
}

int drop_privileges(int uid, int gid) 
{
    if (setgid(gid) != 0) {
        perror("setgid");
        return 1;
    }
    if (setuid(uid) != 0) {
        perror("uetgid");
        return 1;
    }
    return 0;
}

#if 0
/*考虑废弃 因为心跳包不需要回应*/
int tap_auth_send_timer(struct tap_login_thread_t *thread_t)
{
    struct sockaddr *remote = (struct sockaddr *)&thread_t->remote_real;
    struct tap_login_request_t *request = (struct tap_login_request_t *)&thread_t->request_real;

    memcpy(request->l_mac, macaddr, sizeof(request->l_mac));

    ssize_t n = sendto  (thread_t->sockfd, request, sizeof(struct tap_login_request_t), 0, (struct sockaddr *)remote, sizeof(struct sockaddr));
    if (n < 0) {
        perror("sendto username and password");
        return -1;
    } else if (n < sizeof(struct tap_login_request_t)) {
        printf("[login send error] only sent %zd bytes to peer (out of %zd bytes)\n", n, sizeof(struct tap_login_request_t));
        return -1;
    }

    time_t now; //实例化time_t结构    
    struct tm *timenow; //实例化tm结构指针    
    time(&now);   
    timenow = localtime(&now);   
    printf("%s:send keepalive request\r\n", asctime(timenow));
    return 0;
}

int tap_auth_recv_timer()
{
    time_t now;         //实例化time_t结构    
    struct tm *recv_time; //实例化tm结构指针    
    time(&now);   
    recv_time = localtime(&now);   
    printf("%s:recv keepalive request\r\n", asctime(recv_time));
    recv_min = recv_time->tm_min;
    return 0;
}
#endif

int tap_auth_login(struct tap_login_thread_t *thread_t)
{
    struct sockaddr *remote = (struct sockaddr *)&thread_t->remote_real;
    struct tap_login_request_t *request = (struct tap_login_request_t *)&thread_t->request_real;

    struct tap_login_respond_t respond;
    memcpy(request->l_mac, macaddr, sizeof(request->l_mac));

    printf("send auth\r\n");
    ssize_t n = sendto  (thread_t->sockfd, request, sizeof(struct tap_login_request_t), 0, (struct sockaddr *)remote, sizeof(struct sockaddr));
    if (n < 0) {
        perror("sendto username and password");
        return -1;
    } else if (n < sizeof(struct tap_login_request_t)) {
        printf("[login send error] only sent %zd bytes to peer (out of %zd bytes)\n", n, sizeof(struct tap_login_request_t));
        return -1;
    }

    socklen_t l = sizeof(struct sockaddr);
    n = recvfrom(thread_t->sockfd, &respond, sizeof(respond), 0, remote, &l);
    if(n != sizeof(struct tap_login_respond_t))
    {
        printf("get login status error n = %ld\n", (long)n);
        return -1;
    }
    memcpy(request->r_mac, respond.l_mac, sizeof(request->r_mac));
    memcpy(&thread_t->remote_real, remote, sizeof(thread_t->remote_real));

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

int tap_keepalive(struct tap_login_thread_t *thread_t)
{
    struct sockaddr *remote = (struct sockaddr *)&thread_t->remote_real;
    struct tap_login_request_t *request = (struct tap_login_request_t *)&thread_t->request_real;

    memcpy(request->l_mac, macaddr, sizeof(request->l_mac));
    memcpy(request->tun_ip, if_ip, sizeof(request->tun_ip));
    request->status = KEEPLIVE_TIME;

    printf("send keepalive\r\n");
    ssize_t n = sendto  (thread_t->sockfd, request, sizeof(struct tap_login_request_t), 0, (struct sockaddr *)remote, sizeof(struct sockaddr));
    if (n < 0) {
        perror("send keepalive");
        return -1;
    } else if (n < sizeof(struct tap_login_request_t)) {
        printf("[keepalive send error] only sent %zd bytes to peer (out of %zd bytes)\n", n, sizeof(struct tap_login_request_t));
        return -1;
    }

    return 0;
}

int NetPing(char *server_ip)
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

void *tap_auth_keeplive_timer(void *data)
{
    struct tap_login_thread_t *thread_t = data;

    /*此处必须获取到ip?*/
    int getip = 0;
    while(!getip)
    {
        if(get_dhcp_block(device) != 0)
        {
            sleep(1);
            continue;
        }

        for(int i = 0; i < 3; i++)
        {
            if(NetPing("10.10.0.1") == 0)
            {
                printf("getip successful\r\n");
                getip = 1;
            }
        }
    }

    if(!getip)
    {
        printf("get ip fail \r\n");
        exit(0);
    }

    /*nat超时-经过测试本地nat没到3分钟就超时了 arp 默认30秒*/
    while(1)
    {

        /*二层认证并保持连接 每隔60S发送一次 三分钟没收到则认为超时*/
        tap_keepalive(thread_t);
#if 0
        struct tm *now_time; //实例化tm结构指针    
        time_t now;         //实例化time_t结构    
        time(&now);   
        now_time = localtime(&now);   
        tap_auth_send_timer(thread_t);
        if((now_time->tm_min - recv_min) > 2)
        {
            printf("连接超时 ...\r\n");
            sleep(10); 
            continue;
        }
#endif

#if 0
        if(NetPing("10.10.0.1"))
        {
            printf("ping超时 ...\r\n");
            //run_dhcp_thread("tap0");
            sleep(10);
            continue;
        }
#endif

        sleep(60);  
    }
    return 0;
}

int tap_auth_keeplive_process(struct tap_login_thread_t *thread_t)
{
    pthread_t tid;
    printf("[%d]%s\r\n", __LINE__, __func__);
    if(pthread_create(&tid,NULL,(void *)tap_auth_keeplive_timer, thread_t))
    {
        printf("create pthread error!\n");
        return -1; 
    }
    return 0;
}

int socket_connect(struct args *args, struct sockaddr_in *remote)
{
    // TODO: make the port and bind address configurable
    int sockfd = setup_socket(inet_addr("0.0.0.0"), 32000);
    if (sockfd < 0) {
        fprintf(stderr, "unable to create socket\n");
        return sockfd;
    }

    /*认证和心跳*/
    struct tap_login_request_t request = {{0xFF,0xFF,0xFF,0xFF,0xFF,0xFF}, {0,0,0,0,0,0}, {0xF0, 0xF0}, 1, LOGIN_REQUEST, "admin", "admin"};
    sprintf(request.username, args->username);
    sprintf(request.password, args->password);

    struct tap_login_thread_t *thread_t = malloc(sizeof(struct tap_login_thread_t));

    thread_t->sockfd = sockfd;
    memcpy(&thread_t->remote_real, remote, sizeof(struct sockaddr_in));
    memcpy(&thread_t->request_real, &request, sizeof(struct tap_login_request_t));
    while(tap_auth_login(thread_t))
    {
        printf("login auth fail \r\n");
        if(!args->repeat)
            exit(0);
        sleep(3);
    }

    tap_auth_keeplive_process(thread_t);
    return sockfd;
}

int run_tunnel(struct args *args, sigset_t *orig_mask) 
{
    struct sockaddr_in remote;
    memset(&remote, 0, sizeof remote);

    remote.sin_family = AF_INET;
    remote.sin_port = htons(32000);
    remote.sin_addr.s_addr = inet_addr(args->remote);

    if (remote.sin_addr.s_addr == INADDR_NONE) {
        fprintf(stderr, "failed to parse remote: %s\n", args->remote);
        return 2;
    }
    fprintf(stderr, "running in client mode with remote: %s\n", args->remote);

    /*连接服务器*/
    int sockfd = socket_connect(args, &remote);
    if(sockfd < 0)
    {
        printf("connect server error\n");
        return sockfd;
    }

    /*创建虚拟接口*/
    int fd = create_tap(args->iface, device, args->mtu);
    if (fd < 0) {
        fprintf(stderr, "unable to create tap device\n");
        return 1;
    }

    // circular queues
    struct frame recv_queue[RECV_QUEUE] = {0};
    size_t recv_idx = 0;
    size_t recv_len = 0;

    struct frame send_queue[SEND_QUEUE] = {0};
    size_t send_idx = 0;
    size_t send_len = 0;

    struct timespec tm;
    memset(&tm, 0, sizeof tm);
    tm.tv_nsec = 10000000;  // 0.01 seconds

    struct pollfd fds[2];
    memset(&fds, 0, sizeof fds);
    fds[0].fd = fd;
    fds[1].fd = sockfd;

    if (args->up_script) 
    {
        int ret = run_updown(args->up_script, device);
        if (ret != 0) {
            fprintf(stderr, "up script exited with status: %d\n", ret);
            return 1;
        }
    }

    for (;;) 
    {
        fds[0].events = POLLIN;
        if (recv_len > 0) {
            fds[0].events |= POLLOUT;
        }

        fds[1].events = POLLIN;
        if (send_len > 0) {
            fds[1].events |= POLLOUT;
        }

        int result = ppoll(fds, 2, &tm, orig_mask);
        if (result < 0) {
            if (errno != EINTR) {
                perror("ppoll");
                return 3;
            }
        }

        if (exit_wanted) 
        {
            fprintf(stderr, "\nreceived signal %d, stopping tunnel\n", received_signal);
            break;
        }

        // tap can handle a write
        if (fds[0].revents & POLLOUT) 
        {
            struct frame *f = &recv_queue[recv_idx];
            assert(f->len <= args->mtu + ETHERNET_HEADER);
            recv_idx = (recv_idx + 1) % RECV_QUEUE;
            recv_len -= 1;

            ssize_t n = write(fd, f->data, f->len);
            if (n < 0) {
                if (errno == EINVAL) {
                    fprintf(stderr, "received garbage frame\n");
                } else {
                    perror("write");
                    return 4;
                }
            } else if (n < f->len) {
                printf("[error] only wrote %zd bytes to tap (out of %zd bytes)\n", n, f->len);
            }
        }

        // udp socket can handle a write
        if (fds[1].revents & POLLOUT) 
        {
            struct frame *f = &send_queue[send_idx];
            assert(f->len <= args->mtu + ETHERNET_HEADER);
            send_idx = (send_idx + 1) % SEND_QUEUE;
            send_len -= 1;


            ssize_t n = sendto(sockfd, f->data, f->len, 0, (struct sockaddr *) &remote, sizeof remote);
            if (n < 0) {
                perror("sendto");
                //return 4;
            } else if (n < f->len) {
                printf("[error] only sent %zd bytes to peer (out of %zd bytes)\n", n, f->len);
            }
        }

        // tap has data for us to read
        if (fds[0].revents & POLLIN) 
        {
            size_t idx = (send_idx + send_len) % SEND_QUEUE;

            if (send_len < SEND_QUEUE) {
                send_len += 1;
            } else {
                assert(send_len == SEND_QUEUE);
                printf("dropping frame from send queue\n");

                // put this packet at the end of the queue;
                // drop the first frame in the queue
                send_idx += 1;
            }

            struct frame *f = &send_queue[idx];
            memset(f, 0, sizeof(struct frame));
            ssize_t n = read(fd, &f->data, args->mtu + ETHERNET_HEADER);
            f->len = n;
        }

        // udp socket has data for us to read
        if (fds[1].revents & POLLIN) 
        {
            size_t idx = (recv_idx + recv_len) % RECV_QUEUE;

            if (recv_len < RECV_QUEUE) {
                recv_len += 1;
            } else {
                assert(recv_len == RECV_QUEUE);
                printf("dropping frame from recv queue\n");

                // put this packet at the end of the queue;
                // drop the first frame in the queue
                recv_idx += 1;
            }

            struct frame *f = &recv_queue[idx];
            memset(f, 0, sizeof(struct frame));

            // TODO: handle case where remote changes, in both server+client mode
            socklen_t l = sizeof(remote);
            ssize_t n = recvfrom(
                    sockfd,
                    &f->data,
                    args->mtu + ETHERNET_HEADER,
                    0,
                    (struct sockaddr *) &remote,
                    &l
                    );
            f->len = n;

            //打印keepalive信息
            if((unsigned char)f->data[12] == 0xF0 && (unsigned char)f->data[13] == 0xF0)
            {
                //tap_auth_recv_timer();
                printf("received cmd\n");
            }
        }

    }

    if (args->down_script) 
    {
        int ret = run_updown(args->down_script, device);
        if (ret != 0) {
            fprintf(stderr, "down script exited with status: %d\n", ret);
            return 1;
        }
    }

    return 0;
}
