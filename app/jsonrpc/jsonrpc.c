/**
  Onion HTTP server library
  Copyright (C) 2010-2018 David Moreno Montero and others

  This library is free software; you can redistribute it and/or
  modify it under the terms of, at your choice:

  a. the Apache License Version 2.0.

  b. the GNU General Public License as published by the
  Free Software Foundation; either version 2.0 of the License,
  or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of both licenses, if not see
  <http://www.gnu.org/licenses/> and
  <http://www.apache.org/licenses/LICENSE-2.0>.
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

#include <onion/onion.h>
#include <onion/dict.h>
#include <onion/block.h>
#include <onion/request.h>
#include <onion/response.h>
#include <onion/url.h>
#include <onion/shortcuts.h>
#include <onion/log.h>
//#include <onion/handlers/exportlocal.h>
//#include <onion/handlers/auth_pam.h>
#include <onion/types.h>
#include <onion/types_internal.h>

#include <jansson.h>

#include "jsonrpc.h"
#include "sysinfo.h"
#include "compat_getpwuid.h"

int const MAX_STR_LEN = 1024;
pthread_mutex_t rpc_lock;  
void md5(const uint8_t *initial_msg, size_t initial_len, uint8_t *digest);

char *getMode(mode_t mode, char *buf) 
{
	if (buf == NULL) {
		fprintf(stderr, "buf can't be NULL\n");
		return NULL;
	}
 
	memset(buf, '-', 9);
 
	if (mode & S_IRUSR)
		buf[0] = 'r';
 
	if (mode & S_IWUSR)
		buf[1] = 'w';
	if (mode & S_IXUSR)
		buf[2] = 'x';
	if (mode & S_IRGRP)
		buf[3] = 'r';
	if (mode & S_IWGRP)
		buf[4] = 'w';
	if (mode & S_IXGRP)
		buf[5] = 'x';
	if (mode & S_IROTH)
		buf[6] = 'r';
	if (mode & S_IWOTH)
		buf[7] = 'w';
	if (mode & S_IXOTH)
		buf[8] = 'x';
 
	buf[9] = 0;
	return buf;
}

int dispach_list(onion_response * res, json_t *object)
{
    void *iter;
    json_error_t error;

    iter = json_object_iter_at(object, "path");
    if(!iter)
    {
        fprintf(stderr, "error: on line %d: %s\n", error.line, error.text);
        return -1;
    }

    //const char *iter_keys = json_object_iter_key(iter);
    json_t *iter_values   = json_object_iter_value(iter);
    const char *path      = json_string_value(iter_values);
    printf("path=%s\n", path);

   	// check the parameter !
	if( NULL == path )
	{
		printf(" path is null !\r\n");
		return -1;
	}
 
    char dir_name[1024];
    sprintf(dir_name, ".%s", path);
    //json_decref(iter_values);

	// check if dir_name is a valid dir
	struct stat s;
	lstat(dir_name , &s);
	if(!S_ISDIR(s.st_mode))
	{
		printf("dir_name is not a valid directory !\r\n");
		return -1;
	}
	
	struct dirent *filename;    // return value for readdir()
 	DIR *dir;                   // return value for opendir()
	dir = opendir( dir_name );
	if(NULL == dir)
	{
		printf("Can not open dir %s\r\n", dir_name);
		return -1;
	}
	printf("Successfully opened the dir !\r\n");
	
    json_t *object_res = json_object();
    if(!object_res)
    {
        return 0;
    }
    json_t *array      = json_array();
    if(!array)
    {
        json_decref(object_res);
        return 0;
    }

    /* read all the files in the dir ~ */
    while( ( filename = readdir(dir) ) != NULL )
    {
        // get rid of "." and ".."
        if( strcmp( filename->d_name , "." ) == 0 || 
                strcmp( filename->d_name , "..") == 0    )
            continue;

        json_t *object_res_sub = json_object();
        if(!object_res)
        {
            json_decref(array);
            json_decref(object_res);
            return 0;
        }

        json_object_set_new(object_res_sub, "name", json_string(filename->d_name));
        if(DT_DIR == filename->d_type) 
            json_object_set_new(object_res_sub, "type", json_string("dir"));
        else
            json_object_set_new(object_res_sub, "type", json_string("file"));

#if 1
        struct stat file_stat;
        char filename_full[256];
        sprintf(filename_full, "%s%s", dir_name, filename->d_name);
        stat(filename->d_name, &file_stat);
        char mode_buf[12];
        getMode(file_stat.st_mode, mode_buf);
        json_object_set_new(object_res_sub, "rights", json_string(mode_buf));
        json_object_set_new(object_res_sub, "size", json_integer(file_stat.st_size));

        char date_time[1024];
        struct tm now_time;
        localtime_r(&file_stat.st_atime, &now_time);

        //strftime(date_time, sizeof(date_time), "%Y-%m-%d %H:%M:%S", localtime_r(&file_stat.st_mtime, &t));
        sprintf(date_time, "%d-%d-%d %d:%d:%d\n", now_time.tm_year + 1900, now_time.tm_mon + 1,
                now_time.tm_mday, now_time.tm_hour, now_time.tm_min, now_time.tm_sec);
        json_object_set_new(object_res_sub, "date", json_string(date_time));
        //printf("clock_gettime : date_time=%s, tv_nsec=%ld\n", date_time, file_stat.st_mtim.tv_nsec);

#endif
        json_array_append(array, object_res_sub);
        json_decref(object_res_sub);
        //cout<<filename ->d_name <<endl;
    }

    json_object_set(object_res, "result", array);

    char *resstr = json_dumps(object_res, JSON_PRESERVE_ORDER);
    if(!resstr)
    {
        json_decref(array);
        json_decref(object_res);
        return 0;
    }

    printf("result=%s\n",resstr);
    //printf("result_size = %ld\n", strlen(resstr));

    onion_response_write(res, resstr, (strlen(resstr) > (1024 * 10)) ? (1024 * 10): strlen(resstr));

    //onion_response_flush(res);
    free(resstr);

    json_decref(array);
    json_decref(object_res);
    return 0;
}

