/*
 * =====================================================================================
 *
 *       Filename:  dhclient.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  08/16/2018 04:52:16 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  YOUR NAME (), 
 *   Organization:  
 *
 * =====================================================================================
 */
#ifndef _DHCLIENT_H
#define _DHCLIENT_H

int run_dhcp_thread(char *interface);
int run_dhcp(char *interface);
void *get_dhcp(char *interface);

int get_dhcp_block(char *interface);

#endif
