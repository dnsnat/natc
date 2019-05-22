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

#ifndef _NET_H
#define _NET_H

//IP header
struct ipheader
{
    unsigned int    iph_ihl:4; 	//IP header length in 32 bits words
    unsigned int    iph_ver:4; 	//IP version
    unsigned char   iph_tos; 	// Type of service
    unsigned short  iph_len; 	// Total packet length
    unsigned short  iph_ident; 	// Identification
    unsigned short  iph_offset; //Fragment offset
    unsigned char   iph_ttl; 	// Time to live
    unsigned char   iph_protocol; // Protocol
    unsigned short  iph_chksum;	  // Header checksum
    unsigned int    iph_sourceip; // Sender IP address
    unsigned int    iph_destip;   // Destination IP address
};

//UDP header
struct udpheader
{
    unsigned short udph_srcport;    // Scource port
    unsigned short udph_destport;   // Destination port
    unsigned short udph_len;        // Length (UDP + data)
    unsigned short udph_chksum;     // UDP checksum
};

//Structure describing an ethernet interface
struct hw_eth_iface
{
    int  index;         // Index identifying the HW interface in the system
    int  addr_len : 6;  // HW address lentgh
    char hw_addr[6];    // HW address
};

//Checksum calculation function
unsigned short csum_ip(unsigned short *buf, int nwords);

//Find a HW network interface based on its name
struct hw_eth_iface find_iface(int sock_fd, char* iface_name);

//Build an UDP header
int build_upd_hdr(void* ptr, unsigned short len, unsigned short src_port, unsigned short dst_port);

//Build an ipV4 header
int build_ip4_hdr(void *ptr, unsigned short data_len, char* src_addr, char* dst_addr, unsigned char proto);

// Build an UDP over IPV4 packet
int build_ip4_udp_pkt(char* buffer, int buff_len, char* data, unsigned short data_len, char* src_addr, char* dst_addr, unsigned short src_port, unsigned short dst_port, unsigned char proto);

#endif