const char *find_file_name(const char *name)
{
	char *name_start = NULL;
	int sep = '/';
	if (NULL == name) {
			printf("the path name is NULL\n");
	    return NULL;
	}
	name_start = strrchr(name, sep);

	return (NULL == name_start)?name:(name_start + 1);
}

/*file manage rpc*/
onion_connection_status strip_rpc(void *_, onion_request * req, onion_response * res) 
{ 
    void *iter;
    json_t *object;
    json_error_t error;

#if 0
    if(onion_request_get_header(req, "Origin"))
        onion_response_set_header(res, "Access-Control-Allow-Origin", onion_request_get_header(req, "Origin"));

    onion_response_set_header(res, "Access-Control-Allow-Credentials", "true");//设置是否发送cookie，需要在客户端同时设置才会生效
    onion_response_set_header(res, "Access-Control-Allow-Methods", "POST");
    onion_response_set_header(res, "Access-Control-Allow-Headers", "X-Requested-With,Content-Type");
    onion_response_set_header(res, "Content-Security-Policy", "upgrade-insecure-requests");
#endif

    if(!onion_request_get_cookie(req, "sessionid"))
        return OCS_PROCESSED;

    /*上传*/
    if (onion_request_get_flags(req) & OR_POST) 
    {
        int i;
        for(i = 0; i < 128; i++)
        {
            char fileid[32];
            sprintf(fileid, "file-%d", i);
            const char *name = onion_request_get_post(req, fileid);
            const char *filename = onion_request_get_file(req, fileid);
            if (!name || !filename)
                break;

            char finalname[1024];
            snprintf(finalname, sizeof(finalname), "%s/%s", ".", name);
            ONION_DEBUG("Copying from %s to %s", filename, finalname);
            onion_shortcut_rename(filename, finalname);
        }
        if(i)
        {
            char resstr[] = {"{ \"result\": { \"success\": true, \"error\": null } }"};
            onion_response_write(res, resstr, strlen(resstr));
            return OCS_PROCESSED;
        }
    }

    printf("download:[%s]\n", onion_request_get_fullpath(req));
    /*下载*/
    if(onion_request_get_query(req, "action") && onion_request_get_query(req, "path")) 
        if(strncmp("download", onion_request_get_query(req, "action"), strlen("download")) == 0)
        {
            char attachment_filename[1024];
            sprintf(attachment_filename, "attachment; filename=%s", find_file_name(onion_request_get_query(req, "path")));
            onion_response_set_header(res, "Content-Disposition", attachment_filename);
            return onion_shortcut_response_file(onion_request_get_query(req, "path") + 1, req, res);
        }

    /*post:action*/
    const onion_block *dreq = onion_request_get_data(req);
    if (!dreq)
    {
        return OCS_PROCESSED;
    }

    const char *data = onion_block_data(dreq);
    if (!data)
        return OCS_PROCESSED;

    object = json_loads(data, 0, &error);
    if(!object)
    {
        fprintf(stderr, "error: on line %d: %s\n", error.line, error.text);
        return OCS_PROCESSED;
    }

    iter = json_object_iter_at(object, "action");
    if(!iter)
    {
        fprintf(stderr, "error: on line %d: %s\n", error.line, error.text);
        return OCS_PROCESSED;
    }

    //const char *iter_keys = json_object_iter_key(iter);
    json_t *iter_values   = json_object_iter_value(iter);
    if(!iter_values)
        return OCS_PROCESSED;

    const char *action    = json_string_value(iter_values);
    if(strncmp(action, "list", strlen("list")) == 0)
        dispach_list(res, object);

    json_decref(object);
    return OCS_PROCESSED;
}

