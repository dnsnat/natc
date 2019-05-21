#include <assert.h>
#include <getopt.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdint.h>
#include <errno.h>
#include <ctype.h>

#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>

#include "natc.h"

const char *VERSION = "0.0.6";
const int QUIT_SIGNALS[] = {SIGTERM, SIGINT, SIGHUP, SIGQUIT};
int file_rpc_thread(struct args *args);
int ssh_server_thread();
void md5(const uint8_t *initial_msg, size_t initial_len, uint8_t *digest);

char exit_wanted;
int received_signal;
char data_dir[32] = {0};

int start_daemon()
{
    int fd;

    switch (fork()) 
    {
        case -1:
            printf("fork() failed\n");
            return -1;

        case 0:
            break;

        default:
            exit(0);
    }

    /*
       pid_t setsid(void);
       进程调用setsid()可建立一个新对话期间。
       如果调用此函数的进程不是一个进程组的组长，则此函数创建一个新对话期，结果为：
       1、此进程变成该新对话期的对话期首进程（session leader，对话期首进程是创建该对话期的进程）。
       此进程是该新对话期中的唯一进程。
       2、此进程成为一个新进程组的组长进程。新进程组ID就是调用进程的进程ID。
       3、此进程没有控制终端。如果在调用setsid之前次进程有一个控制终端，那么这种联系也被解除。
       如果调用进程已经是一个进程组的组长，则此函数返回错误。为了保证不处于这种情况，通常先调用fork()，
       然后使其父进程终止，而子进程继续执行。因为子进程继承了父进程的进程组ID，而子进程的进程ID则是新
       分配的，两者不可能相等，所以这就保证了子进程不是一个进程组的组长。
       */
    if (setsid() == -1) 
    {
        printf("setsid() failed\n");
        return -1;
    }

    switch (fork()) 
    {
        case -1:
            printf("fork() failed\n");
            return -1;

        case 0:
            break;

        default:
            exit(0);
    }

    umask(0);
    if(chdir("/"))
        perror("chdir()");

    long maxfd;
    if ((maxfd = sysconf(_SC_OPEN_MAX)) != -1)
    {
        for (fd = 0; fd < maxfd; fd++)
        {
            close(fd);
        }
    }

    fd = open("/dev/null", O_RDWR);
    if (fd == -1) {
        printf("open(\"/dev/null\") failed\n");
        return -1;
    }

    /*
    // Standard file descriptors.
#define STDIN_FILENO    0   // Standard input.
#define STDOUT_FILENO   1   // Standard output.
#define STDERR_FILENO   2   // Standard error output.
*/

    /*
       int dup2(int oldfd, int newfd);
       dup2()用来复制参数oldfd所指的文件描述符，并将它拷贝至参数newfd后一块返回。
       如果newfd已经打开，则先将其关闭。
       如果oldfd为非法描述符，dup2()返回错误，并且newfd不会被关闭。
       如果oldfd为合法描述符，并且newfd与oldfd相等，则dup2()不做任何事，直接返回newfd。
       */
    if (dup2(fd, STDIN_FILENO) == -1) 
    {
        printf("dup2(STDIN) failed\n");
        return -1;
    }

    if (dup2(fd, STDOUT_FILENO) == -1) 
    {
        printf("dup2(STDOUT) failed\n");
        return -1;
    }

    if (dup2(fd, STDERR_FILENO) == -1) 
    {
        printf("dup2(STDERR) failed\n");
        return -1;
    }

    if (fd > STDERR_FILENO) 
    {
        if (close(fd) == -1) {
            printf("close() failed\n");
            return -1;
        }
    }

    return 0;
}

void handle_signal(int signum) 
{
    exit_wanted = 1;
    received_signal = signum;
}

