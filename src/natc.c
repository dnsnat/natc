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
#include "fsm.h"
#include "iface.h"
#include "dhclient.h"

#define RECV_QUEUE 1024
#define SEND_QUEUE 1024

//int recv_min = 0;
char device[IFNAMSIZ];

extern char exit_wanted;
extern int received_signal;

int setup_socket(char *remote_ip, in_addr_t bind_addr, uint16_t bind_port) 
{
    int sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sockfd < 0) 
    {
        perror("socket");
        return sockfd;
    }

    struct sockaddr_in local;
    memset(&local, 0, sizeof local);
    local.sin_family = AF_INET;
    local.sin_port = htons(bind_port);
    local.sin_addr.s_addr = bind_addr;

    /*绑定本地端口*/
    int bind_result = bind(sockfd, (struct sockaddr *) &local, sizeof local);
    if (bind_result < 0) 
    {
        perror("bind");
        return bind_result;
    }

    /*绑定远程端口*/
    struct sockaddr_in remote;
    memset(&remote, 0, sizeof(struct sockaddr_in));
    remote.sin_family = AF_INET;
    remote.sin_port = htons(32000);
    remote.sin_addr.s_addr = inet_addr(remote_ip);
    if (remote.sin_addr.s_addr == INADDR_NONE) {
        fprintf(stderr, "failed to parse remote: %s\n", remote_ip);
        return -2;
    }

    int con_result = connect(sockfd, (struct sockaddr *)&remote, sizeof(remote));
    if (con_result < 0)
    {
        perror("connect");
        return con_result;
    }

    /*设置连接属性*/
    struct timeval timeout= {10, 0}; //10S 超时
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));

    int i = 1;
    if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &i, sizeof(i))) 
    {
        perror("(create_server) setsockopt(): Error setting sockopt.");
        close(sockfd);
        return -1;
    }

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

int run_tunnel(struct args *args, sigset_t *orig_mask) 
{
    /*创建虚拟接口*/
    int fd = create_tap(args->iface, device, args->mtu);
    if (fd < 0) {
        fprintf(stderr, "unable to create tap device\n");
        return 1;
    }

    /*创建远程连接*/
    int sockfd = setup_socket(args->remote, inet_addr("0.0.0.0"), 32000);
    if (sockfd < 0) {
        fprintf(stderr, "unable to create socket\n");
        return sockfd;
    }

    /*状态机初始化*/
    int status = fsm_init(args);
    if(status < 0){
        fprintf(stderr, "unable to create fsm\n");
        return status;
    }

    // circular queues
    struct frame recv_queue[RECV_QUEUE];
    size_t recv_idx = 0;
    size_t recv_len = 0;

    struct frame send_queue[SEND_QUEUE];
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


            ssize_t n = send(sockfd, f->data, f->len, 0);
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
            //socklen_t l = sizeof(remote);
            ssize_t n = recv(sockfd, &f->data, args->mtu + ETHERNET_HEADER, 0);
            f->len = n;
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