int read_file_to_string(char *pread, char *charFilePath)
{
    FILE *pfile = fopen(charFilePath, "rb");//打开文件，返回文件操作符
    size_t result = 0;

    if(pfile)//打开文件一定要判断是否成功
    {
        fseek(pfile,0,SEEK_END);//将文件内部的指针指向文件末尾
        long lsize=ftell(pfile);//获取文件长度，（得到文件位置指针当前位置相对于文件首的偏移字节数）
        rewind(pfile);//将文件内部的指针重新指向一个流的开头
        result=fread(pread,1,lsize,pfile);//将pfile中内容读入pread指向内存中
    }
    fclose(pfile);
    return result;
}

int status_json_add_mem(json_t *object_res)
{
    json_t *array      = json_array();
    if(!array)
    {
        return -1;
    }

    MEM_OCCUPY mem[4];
    get_memoccupy(mem); //对无类型get函数含有一个形参结构体类弄的指针O
    json_array_append_new(array, json_integer(mem[0].total/1024));
    json_array_append_new(array, json_integer(mem[1].total/1024));
    json_array_append_new(array, json_integer(mem[2].total/1024));
    json_array_append_new(array, json_integer(mem[3].total/1024));
    json_object_set_new(object_res, "Mem", array);

    return 0;
}

int status_json_add_swap(json_t *object_res)
{
    json_t *array      = json_array();
    if(!array)
    {
        return -1;
    }

    MEM_OCCUPY mem[4] = {0};
    get_swapoccupy(mem); //对无类型get函数含有一个形参结构体类弄的指针O
    json_array_append_new(array, json_integer(mem[0].total/1024));   //SwapSize 
    json_array_append_new(array, json_integer(mem[1].total/1024));   //SwapFree 
    json_array_append_new(array, json_integer((mem[0].total - mem[1].total)/1024));    //SwapUsed 
    json_array_append_new(array, json_integer(mem[1].total/1024));   //SwapAvail
    json_object_set_new(object_res, "Swap", array);

    return 0;
}

extern struct st_disk st_disks[12];
int status_json_add_disk(json_t *object_res)
{
    int disk_num = 0;
    int i;
    json_t *array      = json_array();
    if(!array)
    {
        return -1;
    }

    get_diskinfo(&disk_num); //对无类型get函数含有一个形参结构体类弄的指针O

    for(i = 0; i < disk_num && i < 12; i++)
    {
        json_t *array_sub  = json_array();
        if(!array_sub)
        {
            break;
            printf("[%d]%s json_array error\r\n", __LINE__, __func__);
        }

        json_array_append_new(array_sub, json_string(st_disks[i].name));
        json_array_append_new(array_sub, json_string(st_disks[i].size));
        json_array_append_new(array_sub, json_string(st_disks[i].used));
        json_array_append_new(array_sub, json_string(st_disks[i].free));
        json_array_append_new(array_sub, json_string(st_disks[i].UseAvg));

        json_array_append(array, array_sub);
        json_decref(array_sub);
    }

    json_object_set_new(object_res, "Disk", array);

    return 0;
}

int status_json_add_time(json_t *object_res)
{
    time_t now; //实例化time_t结构    
    struct tm *timenow; //实例化tm结构指针    
    time(&now);   
    timenow = localtime(&now);   

    json_object_set_new(object_res, "Time", json_string(asctime(timenow)));
    return 0;
}

int status_json_add_user(json_t *object_res)
{
    json_t *array      = json_array();
    if(!array)
    {
        return -1;
    }

    struct passwd *user;
    user= compat_getpwuid(getuid());
    if (user)
        json_array_append_new(array, json_string(user->pw_name));
    else
        json_array_append_new(array, json_string("null"));

    json_object_set_new(object_res, "Users", array);

    return 0;
}

