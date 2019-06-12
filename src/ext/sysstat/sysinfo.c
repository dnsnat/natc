/*
 * =====================================================================================
 *
 *       Filename:  sysinfo.c
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  10/04/2018 08:00:42 PM
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
#include <mntent.h>
#include <sys/vfs.h>
#include <unistd.h>

#include "sysinfo.h"

int get_memoccupy (MEM_OCCUPY *mem) //对无类型get函数含有一个形参结构体类弄的指针O
{
    FILE *fd;          
    int i = 0; 
    char *s;
    char buff[256];   
    fd = fopen("/proc/meminfo", "r"); 
    if(!fd)
        return -1;

    for(i = 0; i < 4; i++)
    {
        s = fgets(buff, sizeof(buff), fd); 
        if(s)
            sscanf(buff, "%s %lu %s", (mem+i)->name, &(mem+i)->total, (mem+i)->name2); 
    }
    fclose(fd);     //关闭文件fd
    return 0;
}

int get_swapoccupy (MEM_OCCUPY *mem) //对无类型get函数含有一个形参结构体类弄的指针O
{
    FILE *fd;          
    int j = 0; 
    char buff[256];   
    fd = fopen("/proc/meminfo", "r"); 
    if(!fd)
        return -1;

    while(fgets(buff, sizeof(buff), fd))
    {
        sscanf(buff, "%s %lu %s", (mem+j)->name, &(mem+j)->total, (mem+j)->name2); 
        if(strncmp((mem+j)->name, "SwapTotal", strlen("SwapTotal")) == 0 ||
           strncmp((mem+j)->name, "SwapFree", strlen("SwapFree")) == 0) 
            j++;

        if(j==2)
            break;
    }
    fclose(fd);     //关闭文件fd
    return 0;
}

int get_cpuinfo(int *num, char *name) //对无类型get函数含有一个形参结构体类弄的指针O
{
    FILE *fd;          
    char buff[256];   
    char content[128];   
    fd = fopen("/proc/cpuinfo", "r"); 
    if(!fd)
        return -1;

    while(fgets(buff, sizeof(buff), fd))
    {
        if(strstr(buff,"cpu cores")!=NULL)
        {
            buff[strlen(buff)-1]=0;
            sscanf(buff,"%s%s%s%s",content,content,content,content);
            *num = atoi(content);
            break;
        }
        if(strstr(buff,"model name")!=NULL)
        {
            strncpy(name, buff + strlen("model name	:"), 128);
        }
    }
    fclose(fd);     //关闭文件fd
    return 0;
}

struct st_net st_nets[12];
int get_netinfo(int *num) //对无类型get函数含有一个形参结构体类弄的指针O
{
    FILE *fd;          
    char buff[256];   
    char *p;
    int i = 0;

    fd = fopen("/proc/net/dev", "r"); 
    if(!fd)
        return -1;

    fseek(fd, 0, SEEK_SET);
    while(fgets(buff, sizeof(buff), fd))
    {
        i = 0;
        /*去除空格，制表符，换行符等不需要的字段*/
        for (p = strtok(buff, " \t\r\n"); p; p = strtok(NULL, " \t\r\n"))
        {
            i++;
            if(i == 1)
            {
                if(strcmp(p, "Inter-|") == 0 || strcmp(p, "face") == 0)
                    break;
                strncpy(st_nets[*num].name, p, sizeof(st_nets[*num].name));
            }

            /*得到的字符串中的第二个字段是接收流量*/
            if(i == 2)
                strncpy(st_nets[*num].rx_packet, p, sizeof(st_nets[*num].rx_packet));

            if(i == 3)
                strncpy(st_nets[*num].rx_byte, p, sizeof(st_nets[*num].rx_byte));

            /*得到的字符串中的第十个字段是发送流量*/
            if(i == 10)
                strncpy(st_nets[*num].tx_packet, p, sizeof(st_nets[*num].tx_packet));

            if(i == 11)
            {
                strncpy(st_nets[*num].tx_byte, p, sizeof(st_nets[*num].tx_byte));
                (*num)++;
                break;
            }
        }
    }
    fclose(fd);     //关闭文件fd
    return 0;
}

