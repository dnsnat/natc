#define _GNU_SOURCE

#include <fcntl.h>
#include <net/if.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>


#define __UAPI_DEF_ETHHDR 0
#include <linux/if_tun.h>
#include <netinet/if_ether.h>
#include <stdlib.h>

extern char data_dir[32];
unsigned char macaddr[6]={0}; //ETH_ALEN（6）是MAC地址长度

int get_file_mac()
{
    FILE *stream;
    char macpath[128];
    sprintf(macpath, "%s/_mac", data_dir);

    /*openafileforupdate*/
    stream = fopen(macpath, "r");
    if(!stream)
        return -1;

    fread((char *)macaddr, sizeof(char), sizeof(macaddr), stream);

    fclose(stream);
    return 0;
}

int set_file_mac()
{
    FILE *stream;
    char macpath[128];
    sprintf(macpath, "%s/_mac", data_dir);

    stream = fopen(macpath, "w");
    if(!stream)
        return -1;

    fwrite(macaddr, sizeof(char), sizeof(macaddr), stream);

    fclose(stream);
    return 0;
}

void get_if_mac(char *device)
{
    struct ifreq req;
    int err,i;

    int s=socket(AF_INET,SOCK_DGRAM,0);     //internet协议族的数据报类型套接口
    strcpy(req.ifr_name,device);            //将设备名作为输入参数传入
    req.ifr_hwaddr.sa_family=ARPHRD_ETHER;  //此处需要添加协议，网络所传程序没添加此项获取不了mac。
    err=ioctl(s,SIOCGIFHWADDR,&req);        //执行取MAC地址操作
    close(s);

    if(err != -1)
    { 
        printf("获取mac地址:\n");
        memcpy(macaddr,req.ifr_hwaddr.sa_data,ETH_ALEN); //取输出的MAC地址
        for(i=0;i<ETH_ALEN;i++)
            printf("0x%02x ",macaddr[i]);
    }
    printf("\r\n");
}

void set_if_mac(char *device)
{
    struct ifreq req;
    int err,i;

    int s=socket(AF_INET,SOCK_DGRAM, 0);     //internet协议族的数据报类型套接口

    strcpy(req.ifr_name, device);            //将设备名作为输入参数传入
    req.ifr_hwaddr.sa_family = ARPHRD_ETHER;  //此处需要添加协议，网络所传程序没添加此项获取不了mac。
    memcpy((unsigned char *)req.ifr_hwaddr.sa_data, macaddr, 6);

    err=ioctl(s, SIOCSIFHWADDR, &req);        //执行取MAC地址操作
    close(s);

    if(err != -1)
    { 
        printf("设置mac地址:\n");
        memcpy(macaddr,req.ifr_hwaddr.sa_data,ETH_ALEN); //取输出的MAC地址
        for(i=0;i<ETH_ALEN;i++)
            printf("0x%02x ",macaddr[i]);
    }
    printf("\r\n");
}

int up_iface(char *name) 
{
    struct ifreq req;
    memset(&req, 0, sizeof req);
    req.ifr_flags = IFF_UP;

    if (strlen(name) + 1 >= IFNAMSIZ) {
        fprintf(stderr, "device name is too long: %s\n", name);
        return -1;
    }
    strncpy(req.ifr_name, name, IFNAMSIZ);

    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return sockfd;
    }

    int err = ioctl(sockfd, SIOCSIFFLAGS, &req);
    if (err < 0) {
        perror("ioctl");
        close(sockfd);
        return err;
    }

    close(sockfd);
    return 0;
}

int set_mtu(char *name, unsigned int mtu) 
{
    struct ifreq req;
    memset(&req, 0, sizeof req);
    req.ifr_mtu = mtu;

    if (strlen(name) + 1 >= IFNAMSIZ) {
        fprintf(stderr, "device name is too long: %s\n", name);
        return -1;
    }
    strncpy(req.ifr_name, name, IFNAMSIZ);

    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return sockfd;
    }

    int err = ioctl(sockfd, SIOCSIFMTU, &req);
    if (err < 0) {
        perror("ioctl");
        close(sockfd);
        return err;
    }

    close(sockfd);
    return 0;
}

int create_tap(char *name, char return_name[IFNAMSIZ], unsigned int mtu) 
{
    // https://raw.githubusercontent.com/torvalds/linux/master/Documentation/networking/tuntap.txt
    int fd = open("/dev/net/tun", O_RDWR);
    if (fd < 0) {
        perror("open");
        return fd;
    }

    struct ifreq req;
    memset(&req, 0, sizeof req);
    req.ifr_flags = IFF_TAP | IFF_NO_PI;
    strncpy(req.ifr_name, "natc", IFNAMSIZ);

    if (name) 
    {
        if (strlen(name) + 1 >= IFNAMSIZ) {
            close(fd);
            fprintf(stderr, "device name is too long: %s\n", name);
            return -1;
        }
        strncpy(req.ifr_name, name, IFNAMSIZ);
    }

    int err = ioctl(fd, TUNSETIFF, &req);
    if (err < 0) {
        close(fd);
        perror("ioctl");
        return err;
    }

    strncpy(return_name, req.ifr_name, IFNAMSIZ);
    return_name[IFNAMSIZ - 1] = '\0';

    err = set_mtu(return_name, mtu);
    if (err < 0) {
        close(fd);
        return err;
    }

    /*保证重启应用mac地址不变*/
    get_if_mac(return_name);
    if(!get_file_mac())
        set_if_mac(return_name);
    else
        set_file_mac();

    err = up_iface(return_name);
    if (err < 0) {
        close(fd);
        return err;
    }

    printf("tap device is: %s\n", return_name);
    return fd;
}