int status_json_add_uptime(json_t *object_res)
{
    json_t *array      = json_array();
    if(!array)
    {
        return -1;
    }

    struct sysinfo info;
    //time_t boot_time = 0;
    char boot_time_s[128];
    if (sysinfo(&info)) {
        json_array_append_new(array, json_string("00:00:00"));
        json_array_append_new(array, json_string("null"));
        json_array_append_new(array, json_string("0.00"));
        json_array_append_new(array, json_string("0.00"));
        json_array_append_new(array, json_string("0.00"));
        json_object_set_new(object_res, "Uptime", array);
        return -1;
    }

    //struct tm *ptm = NULL;
    localtime((const time_t *)&info.uptime);
    sprintf(boot_time_s, "%ld天%ld小时%ld分%ld秒", info.uptime/86400,
            info.uptime%86400/3600, info.uptime%86400%3600/60, info.uptime%86400%3600%60);

    json_array_append_new(array, json_string(boot_time_s));
    json_array_append_new(array, json_string("1user"));
    json_array_append_new(array, json_string("0.84"));
    json_array_append_new(array, json_string("0.66"));
    json_array_append_new(array, json_string("0.47"));
    json_object_set_new(object_res, "Uptime", array);

    return 0;
}

int status_json_add_cpu_cores(json_t *object_res)
{
    int num;
    char name[128];

    get_cpuinfo(&num, name); //对无类型get函数含有一个形参结构体类弄的指针O
    json_object_set_new(object_res, "CPU_Cores", json_integer(num));
    json_object_set_new(object_res, "CPU_Name", json_string(name));
    return 0;
}

int status_json_add_cpu_name(json_t *object_res)
{
    return 0;
}

