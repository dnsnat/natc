/*
 * =====================================================================================
 *
 *       Filename:  httpd.c
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  06/13/2019 09:25:57 AM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  YOUR NAME (), 
 *   Organization:  
 *
 * =====================================================================================
 */
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>

#include <onion/onion.h>
#include <onion/log.h>
#include <onion/dict.h>
#include <dirent.h>
#include <sys/stat.h>
#include <onion/shortcuts.h>
#include <locale.h>
#include <libintl.h>
#include "httpd.h"
#include "jsonrpc.h"

int fileserver_page(const char *basepath, onion_request *req, onion_response * res)
{
    char dirname[1024];

    if(strlen(onion_request_get_path(req)))
            snprintf(dirname, sizeof(dirname), "%s/%s", basepath, onion_request_get_path(req));
    else
            snprintf(dirname, sizeof(dirname), "%s/%s", basepath, "/index.html");

    char *realp = realpath(dirname, NULL);
    if (!realp)
        return OCS_INTERNAL_ERROR;

    // First check if it exists and so on. If it does not exist, no trying to escape message
    struct stat reals;
    int ok = stat(realp, &reals);
    if (ok < 0)                   // Cant open for even stat
    {
        ONION_DEBUG0("Not found %s.", tmp);
        return 0;
    }

    if (S_ISDIR(reals.st_mode)) 
    {
        //ONION_DEBUG("DIR");
        return OCS_NOT_PROCESSED;
    } 
    else if (S_ISREG(reals.st_mode)) {
        //ONION_DEBUG("FILE");
        return onion_shortcut_response_file(realp, req, res);
    }
    return OCS_NOT_PROCESSED;
}

void *httpd(void *arg) 
{
    struct args *args = arg;
    onion *o = NULL;
    char *hostname = "::";
    char *basepath = args->httpd_path;
    //char *port     = args->httpd_port;

    o = onion_new(O_POOL|O_THREADS_ENABLED|O_THREADS_AVAILABLE|O_DETACHED);
    onion_set_max_post_size(o, 1024 * 1024 * 10);   //1M
    onion_set_max_file_size(o, 1024 * 1024 * 1024); //1G
    //onion_set_port(o, port);
    onion_set_port(o, "20002");
    onion_set_hostname(o, hostname);

    //onion_handler *root = onion_handler_new((onion_handler_handler)fileserver_page, (void *)basepath, NULL);
    //onion_set_root_handler(o, root);

    onion_url *url = onion_root_url(o);
    onion_url_add(url, "jsonrpc", (void *)strip_rpc);
    onion_url_add(url, "status", (void *)http_jsonrpc_status);
    onion_url_add_handler(url, "^", onion_handler_new((onion_handler_handler)fileserver_page, (void *)basepath, NULL));

    int error = onion_listen(o);
    if (error) {
        perror("Cant create the server");
    }

    onion_free(o);
    return 0;
}

int httpd_thread(struct args *args)
{
    int ret=0;
    pthread_t tid1;
    ret = pthread_create(&tid1, NULL, httpd, args);
    if(ret)
        printf("create httpd thread error!\n");
    printf("create  httpd thread successfully!\n");
    return ret;
}

