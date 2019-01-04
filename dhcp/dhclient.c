#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <linux/if_arp.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/udp.h>
#include <arpa/inet.h>

#include <pthread.h>

#include "dhcp.h"
#include "net.h"

#define BUFF_SIZE 1024
char if_ip[32] = {0};

#if 0
void main(int argc, char** argv)
{
    if(geteuid() != 0)
    {
        fprintf(stderr, "You need root permissions\n");
        exit(1);
    }

    //TODO : Add a debug mode
    char* interface;
    if(argc > 1)
        interface = argv[1];
    else
    {
        fprintf(stderr, "Please specify network interface to use\n");
        exit(1);
    }

//    printf("Interface = %s\n", interface);

    int sock = socket(PF_PACKET, SOCK_DGRAM, htons(ETH_P_IP));
	
	//Set receive timeout
	struct timeval tv;
	tv.tv_usec = 0;
	tv.tv_sec = 10; //10 seconds in case of latency on the network
	setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    //retrieve ethernet NIC index and HW address
    struct hw_eth_iface iface = find_iface(sock, interface); // TODO : Check interface existency, and print a list of possible NIC

    struct sockaddr_ll target;
    memset(&target, 0, sizeof(target));
    target.sll_family   = PF_PACKET;
    target.sll_protocol = htons(ETH_P_IP);
    target.sll_ifindex  = iface.index;
    target.sll_hatype   = ARPHRD_ETHER;
    target.sll_pkttype  = PACKET_HOST;
    target.sll_halen    = ETH_ALEN;

    memset(target.sll_addr, 0xff, 6);

    char buffer[BUFF_SIZE];

//    struct dhcp_pkt *dhcp = (struct dhcp_pkt*)(buffer + sizeof(struct udpheader) + sizeof(struct ipheader));
    struct dhcp_pkt dhcp;
    int dhcp_len = build_dhcp_discover(&dhcp, iface.hw_addr, iface.addr_len);

    int len = build_ip4_udp_pkt(buffer, BUFF_SIZE, (unsigned char*)&dhcp, dhcp_len, "0.0.0.0", "255.255.255.255", 68, 67, IPPROTO_UDP);

    char if_up[1024];
    sprintf(if_up, "ifconfig %s up" ,argv[1]);
    system(if_up);

	//Send the packet over the network
    if(sendto(sock, buffer, len, 0, (struct sockaddr *)&target, sizeof(target)) < 0)
    {
        perror("Error while writing to socket");
        exit(1);
    }


	//Now, wait for the server response, and read it

receive:
    memset(buffer, 0, BUFF_SIZE);

	//Read a packet
    int read_len = recvfrom(sock, buffer, BUFF_SIZE, 0, NULL, NULL);
    if(read_len <= 0)
    {
        perror("Cannot read");
        exit(1);
    }

    struct ipheader *rip   = (struct ipheader*) buffer;
    struct udpheader *rudp = (struct udpheader*)(buffer + sizeof(struct ipheader));
    struct dhcp_pkt *rdhcp = (struct dhcp_pkt*)(buffer + sizeof(struct udpheader) + sizeof(struct ipheader));

    //Check packet validity
	// if dest port isn't our or packet is not a dhcp one, drop the packet
    if(rip->iph_protocol != IPPROTO_UDP || rudp->udph_destport != htons(68) || !is_dhcp(rdhcp) || rdhcp->op != OP_BOOT_REPLY)
        goto receive;

//    printf("Data Recieved, length = %d\n", read_len);

	//Find the IP attributed to us by the server
    struct in_addr raddr;

    struct in_addr ip_addr;
    struct in_addr netmask;
    ip_addr.s_addr = rdhcp->yi_addr;

    printf("IPADDR=%s\n", inet_ntoa(ip_addr));

	//Now check DHCP options, and process them
    struct dhcp_opt *opt = NULL;
    int offs = 0;
    opt = get_dhcp_option(rdhcp, &offs);
    while(opt != NULL)
    {
//        printf("OPT FOUND offset = %d\n", offs);
        switch(opt->id)
        {
            case OPTION_ROUTER: // If the option is the gateway address
				if(opt->len == 4)
				{
					raddr.s_addr = char_to_ip(opt->values);
					printf("GATEWAY=%s\n", inet_ntoa(raddr));
				}
                break;

            case OPTION_SUBNET_MASK: // If the option is the netwmask
				if(opt->len == 4)
				{
					netmask.s_addr = char_to_ip(opt->values);
					printf("NETMASK=%s\n", inet_ntoa(netmask));
				}
                break;

            case OPTION_DNS: // If option is the DNS addresses
				if(opt->len % 4 == 0)
				{
					int i = 0;
					printf("NAMESERVER=");
					int max = opt->len / 4;
					for(i = 0; i < max; i++)
					{
						raddr.s_addr = char_to_ip(opt->values + 4*i);
						printf("%s", inet_ntoa(raddr));
						if(i < max - 1)
							printf(",");
					}
					printf("\n");
				}
                break;

            default:
                break;
        }
        opt = get_dhcp_option(rdhcp, &offs);
    }

    char if_ip[32];
    sprintf(if_ip, "%s", inet_ntoa(ip_addr));
    sprintf(if_up, "ifconfig %s %s netmask %s", argv[1], if_ip, inet_ntoa(netmask));
    system(if_up);

    close(sock);
    exit(0);
}
#endif