extern struct st_net st_nets[12];
int status_json_add_network(json_t *object_res)
{
    int net_num = 0;
    int i;
    json_t *array      = json_array();
    if(!array)
    {
        return -1;
    }

    get_netinfo(&net_num); //对无类型get函数含有一个形参结构体类弄的指针O

    for(i = 0; i < net_num && i < 12; i++)
    {
        json_t *array_sub  = json_array();
        if(!array_sub)
        {
            break;
            printf("[%d]%s json_array error\r\n", __LINE__, __func__);
        }

        json_array_append_new(array_sub, json_string(st_nets[i].name));
        json_array_append_new(array_sub, json_string(st_nets[i].rx_packet));
        json_array_append_new(array_sub, json_string(st_nets[i].tx_packet));
        json_array_append_new(array_sub, json_string(st_nets[i].rx_byte));
        json_array_append_new(array_sub, json_string(st_nets[i].tx_byte));

        json_array_append(array, array_sub);
        json_decref(array_sub);
    }


    json_object_set_new(object_res, "Network", array);

    return 0;
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

int status_json_add_cpus(json_t *object_res)
{
    int i;
    json_t *array      = json_array();
    if(!array)
    {
        return -1;
    }

    char args[3][32]={"mpsate", "-P", "ALL"};
    char *argv[3]={args[0], args[1], args[2]};
    mpstat_main(3, argv);

    for(i = 0; i <= cpu_num && i < 12; i++)
    {
        json_t *array_sub  = json_array();
        //printf("array_sub = [%x] cpu_num = %d i = %d\n", array_sub, cpu_num, i);
        if(!array_sub)
        {
            //json_decref(array);
            //return -1;
            //break;
            printf("[%d]%s json_array error\r\n", __LINE__, __func__);
        }

        if(i == 0)
            json_array_append_new(array_sub, json_string("all"));
        else
            json_array_append_new(array_sub, json_integer(i - 1));
        json_array_append_new(array_sub, json_real(((int)(st_allcpu[i].cpu_user*100+0.5))/100.0));
        json_array_append_new(array_sub, json_real(((int)(st_allcpu[i].cpu_nice*100+0.5))/100.0));
        json_array_append_new(array_sub, json_real(((int)(st_allcpu[i].cpu_sys*100+0.5))/100.0));
        json_array_append_new(array_sub, json_real(((int)(st_allcpu[i].cpu_idle*100+0.5))/100.0));
        json_array_append_new(array_sub, json_real(((int)(st_allcpu[i].cpu_iowait*100+0.5))/100.0));
        json_array_append_new(array_sub, json_real(((int)(st_allcpu[i].cpu_steal*100+0.5))/100.0));
        json_array_append_new(array_sub, json_real(((int)(st_allcpu[i].cpu_hardirq*100+0.5))/100.0));
        json_array_append_new(array_sub, json_real(((int)(st_allcpu[i].cpu_softirq*100+0.5))/100.0));
        json_array_append_new(array_sub, json_real(((int)(st_allcpu[i].cpu_guest*100+0.5))/100.0));
        json_array_append_new(array_sub, json_real(((int)(st_allcpu[i].cpu_guest_nice*100+0.5))/100.0));

        json_array_append(array, array_sub);
        json_decref(array_sub);
    }
    json_object_set_new(object_res, "CPUs", array);
    return 0;
}

int status_json_add_process_10t_cpu(json_t *object_res)
{
    json_t *array      = json_array();
    if(!array)
    {
        return -1;
    }

    json_t *array_sub  = json_array();
    if(!array)
    {
        json_decref(array);
        return -1;
    }

    json_array_append_new(array_sub, json_string("20192"));
    json_array_append_new(array_sub, json_string("9.9"));
    json_array_append_new(array_sub, json_string("root"));
    json_array_append_new(array_sub, json_string("chrome"));
    json_array_append_new(array, array_sub);
    array_sub  = json_array();
    json_array_append_new(array_sub, json_string("20192"));
    json_array_append_new(array_sub, json_string("9.9"));
    json_array_append_new(array_sub, json_string("root"));
    json_array_append_new(array_sub, json_string("chrome"));
    json_array_append_new(array, array_sub);
    array_sub  = json_array();
    json_array_append_new(array_sub, json_string("20192"));
    json_array_append_new(array_sub, json_string("9.9"));
    json_array_append_new(array_sub, json_string("root"));
    json_array_append_new(array_sub, json_string("chrome"));
    json_array_append_new(array, array_sub);
    array_sub  = json_array();
    json_array_append_new(array_sub, json_string("20192"));
    json_array_append_new(array_sub, json_string("9.9"));
    json_array_append_new(array_sub, json_string("root"));
    json_array_append_new(array_sub, json_string("chrome"));
    json_array_append_new(array, array_sub);
    array_sub  = json_array();
    json_array_append_new(array_sub, json_string("20192"));
    json_array_append_new(array_sub, json_string("9.9"));
    json_array_append_new(array_sub, json_string("root"));
    json_array_append_new(array_sub, json_string("chrome"));
    json_array_append_new(array, array_sub);
    array_sub  = json_array();
    json_array_append_new(array_sub, json_string("20192"));
    json_array_append_new(array_sub, json_string("9.9"));
    json_array_append_new(array_sub, json_string("root"));
    json_array_append_new(array_sub, json_string("chrome"));
    json_array_append_new(array, array_sub);
    array_sub  = json_array();
    json_array_append_new(array_sub, json_string("20192"));
    json_array_append_new(array_sub, json_string("9.9"));
    json_array_append_new(array_sub, json_string("root"));
    json_array_append_new(array_sub, json_string("chrome"));
    json_array_append_new(array, array_sub);
    array_sub  = json_array();
    json_array_append_new(array_sub, json_string("20192"));
    json_array_append_new(array_sub, json_string("9.9"));
    json_array_append_new(array_sub, json_string("root"));
    json_array_append_new(array_sub, json_string("chrome"));
    json_array_append_new(array, array_sub);
    array_sub  = json_array();
    json_array_append_new(array_sub, json_string("20192"));
    json_array_append_new(array_sub, json_string("9.9"));
    json_array_append_new(array_sub, json_string("root"));
    json_array_append_new(array_sub, json_string("chrome"));
    json_array_append_new(array, array_sub);
    array_sub  = json_array();
    json_array_append_new(array_sub, json_string("20192"));
    json_array_append_new(array_sub, json_string("9.9"));
    json_array_append_new(array_sub, json_string("root"));
    json_array_append_new(array_sub, json_string("chrome"));
    json_array_append_new(array, array_sub);

    json_object_set_new(object_res, "Process_10T_CPU", array);

    return 0;
}

int status_json_add_process_10t_mem(json_t *object_res)
{
    json_t *array      = json_array();
    if(!array)
    {
        return -1;
    }

    json_t *array_sub  = json_array();
    if(!array)
    {
        json_decref(array);
        return -1;
    }

    json_array_append_new(array_sub, json_string("20192"));
    json_array_append_new(array_sub, json_string("9.9"));
    json_array_append_new(array_sub, json_string("root"));
    json_array_append_new(array_sub, json_string("chrome"));
    json_array_append_new(array, array_sub);
    array_sub  = json_array();
    json_array_append_new(array_sub, json_string("20192"));
    json_array_append_new(array_sub, json_string("9.9"));
    json_array_append_new(array_sub, json_string("root"));
    json_array_append_new(array_sub, json_string("chrome"));
    json_array_append_new(array, array_sub);
    array_sub  = json_array();
    json_array_append_new(array_sub, json_string("20192"));
    json_array_append_new(array_sub, json_string("9.9"));
    json_array_append_new(array_sub, json_string("root"));
    json_array_append_new(array_sub, json_string("chrome"));
    json_array_append_new(array, array_sub);
    array_sub  = json_array();
    json_array_append_new(array_sub, json_string("20192"));
    json_array_append_new(array_sub, json_string("9.9"));
    json_array_append_new(array_sub, json_string("root"));
    json_array_append_new(array_sub, json_string("chrome"));
    json_array_append_new(array, array_sub);
    array_sub  = json_array();
    json_array_append_new(array_sub, json_string("20192"));
    json_array_append_new(array_sub, json_string("9.9"));
    json_array_append_new(array_sub, json_string("root"));
    json_array_append_new(array_sub, json_string("chrome"));
    json_array_append_new(array, array_sub);
    array_sub  = json_array();
    json_array_append_new(array_sub, json_string("20192"));
    json_array_append_new(array_sub, json_string("9.9"));
    json_array_append_new(array_sub, json_string("root"));
    json_array_append_new(array_sub, json_string("chrome"));
    json_array_append_new(array, array_sub);
    array_sub  = json_array();
    json_array_append_new(array_sub, json_string("20192"));
    json_array_append_new(array_sub, json_string("9.9"));
    json_array_append_new(array_sub, json_string("root"));
    json_array_append_new(array_sub, json_string("chrome"));
    json_array_append_new(array, array_sub);
    array_sub  = json_array();
    json_array_append_new(array_sub, json_string("20192"));
    json_array_append_new(array_sub, json_string("9.9"));
    json_array_append_new(array_sub, json_string("root"));
    json_array_append_new(array_sub, json_string("chrome"));
    json_array_append_new(array, array_sub);
    array_sub  = json_array();
    json_array_append_new(array_sub, json_string("20192"));
    json_array_append_new(array_sub, json_string("9.9"));
    json_array_append_new(array_sub, json_string("root"));
    json_array_append_new(array_sub, json_string("chrome"));
    json_array_append_new(array, array_sub);
    array_sub  = json_array();
    json_array_append_new(array_sub, json_string("20192"));
    json_array_append_new(array_sub, json_string("9.9"));
    json_array_append_new(array_sub, json_string("root"));
    json_array_append_new(array_sub, json_string("chrome"));
    json_array_append_new(array, array_sub);

    json_object_set_new(object_res, "Process_10T_Mem", array);

    return 0;
}

onion_connection_status status_rpc(void *_, onion_request * req, onion_response * res) 
{
#if 0
    if(onion_request_get_header(req, "Origin"))
        onion_response_set_header(res, "Access-Control-Allow-Origin", onion_request_get_header(req, "Origin"));

    onion_response_set_header(res, "Access-Control-Allow-Credentials", "true");//设置是否发送cookie，需要在客户端同时设置才会生效
    onion_response_set_header(res, "Access-Control-Allow-Methods", "POST");
    onion_response_set_header(res, "Access-Control-Allow-Headers", "X-Requested-With,Content-Type");
    onion_response_set_header(res, "Content-Security-Policy", "upgrade-insecure-requests");
#endif

    json_t *object_res = json_object();
    if(!object_res)
    {
        //return 0;
        goto return_proc;
    }

    status_json_add_mem(object_res);
    status_json_add_swap(object_res);
    status_json_add_disk(object_res);
    status_json_add_time(object_res);
    status_json_add_user(object_res);
    status_json_add_uptime(object_res);
    status_json_add_cpu_cores(object_res);
    status_json_add_cpu_name(object_res);
    status_json_add_network(object_res);
    status_json_add_cpus(object_res);
    status_json_add_process_10t_cpu(object_res);
    status_json_add_process_10t_mem(object_res);

    char *resstr = json_dumps(object_res, JSON_PRESERVE_ORDER);
    if(!resstr)
    {
        json_decref(object_res);
        goto return_proc;
    }

    //char buf[3000] = {0};
    //strncpy(buf, resstr, sizeof(buf));
    //read_file_to_string(&buf[strlen(buf) - 1], "data.json");
    onion_response_write(res, resstr, strlen(resstr));

    free(resstr);
    json_decref(object_res);
return_proc:
    return OCS_PROCESSED;
}

int auth_value_check(onion_request * req, onion_response * res,
        char *username, char *password)
{ 
    json_t *auth_json;
    json_error_t error;

    const onion_block *dreq = onion_request_get_data(req);
    if (!dreq){
        printf("drep NULL\n");
        return -1;
    }
    printf("dreq:[%s]\n", onion_block_data(dreq));

    auth_json = json_loads(onion_block_data(dreq), 0, &error);
    if (!auth_json) {                   // safe_strcmp(onion_request_get_header(req, "content-type"), "application/json") // Not used in tinyrpc, not really mandatory
        onion_response_write0(res,
                "{\"jsonrpc\": \"2.0\", \"error\": {\"code\": -32700, \"message\": \"Parse error\"}, \"id\": null}");
        printf("jrep NULL\n");
        return -1;
    }

    json_t *jsonrpc_json_t = json_object_get(auth_json, "jsonrpc");
    if (!jsonrpc_json_t) {                   // safe_strcmp(onion_request_get_header(req, "content-type"), "application/json") // Not used in tinyrpc, not really mandatory
        onion_response_write0(res,
                "{\"jsonrpc\": \"2.0\", \"error\": {\"code\": -32700, \"message\": \"Parse error\"}, \"id\": null}");
        printf("jrep NULL\n");
        return -1;
    }
    printf("jsonrpc:%s\n", json_string_value(jsonrpc_json_t));

    json_t *method_json_t = json_object_get(auth_json, "method");
    if (!jsonrpc_json_t) {                   // safe_strcmp(onion_request_get_header(req, "content-type"), "application/json") // Not used in tinyrpc, not really mandatory
        onion_response_write0(res,
                "{\"jsonrpc\": \"2.0\", \"error\": {\"code\": -32700, \"message\": \"Parse error\"}, \"id\": null}");
        printf("jrep NULL\n");
        return -1;
    }
    printf("method:%s\n", json_string_value(method_json_t));

    json_t *params_json_t = json_object_get(auth_json, "params");
    if (!jsonrpc_json_t) {                   // safe_strcmp(onion_request_get_header(req, "content-type"), "application/json") // Not used in tinyrpc, not really mandatory
        onion_response_write0(res,
                "{\"jsonrpc\": \"2.0\", \"error\": {\"code\": -32700, \"message\": \"Parse error\"}, \"id\": null}");
        printf("jrep NULL\n");
        return -1;
    }

    if(json_array_get(params_json_t, 0))
        strncpy(username, json_string_value(json_array_get(params_json_t, 0)), 128 - 1);

    if(json_array_get(params_json_t, 1))
        strncpy(password, json_string_value(json_array_get(params_json_t, 1)), 128 - 1);
    
    json_decref(auth_json);
    return 0;
}

#if 0
int auth_by_local(char *username, char *md5_password)
{
    int res;
    res = strncmp(username, g_args->username, sizeof(g_args->username));
    res |= strncmp(md5_password, g_args->password, sizeof(g_args->password));
    return res;
}
#endif

onion_connection_status auth_handler(void *_, onion_request * req, onion_response * res) 
{ 
    char username[128] = {0};
    char password[128] = {0};
    static char md5_username[128] = {0};
    static char md5_password[128] = {0};
    unsigned char md5_result[16];

#if 0
    if(onion_request_get_header(req, "Origin"))
        onion_response_set_header(res, "Access-Control-Allow-Origin", onion_request_get_header(req, "Origin"));
    onion_response_set_header(res, "Access-Control-Allow-Credentials", "true");//设置是否发送cookie，需要在客户端同时设置才会生效
    onion_response_set_header(res, "Access-Control-Allow-Methods", "POST");
    onion_response_set_header(res, "Access-Control-Allow-Headers", "X-Requested-With,Content-Type");
    onion_response_set_header(res, "Content-Security-Policy", "upgrade-insecure-requests");
#endif

    if(auth_value_check(req, res, username, password) == -1)
        return OCS_PROCESSED;
    printf("username=[%s] passrod=[%s]\n", username, password);

    md5((const uint8_t *)username, strlen(username), md5_result);
    for (int i = 0; i < 16; i++)
        sprintf(md5_username + (2 * i), "%2.2x", md5_result[i]);

    md5((const uint8_t *)password, strlen(password), md5_result);
    for (int i = 0; i < 16; i++)
        sprintf(md5_password + (2 * i), "%2.2x", md5_result[i]);

#if 0
    if(auth_by_local(username, md5_password) != 0)
    {
        onion_response_write0(res,
                "{\"jsonrpc\": \"2.0\", \"error\": {\"code\": -32700, \"message\": \"Auth error\"}, \"id\": null}");
        printf("discuz auth false\n");
        return OCS_PROCESSED;
    }
    //printf("username = [%s] strlen = [%ld] \n", md5_username, strlen(md5_username));
#endif

    onion_request_session_free(req);
    /*没有则创建session*/
    onion_dict *session = onion_request_get_session_dict(req);
    onion_dict_lock_write(session);
    onion_dict_add(session, "username", md5_username, OD_STRING);   //必须是32位加密
    onion_dict_add(session, "sessionid", req->session_id, 0);
    onion_dict_unlock(session);
    printf("data = %s\n", onion_block_data(onion_dict_to_json(session)));
    printf("query = %s\n", onion_request_get_query(req, "sessionid"));
    printf("req->session = %s ", onion_dict_get(session, "sessionid"));

    char auth_rpc_s[2048] = {0};
    sprintf(auth_rpc_s, "{\"jsonrpc\": \"2.0\", \"result\": \"%s\", \"error\": null}", onion_dict_get(session, "sessionid"));
    onion_response_write0(res, auth_rpc_s);
    return OCS_PROCESSED;
}

int status_json_add_uname(json_t *object_res)
{
    json_t *array = json_array();
    struct utsname uts_buf;
    if(uname(&uts_buf))
    {
        perror("uname");
        return -1;
    }

    if(!array)
        return -1;

    json_array_append_new(array, json_string(uts_buf.sysname));
    json_array_append_new(array, json_string(uts_buf.nodename));
    json_array_append_new(array, json_string(uts_buf.release));
    json_array_append_new(array, json_string(uts_buf.version));
    json_array_append_new(array, json_string(uts_buf.machine));

    json_object_set_new(object_res, "uname", array);

    return 0;
}

onion_connection_status uname_rpc(void *_, onion_request * req, onion_response * res) 
{
#if 0
    if(onion_request_get_header(req, "Origin"))
        onion_response_set_header(res, "Access-Control-Allow-Origin", onion_request_get_header(req, "Origin"));

    onion_response_set_header(res, "Access-Control-Allow-Credentials", "true");//设置是否发送cookie，需要在客户端同时设置才会生效
    onion_response_set_header(res, "Access-Control-Allow-Methods", "POST");
    onion_response_set_header(res, "Access-Control-Allow-Headers", "X-Requested-With,Content-Type");
    onion_response_set_header(res, "Content-Security-Policy", "upgrade-insecure-requests");
#endif

    json_t *object_res = json_object();
    if(!object_res)
        goto return_proc;

    status_json_add_uname(object_res);

    char *resstr = json_dumps(object_res, JSON_PRESERVE_ORDER);
    if(!resstr)
    {
        json_decref(object_res);
        goto return_proc;
    }

    onion_response_write(res, resstr, strlen(resstr));
    free(resstr);

    json_decref(object_res);
return_proc:
    return OCS_PROCESSED;
}

void *file_rpc(void *data) 
{
    onion *o = onion_new(O_POOL);

    onion_set_max_post_size(o, 1024 * 1024 * 10);   //1M
    onion_set_max_file_size(o, 1024 * 1024 * 1024); //1G
    onion_url *url = onion_root_url(o);
    //onion_url *url = onion_url_new();

    onion_url_add(url, "jsonrpc", (void *)strip_rpc);
    onion_url_add(url, "status", (void *)status_rpc);
    onion_url_add(url, "unamerpc", (void *)uname_rpc);
    //onion_url_add(url, "^", (void *)www_resource);

    //onion_handler *otop = onion_handler_auth_pam("Onion Top", "login", onion_url_to_handler(url));
    //onion_set_root_handler(o, otop);  // should be onion to ask for user.
    
    onion_set_port(o, "20002");
    onion_listen(o);
    return NULL;
}

int file_rpc_thread()
{
    int ret=0;
    pthread_t tid1;
    pthread_mutex_init(&rpc_lock, NULL);  
    ret = pthread_create(&tid1, NULL, file_rpc, NULL);
    if(ret)
    {
        printf("create file rpc error!\n");
        return -1; 
    }

    printf("create file rpc successfully!\n");
    return 0;
}