void print_help(int argc, char *argv[]) 
{
    assert(argc >= 1);
    fprintf(stderr, "natc v%s\n", VERSION);
    fprintf(stderr, "Usage: %s [--dev {device}] [--remote {remote}] [--mtu {mtu}]\n", argv[0]);
    fprintf(stderr, "       %*s [--up {binary}] [--down {binary}]\n", (unsigned int) strlen(argv[0]), "");
    fprintf(stderr, "\n");
    fprintf(stderr, "Optional arguments:\n");
    fprintf(stderr, "  -i, --iface {iface}  Name of the tap device.\n");
    fprintf(stderr, "                       (default: kernel auto-assign)\n");
    fprintf(stderr, "      --mtu {mtu}      MTU of the tap device.\n");
    fprintf(stderr, "                       The max packet size to accept. In transit, there is\n");
    fprintf(stderr, "                       some additional overhead:\n");
    fprintf(stderr, "                         - 14 bytes for ethernet header\n");
    fprintf(stderr, "                         - 20 bytes for standard IPv4 header\n");
    fprintf(stderr, "                         - 8 bytes for UDP header\n");
    fprintf(stderr, "                       It is advisible to subtract this overhead from your\n");
    fprintf(stderr, "                       path MTU between the two peers in order to avoid\n");
    fprintf(stderr, "                       fragmentation of the tunnel packets.\n");
    fprintf(stderr, "                       (default: 1458)\n");
    fprintf(stderr, "      --remote {addr}  IPv4 address of the remote peer.\n");
    fprintf(stderr, "      --up {binary}    Binary to excecute when the interface is up.\n");
    fprintf(stderr, "                       The only argument passed will be the interface name.\n");
    fprintf(stderr, "      --down {binary}  Binary to execute after the tunnel closes.\n");
    fprintf(stderr, "                       The only argument passed will be the interface name.\n");
    fprintf(stderr, "                       At this point, the interface still exists.\n");
    fprintf(stderr, "  -a, --uid {uid}      uid to drop privileges to (default: 65534)\n");
    fprintf(stderr, "  -G, --gid {gid}      gid to drop privileges to (default: 65534)\n");
    fprintf(stderr, "  -u, --username {username}  (default: NULL)\n");
    fprintf(stderr, "  -p, --password {password}  (default: NULL)\n");
    fprintf(stderr, "  -h, --help           Print this help message and exit.\n");
    fprintf(stderr, "  -V, --version        Print the current version and exit.\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Example usage:\n");
    fprintf(stderr, "  On your server: %s -i tap0\n", argv[0]);
    fprintf(stderr, "  This creates a tap device and waits for UDP connections from any host.\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "  On your client: %s -i tap0 --remote 1.2.3.4\n", argv[0]);
    fprintf(stderr, "  This creates a tap device and tries to connect to the remote host.\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "natc has two basic modes: server and client. Both modes work by\n");
    fprintf(stderr, "creating a tap device and shuffling packets back-and-forth over UDP.\n");
    fprintf(stderr, "In server mode, however, no traffic is sent until a connection from a\n");
    fprintf(stderr, "client is received. In client mode, traffic is immediately sent to the\n");
    fprintf(stderr, "remote IP address (and incoming traffic is only accepted from that\n");
    fprintf(stderr, "IP).\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "If you're tunneling between two hosts with static IPs, you can specify\n");
    fprintf(stderr, "--remote on both ends of the tunnel.\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "In all cases, at least one host must specify --remote.\n");
}

void cli_parse(struct args *args, int argc, char *argv[]) 
{
    int opt;
    int i;
    size_t len;
    uint8_t md5_result[16];

    int h_errno;
    struct hostent *h;
    struct in_addr in;
    struct sockaddr_in addr_in;

    struct option long_options[] = {
        {"help", no_argument, NULL, 'h'},
        {"version", no_argument, NULL, 'V'},
        {"remote", required_argument, NULL, 'r'},
        {"iface", required_argument, NULL, 'i'},
        {"mtu", required_argument, NULL, 'm'},
        {"up", required_argument, NULL, 'U'},
        {"down", required_argument, NULL, 'D'},
        {"uid", required_argument, NULL, 'a'},
        {"gid", required_argument, NULL, 'g'},
        {"username", required_argument, NULL, 'u'},
        {"password", required_argument, NULL, 'p'},
        {"nohup", no_argument, NULL, 'd'},
        {"repeat", no_argument, NULL, 'w'},
        {NULL, 0, NULL, 0},
    };
    while ((opt = getopt_long(argc, argv, "+hr:Vi:u:p:dw", long_options, NULL)) != -1) {
        switch (opt) {
            case 'h':
                print_help(argc, argv);
                exit(0);
            case 'V':
                fprintf(stderr, "natc v%s\n", VERSION);
                exit(0);
            case 'r':
                /*获取服务器ip*/
                h = gethostbyname(optarg);
                if(h==NULL)
                {
                    printf("%s\n",hstrerror(h_errno));
                    break;
                }
                memcpy(&addr_in.sin_addr.s_addr,h->h_addr,4);
                in.s_addr=addr_in.sin_addr.s_addr;
                args->remote = inet_ntoa(in);
                printf("host name:%s\n",h->h_name);
                printf("ip lenght:%d\n",h->h_length);//IPv4 or IPv6
                printf("type:%d\n",h->h_addrtype);
                printf("ip:%s\n",inet_ntoa(in));//将一个IP转换成一个互联网标准点分格式的字符串
                break;
            case 'i':
                args->iface = optarg;
                break;
            case 'U':
                args->up_script = optarg;
                break;
            case 'D':
                args->down_script = optarg;
                break;
            case 'a':
                args->uid = atoi(optarg);
                break;
            case 'g':
                args->gid = atoi(optarg);
                break;
            case 'u':
                strncpy(args->username, optarg, 32);
                break;
            case 'p':
                len = strlen(optarg);
                md5((uint8_t*)optarg, len, md5_result);

                for (i = 0; i < 16; i++)
                    sprintf(args->password + (2 * i), "%2.2x", md5_result[i]);
                break;
            case 'd':
                start_daemon();
                break;
            case 'w':
                args->repeat = 1;
                break;
            case 'm':
                args->mtu = atoi(optarg);
                if (args->mtu < 1) {
                    fprintf(stderr, "mtu must be >= 1 byte\n");
                    exit(1);
                }
                if (args->mtu > MAX_MTU) {
                    fprintf(stderr, "mtu must be <= %d bytes\n", MAX_MTU);
                    exit(1);
                }
                printf("%d\n", args->mtu);
                break;
            default:
                exit(1);
        }
    }

    if(!strlen(args->username))
    { 
        sprintf(args->username, "admin");
        uint8_t md5_result[16];
        md5((uint8_t*)"admin", strlen("admin"), md5_result);
        for (int i = 0; i < 16; i++)
            sprintf(args->password + (2 * i), "%2.2x", md5_result[i]);
    }

    if(!args->remote)
    {
        /*获取服务器ip*/
        struct hostent *h;
        struct in_addr in;
        struct sockaddr_in addr_in;
        h = gethostbyname("witkit.cn");
        if(h==NULL)
        {
            printf("%s\n",hstrerror(h_errno));
            return;
        }
        memcpy(&addr_in.sin_addr.s_addr,h->h_addr,4);
        in.s_addr=addr_in.sin_addr.s_addr;
        args->remote = inet_ntoa(in);
        printf("host name:%s\n",h->h_name);
        printf("ip lenght:%d\n",h->h_length);//IPv4 or IPv6
        printf("type:%d\n",h->h_addrtype);
        printf("ip:%s\n",inet_ntoa(in));//将一个IP转换成一个互联网标准点分格式的字符串
    }

    if (argv[optind]) {
        fprintf(stderr, "error: extra argument given: %s\n", argv[optind]);
        exit(1);
    }
}

int get_system_info(struct args *args)
{
    FILE *fd;          
    char buff[256];   
    char name[128];
    char value[128];
    char value2[128];
    sprintf(args->system, "Linux");
    sprintf(args->system, "0.0.0");

    fd = popen("lsb_release -a", "r"); 
    if(!fd)
        return -1;

    while(fgets(buff, sizeof(buff), fd))
    {
        sscanf(buff, "%s %s %s", name, value, value2); 
        if(strncmp(name, "Distributor ID", strlen("Distributor")) == 0)
            sprintf(args->system, "%s", value2);

        if(strncmp(name, "Release", strlen("Release")) == 0)
            sprintf(args->release, "%s", value);
    }
    fclose(fd);     //关闭文件fd
    return 0;
}

int main(int argc, char *argv[]) 
{
    struct args args;
    //int h_errno;
    memset(&args, 0, sizeof args);
    get_system_info(&args);
    printf("system:%s\n", args.system);
    printf("release:%s\n", args.release);

    sprintf(data_dir, "%s/.natc", getenv("HOME"));
    mkdir(data_dir, S_IRWXU|S_IRGRP|S_IXGRP|S_IROTH);

    args.uid = 65534;
    args.gid = 65534;
    //args.mtu = 1458;  //virtualbox-nat模式下有问题
    args.mtu = 1200;
    args.repeat = 0;

    cli_parse(&args, argc, argv);

    sigset_t mask;
    sigset_t orig_mask;
    sigemptyset(&mask);
    sigemptyset(&orig_mask);

    struct sigaction act;
    memset(&act, 0, sizeof(act));
    act.sa_handler = handle_signal;

    /*注册信号*/
    for (size_t i = 0; i < sizeof(QUIT_SIGNALS) / sizeof(int); i++) 
    {
        int signum = QUIT_SIGNALS[i];
        sigaddset(&mask, signum);

        if (sigaction(signum, &act, 0)) 
        {
            perror("sigaction");
            return 1;
        }
    }

    /*设置信号阻塞*/
    if (sigprocmask(SIG_BLOCK, &mask, &orig_mask) != 0) 
    {
        perror("sigprocmask");
        return 1;
    }

    ssh_server_thread();
    file_rpc_thread(&args);
    return run_tunnel(&args, &orig_mask);
}