#if 0
void *get_dhcp(char *interface)
{
    //TODO : Add a debug mode

    printf("dhcp: [%s]\r\n", interface);
    int sock = socket(PF_PACKET, SOCK_DGRAM, htons(ETH_P_IP));
	
	//Set receive timeout
	struct timeval tv;
	tv.tv_usec = 0;
	tv.tv_sec = 3000; //3 seconds in case of latency on the network

    struct timeval timeout= {10, 0}; //10S 超时
	setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    //retrieve ethernet NIC index and HW address
    struct hw_eth_iface iface = find_iface(sock, interface); // TODO : Check interface existency, and print a list of possible NIC

    struct sockaddr_ll target;
    memset(&target, 0, sizeof(target));
    target.sll_family   = PF_PACKET;
    target.sll_protocol = htons(ETH_P_IP);
    target.sll_ifindex  = iface.index;
    target.sll_hatype   = ARPHRD_ETHER;
    target.sll_pkttype  = PACKET_HOST;
    target.sll_halen    = ETH_ALEN;

    memset(target.sll_addr, 0xff, 6);

    char buffer[BUFF_SIZE];

//    struct dhcp_pkt *dhcp = (struct dhcp_pkt*)(buffer + sizeof(struct udpheader) + sizeof(struct ipheader));
    struct dhcp_pkt dhcp;
    int dhcp_len = build_dhcp_discover(&dhcp, iface.hw_addr, iface.addr_len);

    int len = build_ip4_udp_pkt(buffer, BUFF_SIZE, (unsigned char*)&dhcp, dhcp_len, "0.0.0.0", "255.255.255.255", 68, 67, IPPROTO_UDP);

    char if_up[1024];
    sprintf(if_up, "ifconfig %s up", interface);
    system(if_up);

	//Send the packet over the network
    if(sendto(sock, buffer, len, 0, (struct sockaddr *)&target, sizeof(target)) < 0)
    {
        perror("Error while writing to socket");
        exit(1);
    }
    printf("send dhcp request\r\n");


	//Now, wait for the server response, and read it

    int recv_num = 0;
receive:
    if(recv_num++ > 100)
    {
        printf("dhcp thread error\r\n");
        return NULL;
    }

    memset(buffer, 0, BUFF_SIZE);

	//Read a packet
    int read_len = recvfrom(sock, buffer, BUFF_SIZE, 0, NULL, NULL);
    if(read_len <= 0)
    {
        perror("Cannot read");
        exit(1);
    }
    
    struct ipheader *rip   = (struct ipheader*) buffer;
    struct udpheader *rudp = (struct udpheader*)(buffer + sizeof(struct ipheader));
    struct dhcp_pkt *rdhcp = (struct dhcp_pkt*)(buffer + sizeof(struct udpheader) + sizeof(struct ipheader));


    //Check packet validity
	// if dest port isn't our or packet is not a dhcp one, drop the packet
    if(rip->iph_protocol != IPPROTO_UDP || rudp->udph_destport != htons(68) || !is_dhcp(rdhcp) || rdhcp->op != OP_BOOT_REPLY)
        goto receive;

//    printf("Data Recieved, length = %d\n", read_len);

    printf("recv dhcp respond\r\n");
	//Find the IP attributed to us by the server
    struct in_addr raddr;

    struct in_addr ip_addr;
    struct in_addr netmask;
    ip_addr.s_addr = rdhcp->yi_addr;

    printf("IPADDR=%s\n", inet_ntoa(ip_addr));

	//Now check DHCP options, and process them
    struct dhcp_opt *opt = NULL;
    int offs = 0;
    opt = get_dhcp_option(rdhcp, &offs);
    while(opt != NULL)
    {
//        printf("OPT FOUND offset = %d\n", offs);
        switch(opt->id)
        {
            case OPTION_ROUTER: // If the option is the gateway address
				if(opt->len == 4)
				{
					raddr.s_addr = char_to_ip(opt->values);
					printf("GATEWAY=%s\n", inet_ntoa(raddr));
				}
                break;

            case OPTION_SUBNET_MASK: // If the option is the netwmask
				if(opt->len == 4)
				{
					netmask.s_addr = char_to_ip(opt->values);
					printf("NETMASK=%s\n", inet_ntoa(netmask));
				}
                break;

            case OPTION_DNS: // If option is the DNS addresses
				if(opt->len % 4 == 0)
				{
					int i = 0;
					printf("NAMESERVER=");
					int max = opt->len / 4;
					for(i = 0; i < max; i++)
					{
						raddr.s_addr = char_to_ip(opt->values + 4*i);
						printf("%s", inet_ntoa(raddr));
						if(i < max - 1)
							printf(",");
					}
					printf("\n");
				}
                break;

            default:
                break;
        }
        opt = get_dhcp_option(rdhcp, &offs);
    }

    close(sock);

    char if_ip[32];
    sprintf(if_ip, "%s", inet_ntoa(ip_addr));
    sprintf(if_up, "ifconfig %s %s netmask %s", interface, if_ip, inet_ntoa(netmask));
    system(if_up);
    printf("dhcp thread success\r\n");
}

