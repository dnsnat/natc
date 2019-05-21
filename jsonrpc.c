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
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <limits.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <pwd.h>
#include <time.h>
#include <pthread.h>
#include <netdb.h>
#include <sys/sysinfo.h>
#include <sys/utsname.h>

#include <sys/socket.h>
#include <netinet/in.h>

#include "common.h"
#include "sysinfo.h"
#include "compat_getpwuid.h"

#include "cJSON.h"
#include "jsonrpc.h"

int sockfd;
int jsonrpc_socket()
{
    int sockfd;
    struct sockaddr_in servaddr;

    /* create a socket */
    sockfd = socket(AF_INET, SOCK_DGRAM, 0); 

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

cJSON *jsonrpc_mem()
{
    cJSON *array = NULL;
    array = cJSON_CreateArray();

    MEM_OCCUPY mem[4];
    get_memoccupy(mem); //对无类型get函数含有一个形参结构体类弄的指针O

    cJSON_AddItemToArray(array, cJSON_CreateNumber(mem[0].total/1024));
    cJSON_AddItemToArray(array, cJSON_CreateNumber(mem[1].total/1024));
    cJSON_AddItemToArray(array, cJSON_CreateNumber(mem[2].total/1024));
    cJSON_AddItemToArray(array, cJSON_CreateNumber(mem[3].total/1024));

    return array;
}

cJSON *jsonrpc_swap()
{
    cJSON *array = NULL;
    array = cJSON_CreateArray();

    MEM_OCCUPY mem[4];
    get_swapoccupy(mem); //对无类型get函数含有一个形参结构体类弄的指针O

    cJSON_AddItemToArray(array, cJSON_CreateNumber(mem[0].total/1024));
    cJSON_AddItemToArray(array, cJSON_CreateNumber(mem[1].total/1024));
    cJSON_AddItemToArray(array, cJSON_CreateNumber(mem[2].total/1024));
    cJSON_AddItemToArray(array, cJSON_CreateNumber(mem[3].total/1024));

    return array;
}

extern struct st_disk st_disks[12];
cJSON *jsonrpc_disk()
{
    int i, disk_num = 0;

    cJSON *array = NULL;
    cJSON *array_sub = NULL;

    array = cJSON_CreateArray();

    get_diskinfo(&disk_num); //对无类型get函数含有一个形参结构体类弄的指针O

    for(i = 0; i < disk_num && i < 12; i++)
    {
        array_sub = cJSON_CreateArray();

        cJSON_AddItemToArray(array_sub, cJSON_CreateString(st_disks[i].name));
        cJSON_AddItemToArray(array_sub, cJSON_CreateString(st_disks[i].size));
        cJSON_AddItemToArray(array_sub, cJSON_CreateString(st_disks[i].used));
        cJSON_AddItemToArray(array_sub, cJSON_CreateString(st_disks[i].free));
        cJSON_AddItemToArray(array_sub, cJSON_CreateString(st_disks[i].UseAvg));

        cJSON_AddItemToArray(array, array_sub);
    }
    return array;
}

cJSON *jsonrpc_time()
{
    time_t now; //实例化time_t结构    
    struct tm *timenow; //实例化tm结构指针    
    time(&now);   
    timenow = localtime(&now);   

    cJSON *root = NULL;
    root = cJSON_CreateObject();

    cJSON_AddStringToObject(root, "Time", asctime(timenow));
    return root;
}

cJSON *jsonrpc_user()
{
    cJSON *array = NULL;
    array = cJSON_CreateArray();

    struct passwd *user;
    user= compat_getpwuid(getuid());
    if (user)
        cJSON_AddItemToArray(array, cJSON_CreateString(user->pw_name));
    else
        cJSON_AddItemToArray(array, cJSON_CreateString("null"));

    return array;
}

cJSON *jsonrpc_uptime()
{
    cJSON *array = NULL;
    array = cJSON_CreateArray();

    struct sysinfo info;
    //time_t boot_time = 0;
    char boot_time_s[128];
    if (sysinfo(&info)) {
        cJSON_AddItemToArray(array, cJSON_CreateString("00:00:00"));
        cJSON_AddItemToArray(array, cJSON_CreateString("null"));
        cJSON_AddItemToArray(array, cJSON_CreateString("0.00"));
        cJSON_AddItemToArray(array, cJSON_CreateString("0.00"));
        cJSON_AddItemToArray(array, cJSON_CreateString("0.00"));
        return array;
    }

    //struct tm *ptm = NULL;
    localtime((const time_t *)&info.uptime);
    sprintf(boot_time_s, "%ld天%ld小时%ld分%ld秒", info.uptime/86400,
            info.uptime%86400/3600, info.uptime%86400%3600/60, info.uptime%86400%3600%60);

    cJSON_AddItemToArray(array, cJSON_CreateString(boot_time_s));
    cJSON_AddItemToArray(array, cJSON_CreateString("1user"));
    cJSON_AddItemToArray(array, cJSON_CreateString("0.84"));
    cJSON_AddItemToArray(array, cJSON_CreateString("0.66"));
    cJSON_AddItemToArray(array, cJSON_CreateString("0.47"));

    return array;
}

extern struct st_net st_nets[12];
cJSON *jsonrpc_network()
{
    int net_num = 0;
    int i;
    cJSON *array = NULL;
    cJSON *array_sub = NULL;
    array = cJSON_CreateArray();

    get_netinfo(&net_num); //对无类型get函数含有一个形参结构体类弄的指针O

    for(i = 0; i < net_num && i < 12; i++)
    {
        array_sub = cJSON_CreateArray();

        cJSON_AddItemToArray(array_sub, cJSON_CreateString(st_nets[i].name));
        cJSON_AddItemToArray(array_sub, cJSON_CreateString(st_nets[i].rx_packet));
        cJSON_AddItemToArray(array_sub, cJSON_CreateString(st_nets[i].tx_packet));
        cJSON_AddItemToArray(array_sub, cJSON_CreateString(st_nets[i].rx_byte));
        cJSON_AddItemToArray(array_sub, cJSON_CreateString(st_nets[i].tx_byte));

        cJSON_AddItemToArray(array, array_sub);
    }

    return array;
}

extern int mpstat_main(int argc, char **argv);
struct double_stats_cpu {
    double cpu_user;
    double cpu_nice;
    double cpu_sys;
    double cpu_idle;
    double cpu_iowait;
    double cpu_steal;
    double cpu_hardirq;
    double cpu_softirq;
    double cpu_guest;
    double cpu_guest_nice;
};
extern int cpu_num;
extern struct double_stats_cpu st_allcpu[12];
cJSON *jsonrpc_cpus()
{
    int i;
    cJSON *array = NULL;
    cJSON *array_sub = NULL;
    array = cJSON_CreateArray();

    char args[3][32]={"mpsate", "-P", "ALL"};
    char *argv[3]={args[0], args[1], args[2]};
    mpstat_main(3, argv);

    for(i = 0; i <= cpu_num && i < 12; i++)
    {
        array_sub = cJSON_CreateArray();

        if(i == 0)
            cJSON_AddItemToArray(array_sub, cJSON_CreateString("all"));
        else
            cJSON_AddItemToArray(array_sub, cJSON_CreateNumber(i - 1));

        cJSON_AddItemToArray(array_sub, cJSON_CreateNumber(((int)(st_allcpu[i].cpu_user*100+0.5))/100.0));
        cJSON_AddItemToArray(array_sub, cJSON_CreateNumber(((int)(st_allcpu[i].cpu_nice*100+0.5))/100.0));
        cJSON_AddItemToArray(array_sub, cJSON_CreateNumber(((int)(st_allcpu[i].cpu_sys*100+0.5))/100.0));
        cJSON_AddItemToArray(array_sub, cJSON_CreateNumber(((int)(st_allcpu[i].cpu_idle*100+0.5))/100.0));
        cJSON_AddItemToArray(array_sub, cJSON_CreateNumber(((int)(st_allcpu[i].cpu_iowait*100+0.5))/100.0));
        cJSON_AddItemToArray(array_sub, cJSON_CreateNumber(((int)(st_allcpu[i].cpu_steal*100+0.5))/100.0));
        cJSON_AddItemToArray(array_sub, cJSON_CreateNumber(((int)(st_allcpu[i].cpu_hardirq*100+0.5))/100.0));
        cJSON_AddItemToArray(array_sub, cJSON_CreateNumber(((int)(st_allcpu[i].cpu_softirq*100+0.5))/100.0));
        cJSON_AddItemToArray(array_sub, cJSON_CreateNumber(((int)(st_allcpu[i].cpu_guest*100+0.5))/100.0));
        cJSON_AddItemToArray(array_sub, cJSON_CreateNumber(((int)(st_allcpu[i].cpu_guest_nice*100+0.5))/100.0));

        cJSON_AddItemToArray(array, array_sub);
    }
    return array;
}

cJSON *jsonrpc_10t_cpu()
{
    cJSON *array = NULL;
    cJSON *array_sub = NULL;

    array = cJSON_CreateArray();

    array_sub = cJSON_CreateArray();
    cJSON_AddItemToArray(array_sub, cJSON_CreateString("20192"));
    cJSON_AddItemToArray(array_sub, cJSON_CreateString("9.9"));
    cJSON_AddItemToArray(array_sub, cJSON_CreateString("root"));
    cJSON_AddItemToArray(array_sub, cJSON_CreateString("chrome"));
    cJSON_AddItemToArray(array, array_sub);
    array_sub  = cJSON_CreateArray();
    cJSON_AddItemToArray(array_sub, cJSON_CreateString("20192"));
    cJSON_AddItemToArray(array_sub, cJSON_CreateString("9.9"));
    cJSON_AddItemToArray(array_sub, cJSON_CreateString("root"));
    cJSON_AddItemToArray(array_sub, cJSON_CreateString("chrome"));
    cJSON_AddItemToArray(array, array_sub);
    array_sub  = cJSON_CreateArray();
    cJSON_AddItemToArray(array_sub, cJSON_CreateString("20192"));
    cJSON_AddItemToArray(array_sub, cJSON_CreateString("9.9"));
    cJSON_AddItemToArray(array_sub, cJSON_CreateString("root"));
    cJSON_AddItemToArray(array_sub, cJSON_CreateString("chrome"));
    cJSON_AddItemToArray(array, array_sub);
    array_sub  = cJSON_CreateArray();
    cJSON_AddItemToArray(array_sub, cJSON_CreateString("20192"));
    cJSON_AddItemToArray(array_sub, cJSON_CreateString("9.9"));
    cJSON_AddItemToArray(array_sub, cJSON_CreateString("root"));
    cJSON_AddItemToArray(array_sub, cJSON_CreateString("chrome"));
    cJSON_AddItemToArray(array, array_sub);
    array_sub  = cJSON_CreateArray();
    cJSON_AddItemToArray(array_sub, cJSON_CreateString("20192"));
    cJSON_AddItemToArray(array_sub, cJSON_CreateString("9.9"));
    cJSON_AddItemToArray(array_sub, cJSON_CreateString("root"));
    cJSON_AddItemToArray(array_sub, cJSON_CreateString("chrome"));
    cJSON_AddItemToArray(array, array_sub);
    array_sub  = cJSON_CreateArray();
    cJSON_AddItemToArray(array_sub, cJSON_CreateString("20192"));
    cJSON_AddItemToArray(array_sub, cJSON_CreateString("9.9"));
    cJSON_AddItemToArray(array_sub, cJSON_CreateString("root"));
    cJSON_AddItemToArray(array_sub, cJSON_CreateString("chrome"));
    cJSON_AddItemToArray(array, array_sub);
    array_sub  = cJSON_CreateArray();
    cJSON_AddItemToArray(array_sub, cJSON_CreateString("20192"));
    cJSON_AddItemToArray(array_sub, cJSON_CreateString("9.9"));
    cJSON_AddItemToArray(array_sub, cJSON_CreateString("root"));
    cJSON_AddItemToArray(array_sub, cJSON_CreateString("chrome"));
    cJSON_AddItemToArray(array, array_sub);
    array_sub  = cJSON_CreateArray();
    cJSON_AddItemToArray(array_sub, cJSON_CreateString("20192"));
    cJSON_AddItemToArray(array_sub, cJSON_CreateString("9.9"));
    cJSON_AddItemToArray(array_sub, cJSON_CreateString("root"));
    cJSON_AddItemToArray(array_sub, cJSON_CreateString("chrome"));
    cJSON_AddItemToArray(array, array_sub);
    array_sub  = cJSON_CreateArray();
    cJSON_AddItemToArray(array_sub, cJSON_CreateString("20192"));
    cJSON_AddItemToArray(array_sub, cJSON_CreateString("9.9"));
    cJSON_AddItemToArray(array_sub, cJSON_CreateString("root"));
    cJSON_AddItemToArray(array_sub, cJSON_CreateString("chrome"));
    cJSON_AddItemToArray(array, array_sub);
    array_sub  = cJSON_CreateArray();
    cJSON_AddItemToArray(array_sub, cJSON_CreateString("20192"));
    cJSON_AddItemToArray(array_sub, cJSON_CreateString("9.9"));
    cJSON_AddItemToArray(array_sub, cJSON_CreateString("root"));
    cJSON_AddItemToArray(array_sub, cJSON_CreateString("chrome"));
    cJSON_AddItemToArray(array, array_sub);

    return array;
}

cJSON *jsonrpc_10t_mem()
{
    cJSON *array = NULL;
    cJSON *array_sub = NULL;

    array = cJSON_CreateArray();

    array_sub = cJSON_CreateArray();
    cJSON_AddItemToArray(array_sub, cJSON_CreateString("20192"));
    cJSON_AddItemToArray(array_sub, cJSON_CreateString("9.9"));
    cJSON_AddItemToArray(array_sub, cJSON_CreateString("root"));
    cJSON_AddItemToArray(array_sub, cJSON_CreateString("chrome"));
    cJSON_AddItemToArray(array, array_sub);
    array_sub  = cJSON_CreateArray();
    cJSON_AddItemToArray(array_sub, cJSON_CreateString("20192"));
    cJSON_AddItemToArray(array_sub, cJSON_CreateString("9.9"));
    cJSON_AddItemToArray(array_sub, cJSON_CreateString("root"));
    cJSON_AddItemToArray(array_sub, cJSON_CreateString("chrome"));
    cJSON_AddItemToArray(array, array_sub);
    array_sub  = cJSON_CreateArray();
    cJSON_AddItemToArray(array_sub, cJSON_CreateString("20192"));
    cJSON_AddItemToArray(array_sub, cJSON_CreateString("9.9"));
    cJSON_AddItemToArray(array_sub, cJSON_CreateString("root"));
    cJSON_AddItemToArray(array_sub, cJSON_CreateString("chrome"));
    cJSON_AddItemToArray(array, array_sub);
    array_sub  = cJSON_CreateArray();
    cJSON_AddItemToArray(array_sub, cJSON_CreateString("20192"));
    cJSON_AddItemToArray(array_sub, cJSON_CreateString("9.9"));
    cJSON_AddItemToArray(array_sub, cJSON_CreateString("root"));
    cJSON_AddItemToArray(array_sub, cJSON_CreateString("chrome"));
    cJSON_AddItemToArray(array, array_sub);
    array_sub  = cJSON_CreateArray();
    cJSON_AddItemToArray(array_sub, cJSON_CreateString("20192"));
    cJSON_AddItemToArray(array_sub, cJSON_CreateString("9.9"));
    cJSON_AddItemToArray(array_sub, cJSON_CreateString("root"));
    cJSON_AddItemToArray(array_sub, cJSON_CreateString("chrome"));
    cJSON_AddItemToArray(array, array_sub);
    array_sub  = cJSON_CreateArray();
    cJSON_AddItemToArray(array_sub, cJSON_CreateString("20192"));
    cJSON_AddItemToArray(array_sub, cJSON_CreateString("9.9"));
    cJSON_AddItemToArray(array_sub, cJSON_CreateString("root"));
    cJSON_AddItemToArray(array_sub, cJSON_CreateString("chrome"));
    cJSON_AddItemToArray(array, array_sub);
    array_sub  = cJSON_CreateArray();
    cJSON_AddItemToArray(array_sub, cJSON_CreateString("20192"));
    cJSON_AddItemToArray(array_sub, cJSON_CreateString("9.9"));
    cJSON_AddItemToArray(array_sub, cJSON_CreateString("root"));
    cJSON_AddItemToArray(array_sub, cJSON_CreateString("chrome"));
    cJSON_AddItemToArray(array, array_sub);
    array_sub  = cJSON_CreateArray();
    cJSON_AddItemToArray(array_sub, cJSON_CreateString("20192"));
    cJSON_AddItemToArray(array_sub, cJSON_CreateString("9.9"));
    cJSON_AddItemToArray(array_sub, cJSON_CreateString("root"));
    cJSON_AddItemToArray(array_sub, cJSON_CreateString("chrome"));
    cJSON_AddItemToArray(array, array_sub);
    array_sub  = cJSON_CreateArray();
    cJSON_AddItemToArray(array_sub, cJSON_CreateString("20192"));
    cJSON_AddItemToArray(array_sub, cJSON_CreateString("9.9"));
    cJSON_AddItemToArray(array_sub, cJSON_CreateString("root"));
    cJSON_AddItemToArray(array_sub, cJSON_CreateString("chrome"));
    cJSON_AddItemToArray(array, array_sub);
    array_sub  = cJSON_CreateArray();
    cJSON_AddItemToArray(array_sub, cJSON_CreateString("20192"));
    cJSON_AddItemToArray(array_sub, cJSON_CreateString("9.9"));
    cJSON_AddItemToArray(array_sub, cJSON_CreateString("root"));
    cJSON_AddItemToArray(array_sub, cJSON_CreateString("chrome"));
    cJSON_AddItemToArray(array, array_sub);

    return array;
}

char *jsonrpc_status()
{
    char *out;
    cJSON *root = NULL;
    root = cJSON_CreateObject();

    int num = 1;
    char name[128];
    get_cpuinfo(&num, name); //对无类型get函数含有一个形参结构体类弄的指针O

    cJSON_AddItemToObject(root, "Mem", jsonrpc_mem());
    cJSON_AddItemToObject(root, "Swap", jsonrpc_swap());
    cJSON_AddItemToObject(root, "Disk", jsonrpc_disk());
    cJSON_AddItemToObject(root, "Time", jsonrpc_time());
    cJSON_AddItemToObject(root, "Users", jsonrpc_user());
    cJSON_AddItemToObject(root, "Uptime", jsonrpc_uptime());

    cJSON_AddNumberToObject(root, "CPU_Cores", num);
    cJSON_AddStringToObject(root, "CPU_Name", name);

    cJSON_AddItemToObject(root, "Network", jsonrpc_network());
    cJSON_AddItemToObject(root, "CPUs", jsonrpc_cpus());
    cJSON_AddItemToObject(root, "Process_10T_CPU", jsonrpc_10t_cpu());
    cJSON_AddItemToObject(root, "Process_10T_Mem", jsonrpc_10t_mem());

    out = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    return out;
}

int jsonrcp_dispach(struct rpc_request_t *request, char **out)
{
    switch(request->cmd)
    {
        case GET_STATUS:
            printf("GET_STATUS\n");
            *out = jsonrpc_status();
            break;
        default:
            break;
    }
    return strlen(*out) + 1;
}

void *jsonrpc_task(void *arg)
{
    int n;
    char mesg[MAXBUF];
    char *out;
    struct sockaddr client_fd;
    socklen_t len = sizeof(client_fd);

    while(1)
    {
        /* waiting for receive data */
        n = recvfrom(sockfd, mesg, sizeof(mesg), 0, &client_fd, &len);
        if(n <= 0)
            continue;

        n = jsonrcp_dispach((struct rpc_request_t *)mesg, &out);

        if(n <= 0)
            continue;

        /* sent data back to client */
        n = sendto(sockfd, out, n, 0, &client_fd, len);
        if(n <= 0)
            printf("[%d]%s: sendto error\n", __LINE__, __func__);

        printf("PUT %s\n", out);
        free(out);
    }

    return NULL;
}

int jsonrpc_thread()
{
    int ret=0;
    pthread_t tid1;
    //pthread_mutex_init(&rpc_lock, NULL);  
    sockfd = jsonrpc_socket();

    ret = pthread_create(&tid1, NULL, jsonrpc_task, NULL);
    if(ret)
    {
        printf("create file rpc error!\n");
        return -1; 
    }

    printf("create file rpc successfully!\n");
    return ret;
}
