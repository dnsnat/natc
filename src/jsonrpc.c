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


pthread_mutex_t rpc_lock;  

/*file manage rpc*/
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

int dispach_list(onion_response * res, cJSON* cjson)
{
    const char *path = cJSON_GetObjectItem(cjson, "path")->valuestring;
    if(!path)
    {
		printf(" path is null !\r\n");
		return -1;
    }
    printf("path=%s\n", path);

    char dir_name[MAXBUF];
    snprintf(dir_name, sizeof(dir_name) - 1, ".%s", path);

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
	
    cJSON *object_res = cJSON_CreateObject();
    if(!object_res)
    {
        return 0;
    }
    cJSON *array      = cJSON_CreateArray();
    if(!array)
    {
        cJSON_Delete(object_res);
        return 0;
    }

    /* read all the files in the dir ~ */
    while( ( filename = readdir(dir) ) != NULL )
    {
        // get rid of "." and ".."
        if( strcmp( filename->d_name , "." ) == 0 || 
                strcmp( filename->d_name , "..") == 0    )
            continue;

        cJSON *object_res_sub = cJSON_CreateObject();
        if(!object_res)
        {
            cJSON_Delete(array);
            cJSON_Delete(object_res);
            return 0;
        }

        cJSON_AddStringToObject(object_res_sub, "name", filename->d_name);
        if(DT_DIR == filename->d_type) 
            cJSON_AddStringToObject(object_res_sub, "type", "dir");
        else
            cJSON_AddStringToObject(object_res_sub, "type", "file");

#if 1
        struct stat file_stat;
        char filename_full[MAXBUF + NAME_MAX + 1];
        snprintf(filename_full, sizeof(filename_full) - 1, "%s%s", dir_name, filename->d_name);
        stat(filename->d_name, &file_stat);
        char mode_buf[12];
        getMode(file_stat.st_mode, mode_buf);
        cJSON_AddStringToObject(object_res_sub, "rights", mode_buf);
        cJSON_AddNumberToObject(object_res_sub, "size", file_stat.st_size);

        char date_time[1024];
        struct tm now_time;
        localtime_r(&file_stat.st_atime, &now_time);

        //strftime(date_time, sizeof(date_time), "%Y-%m-%d %H:%M:%S", localtime_r(&file_stat.st_mtime, &t));
        sprintf(date_time, "%d-%d-%d %d:%d:%d\n", now_time.tm_year + 1900, now_time.tm_mon + 1,
                now_time.tm_mday, now_time.tm_hour, now_time.tm_min, now_time.tm_sec);
        cJSON_AddStringToObject(object_res_sub, "date", date_time);
        //printf("clock_gettime : date_time=%s, tv_nsec=%ld\n", date_time, file_stat.st_mtim.tv_nsec);

#endif

        cJSON_AddItemToArray(array, object_res_sub);
    }

    cJSON_AddItemToObject(object_res, "result", array);

    char *resstr = cJSON_PrintUnformatted(object_res);
    cJSON_Delete(object_res);

    printf("result=%s\n",resstr);
    onion_response_write0(res, resstr);
    free(resstr);
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

onion_connection_status strip_rpc(void *_, onion_request * req, onion_response * res) 
{ 
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

#if 0
    void *iter;
    json_t *object;
    json_error_t error;
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
#endif


    cJSON* cjson = cJSON_Parse(data); 
    if(cjson == NULL)
    {
        printf("json pack into cjson error...");
        return OCS_PROCESSED;
    }

    const char *action = cJSON_GetObjectItem(cjson, "action")->valuestring;
    if(!action)
    {
        cJSON_Delete(cjson);
        return OCS_PROCESSED;
    }
        
    printf("action : [%s]\n", action);

    if(strncmp(action, "list", strlen("list")) == 0)
        dispach_list(res, cjson);

    cJSON_Delete(cjson);
    return OCS_PROCESSED;
}

/*status_rpc*/
cJSON *jsonrpc_mem()
{
    cJSON *array = NULL;
    array = cJSON_CreateArray();

    MEM_OCCUPY mem[4] = {0};
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

    MEM_OCCUPY mem[4] = {0};
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

char *jsonrpc_time()
{
    time_t now; //实例化time_t结构    
    struct tm *timenow; //实例化tm结构指针    
    time(&now);   
    timenow = localtime(&now);   
    return asctime(timenow);
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
    cJSON_AddStringToObject(root, "Time", jsonrpc_time());

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

onion_connection_status http_jsonrpc_status(void *_, onion_request * req, onion_response * res) 
{
    char *resstr = jsonrpc_status();
    onion_response_write0(res, resstr);
    free(resstr);
    return OCS_PROCESSED;
}

void *http_jsonrpc(void *data) 
{
    onion *o = onion_new(O_POOL|O_THREADS_ENABLED|O_THREADS_AVAILABLE|O_DETACHED);

    onion_set_max_post_size(o, 1024 * 1024 * 10);   //1M
    onion_set_max_file_size(o, 1024 * 1024 * 1024); //1G
    onion_url *url = onion_root_url(o);
    //onion_url *url = onion_url_new();

    onion_url_add(url, "jsonrpc", (void *)strip_rpc);
    onion_url_add(url, "status", (void *)http_jsonrpc_status);
    //onion_url_add(url, "unamerpc", (void *)uname_rpc);

    //onion_handler *otop = onion_handler_auth_pam("Onion Top", "login", onion_url_to_handler(url));
    //onion_set_root_handler(o, otop);  // should be onion to ask for user.

    onion_set_port(o, "20002");
    onion_listen(o);
    return NULL;
}

int http_jsonrpc_thread()
{
    int ret=0;
    pthread_t tid1;
    pthread_mutex_init(&rpc_lock, NULL);  
    ret = pthread_create(&tid1, NULL, http_jsonrpc, NULL);
    if(ret)
    {
        printf("create file rpc error!\n");
        return -1; 
    }

    printf("create file rpc successfully!\n");
    return 0;
}

/*UDP*/
#if 0
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
#endif