struct st_disk st_disks[12];
int get_diskinfo(int *num) //对无类型get函数含有一个形参结构体类弄的指针O
{
    FILE* mount_table;
    struct mntent *mount_entry;
    struct statfs s;
    unsigned long blocks_used;
    unsigned blocks_percent_used;
    const char *disp_units_hdr = NULL;
    mount_table = NULL;
    mount_table = setmntent("/etc/mtab", "r");
    if (!mount_table)
    {
        fprintf(stderr, "set mount entry error\n");
        return -1;
    }
    disp_units_hdr = "     Size";
    printf("Filesystem           %-15sUsed Available %s Mounted on\n",
            disp_units_hdr, "Use%");
    while (1) {
        const char *device;
        const char *mount_point;
        if (mount_table) {
            mount_entry = getmntent(mount_table);
            if (!mount_entry) {
                endmntent(mount_table);
                break;
            }
        }
        else
            continue;
        device = mount_entry->mnt_fsname;
        mount_point = mount_entry->mnt_dir;
        //fprintf(stderr, "mount info: device=%s mountpoint=%s\n", device, mount_point);
        if (statfs(mount_point, &s) != 0)
        {
            fprintf(stderr, "statfs failed!\n");   
            continue;
        }
        if ((s.f_blocks > 0) || !mount_table )
        {
            blocks_used = s.f_blocks - s.f_bfree;
            blocks_percent_used = 0;
            if (blocks_used + s.f_bavail)
            {
                blocks_percent_used = (blocks_used * 100ULL
                        + (blocks_used + s.f_bavail)/2
                        ) / (blocks_used + s.f_bavail);
            }
            /* GNU coreutils 6.10 skips certain mounts, try to be compatible.  */
            if (strcmp(device, "rootfs") == 0)
                continue;

            if (strncmp(device, "/dev", strlen("/dev")) == 0 && (*num) < 12)
            {
                strncpy(st_disks[*num].name, device, sizeof(st_disks[*num].name));
                sprintf(st_disks[*num].size, "%d", (unsigned int)(s.f_blocks * s.f_bsize / 1024));
                sprintf(st_disks[*num].used, "%d", (unsigned int)((s.f_blocks - s.f_bfree) * s.f_bsize / 1024));
                sprintf(st_disks[*num].free, "%d", (unsigned int)(s.f_bavail *s.f_bsize / 1024));
                sprintf(st_disks[*num].UseAvg, "%3u%%", blocks_percent_used);
                (*num)++;
            }
        }
    }
    return 0;
}

#if 0
int cal_cpuoccupy (CPU_OCCUPY *o, CPU_OCCUPY *n) 
{   
    unsigned long od, nd;    
    unsigned long id, sd;
    int cpu_use = 0;   

    od = (unsigned long) (o->user + o->nice + o->system +o->idle);//第一次(用户+优先级+系统+空闲)的时间再赋给od
    nd = (unsigned long) (n->user + n->nice + n->system +n->idle);//第二次(用户+优先级+系统+空闲)的时间再赋给od

    id = (unsigned long) (n->user - o->user);    //用户第一次和第二次的时间之差再赋给id
    sd = (unsigned long) (n->system - o->system);//系统第一次和第二次的时间之差再赋给sd
    if((nd-od) != 0)
        cpu_use = (int)((sd+id)*10000)/(nd-od); //((用户+系统)乖100)除(第一次和第二次的时间差)再赋给g_cpu_used
    else cpu_use = 0;
    //printf("cpu: %u/n",cpu_use);
    return cpu_use;
}

get_cpuoccupy (CPU_OCCUPY *cpust) //对无类型get函数含有一个形参结构体类弄的指针O
{   
    FILE *fd;         
    int n;            
    char buff[256]; 
    CPU_OCCUPY *cpu_occupy;
    cpu_occupy=cpust;

    fd = fopen ("/proc/stat", "r"); 
    fgets (buff, sizeof(buff), fd);

    sscanf (buff, "%s %u %u %u %u", cpu_occupy->name, &cpu_occupy->user, &cpu_occupy->nice,&cpu_occupy->system, &cpu_occupy->idle);

    fclose(fd);     
}

#endif