int run_dhcp(char *interface)
{
    get_dhcp(interface);
    return 0;
}

int run_dhcp_thread(char *interface)
{
    int ret=0;
    pthread_t id1;
    ret=pthread_create(&id1,NULL,(void*)get_dhcp, (void *)interface);
    if(ret)
    {
        printf("dhcp pthread error!\n");
        return -1; 
    }
    return 0;
}
#endif

int get_dhcp_block(char *interface)
{
    //TODO : Add a debug mode

    printf("dhcp: [%s]\r\n", interface);
    int sock = socket(PF_PACKET, SOCK_DGRAM, htons(ETH_P_IP));
	
	//Set receive timeout
    struct timeval timeout= {10, 0}; //10S 超时
	setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    //retrieve ethernet NIC index and HW address
    struct hw_eth_iface iface = find_iface(sock, interface); // TODO : Check interface existency, and print a list of possible NIC

    struct sockaddr_ll target;
    memset(&target, 0, sizeof(target));
    target.sll_family   = PF_PACKET;
    target.sll_protocol = htons(ETH_P_IP);
    target.sll_ifindex  = iface.index;
    target.sll_hatype   = ARPHRD_ETHER;
    target.sll_pkttype  = PACKET_HOST;
    target.sll_halen    = ETH_ALEN;

    memset(target.sll_addr, 0xff, 6);

    char buffer[BUFF_SIZE];

    struct dhcp_pkt dhcp;
    int dhcp_len = build_dhcp_discover(&dhcp, iface.hw_addr, iface.addr_len);

    int len = build_ip4_udp_pkt(buffer, BUFF_SIZE, (char*)&dhcp, dhcp_len, "0.0.0.0", "255.255.255.255", 68, 67, IPPROTO_UDP);


	//Send the packet over the network
    if(sendto(sock, buffer, len, 0, (struct sockaddr *)&target, sizeof(target)) < 0)
    {
        perror("Error while writing to socket");
        return -1;
    }
    printf("send dhcp request\r\n");

	//Now, wait for the server response, and read it

    int recv_num = 0;
receive:
    if(recv_num++ > 100)
    {
        printf("dhcp thread error\r\n");
        return -1;
    }

    memset(buffer, 0, BUFF_SIZE);

	//Read a packet
    int read_len = recvfrom(sock, buffer, BUFF_SIZE, 0, NULL, NULL);
    if(read_len <= 0)
    {
        perror("Cannot read");
        return -1;
    }
    
    struct ipheader *rip   = (struct ipheader*) buffer;
    struct udpheader *rudp = (struct udpheader*)(buffer + sizeof(struct ipheader));
    struct dhcp_pkt *rdhcp = (struct dhcp_pkt*)(buffer + sizeof(struct udpheader) + sizeof(struct ipheader));

    //Check packet validity
    if(rip->iph_protocol != IPPROTO_UDP || rudp->udph_destport != htons(68) || !is_dhcp(rdhcp) || rdhcp->op != OP_BOOT_REPLY)
        goto receive;
    
    //接收数据包成功
    printf("Data Recieved, length = %d\n", read_len);

	//Find the IP attributed to us by the server
    struct in_addr raddr;
    struct in_addr ip_addr;
    struct in_addr netmask;
    ip_addr.s_addr = rdhcp->yi_addr;

    printf("IPADDR=%s\n", inet_ntoa(ip_addr));

	//Now check DHCP options, and process them
    struct dhcp_opt *opt = NULL;
    int offs = 0;
    opt = get_dhcp_option(rdhcp, &offs);
    while(opt != NULL)
    {
        printf("OPT FOUND offset = %d\n", offs);
        switch(opt->id)
        {
            case OPTION_ROUTER: // If the option is the gateway address
				if(opt->len == 4)
				{
					raddr.s_addr = char_to_ip(opt->values);
					printf("GATEWAY=%s\n", inet_ntoa(raddr));
				}
                break;

            case OPTION_SUBNET_MASK: // If the option is the netwmask
				if(opt->len == 4)
				{
					netmask.s_addr = char_to_ip(opt->values);
					printf("NETMASK=%s\n", inet_ntoa(netmask));
				}
                break;

            case OPTION_DNS: // If option is the DNS addresses
				if(opt->len % 4 == 0)
				{
					int i = 0;
					printf("NAMESERVER=");
					int max = opt->len / 4;
					for(i = 0; i < max; i++)
					{
						raddr.s_addr = char_to_ip(opt->values + 4*i);
						printf("%s", inet_ntoa(raddr));
						if(i < max - 1)
							printf(",");
					}
					printf("\n");
				}
                break;

            default:
                break;
        }
        opt = get_dhcp_option(rdhcp, &offs);
    }

    close(sock);

    char if_set[128];
    sprintf(if_ip, "%s", inet_ntoa(ip_addr));
    sprintf(if_set, "ifconfig %s %s netmask %s", interface, if_ip, inet_ntoa(netmask));
    system(if_set);
    printf("dhcp thread success\r\n");
    return 0;
}

