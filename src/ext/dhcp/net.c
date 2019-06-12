/*
Copyright (c) 2014, Pierre-Henri Symoneaux
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

* Redistributions of source code must retain the above copyright notice, this
  list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

* Neither the name of LightDhcpClient nor the names of its
  contributors may be used to endorse or promote products derived from
  this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
 
#include <string.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <arpa/inet.h>

#include "net.h"

unsigned short csum_ip(unsigned short *buf, int nwords)
{
	//Checksum the buffer
    unsigned long sum;
    for(sum=0; nwords>0; nwords--)
        sum += *buf++;
    sum = (sum >> 16) + (sum &0xffff);
    sum += (sum >> 16);
    return (unsigned short)(~sum);
}

struct hw_eth_iface find_iface(int sock_fd, char* iface_name)
{
    struct hw_eth_iface iface;
    struct ifreq ifr;
    strncpy(ifr.ifr_name, iface_name, IFNAMSIZ);

	//Get the iface index
    ioctl(sock_fd, SIOCGIFINDEX, &ifr);
    iface.index = ifr.ifr_ifindex;

	//Get the iface HW address
    ioctl(sock_fd, SIOCGIFHWADDR, &ifr);

	//Copy the address in our structure
//    memcpy(iface.hw_addr, ifr.ifr_hwaddr.sa_data, 6); // LINK problem
	int i;
	for(i = 0; i < 6; i++)
		iface.hw_addr[i] = ifr.ifr_hwaddr.sa_data[i];
    iface.addr_len = 6;

    return iface;
}

int build_upd_hdr(void* ptr, unsigned short data_len, unsigned short src_port, unsigned short dst_port)
{
    struct udpheader *udp = (struct udpheader*)ptr;
    int len = sizeof(struct udpheader);
    udp->udph_srcport = htons(68);
    udp->udph_destport = htons(67);
    udp->udph_len = htons(len + data_len);
    //TODO : UDP checksum
    return len;
}

int build_ip4_hdr(void *ptr, unsigned short data_len, char* src_addr, char* dst_addr, unsigned char proto)
{
    struct ipheader *ip = (struct ipheader*)ptr;
    int len = sizeof(struct ipheader);
    ip->iph_ihl = 5;
    ip->iph_ver = 4;
    ip->iph_tos = 0;
    ip->iph_offset = 0;
    ip->iph_len = htons(len + data_len);
    ip->iph_ident = htons(54321); //TODO: random number
    ip->iph_ttl = 64; // hops
    ip->iph_protocol = proto;
    ip->iph_sourceip = inet_addr(src_addr);
    ip->iph_destip = inet_addr(dst_addr);
    ip->iph_chksum = csum_ip((unsigned short *)ip, len/2);

    return len;
}

int build_ip4_udp_pkt(char* buffer, int buff_len, char* data, unsigned short data_len, char* src_addr, char* dst_addr, unsigned short src_port, unsigned short dst_port, unsigned char proto)
{
    memset(buffer, 0, buff_len);

    struct ipheader *ip   = (struct ipheader*) buffer;
    struct udpheader *udp = (struct udpheader*)(buffer + sizeof(struct ipheader));

    int udp_len  = build_upd_hdr(udp, data_len, src_port, dst_port);
    int ip_len   = build_ip4_hdr(ip, data_len + udp_len, src_addr, dst_addr, proto);
//    memcpy(buffer + sizeof(struct udpheader) + sizeof(struct ipheader), data, data_len); // LINK problem
	int i;
	int offset = sizeof(struct udpheader) + sizeof(struct ipheader);
	for(i = 0; i < data_len; i++)
		buffer[offset + i] = data[i];

    return data_len + udp_len + ip_len;
}
