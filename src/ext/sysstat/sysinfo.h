/*
 * =====================================================================================
 *
 *       Filename:  sysinfo.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  10/05/2018 10:23:44 AM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  YOUR NAME (), 
 *   Organization:  
 *
 * =====================================================================================
 */

#ifndef _SYSINFO_H
#define _SYSINFO_H
typedef struct         //定义一个cpu occupy的结构体
{
    char name[20];      //定义一个char类型的数组名name有20个元素
    unsigned int user; //定义一个无符号的int类型的user
    unsigned int nice; //定义一个无符号的int类型的nice
    unsigned int system;//定义一个无符号的int类型的system
    unsigned int idle; //定义一个无符号的int类型的idle
}CPU_OCCUPY;

typedef struct         //定义一个mem occupy的结构体
{
    char name[20];      //定义一个char类型的数组名name有20个元素
    unsigned long total; 
    char name2[20];
    unsigned long free;                       
}MEM_OCCUPY;

struct st_disk
{
    char name[128];
    char size[32];
    char used[32];
    char free[32];
    char UseAvg[32];
};

struct st_net
{
    char name[128];
    char rx_packet[32];
    char tx_packet[32];
    char rx_byte[32];
    char tx_byte[32];
};

int get_memoccupy (MEM_OCCUPY *mem); //对无类型get函数含有一个形参结构体类弄的指针O
int get_swapoccupy (MEM_OCCUPY *mem); //对无类型get函数含有一个形参结构体类弄的指针O
int get_cpunum(int *num, char *name); //对无类型get函数含有一个形参结构体类弄的指针O
int get_cpuinfo(int *num, char *name); //对无类型get函数含有一个形参结构体类弄的指针O
int get_netinfo(int *num); //对无类型get函数含有一个形参结构体类弄的指针O
int get_diskinfo(int *num); //对无类型get函数含有一个形参结构体类弄的指针O
#endif

