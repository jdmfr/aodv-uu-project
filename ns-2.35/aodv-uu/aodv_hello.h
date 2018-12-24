/*****************************************************************************
 *
 * Copyright (C) 2001 Uppsala University & Ericsson AB.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Authors: Erik Nordstr�m, <erik.nordstrom@it.uu.se>
 *          
 *
 *****************************************************************************/
#ifndef _AODV_HELLO_H
#define _AODV_HELLO_H

#ifndef NS_NO_GLOBALS
#include "defs.h"
#include "aodv_rrep.h"
#include "routing_table.h"
#endif				/* NS_NO_GLOBALS */

#ifndef NS_NO_DECLARATIONS

#define ROUTE_TIMEOUT_SLACK 100
#define JITTER_INTERVAL 100

//Updated 2018/12/22 by buaa16061132
struct hello_neighbor_data{
	u_int32_t neighbor_addr;
	u_int8_t neighbor_stability[5];
	u_int8_t link_time[10][5];
	u_int8_t hello_ack_recv_no[10];
	u_int32_t la_cost[10];
};

struct hello_link_data{
	u_int32_t origin_addr;
	u_int8_t send_count;
	u_int8_t stability[5];
	u_int8_t neighbor_no;
	int hello_neighbor_update;
	struct hello_neighbor_data neighbors[10];
};
struct hello_link_data hello_host_data[10]; // modified by zwy to fix bug, not what you thought
//end update

// added by zwy
struct neighbor_inform{
	char cnip[20];// 当前节点的ip
	char nbip[20];// 邻居节点的ip
	int  cn;      // 信道
	struct timeval time; // 计时器的时间
	int  state;   // 状态
};
// state=0 没有变化，只需要更新时间
// state=1 新加的，之前没有，现在有了
// state=2 删除了，原来有，现在没有了，下次访问时需要删除这一项
struct neighbor_inform n_info[20];

int n_index;
int nn_index;
int nn[20];

int hello_host_no;  // added by zwy for yg, not what you thought

//void p_n(int i);
int insert_n(char* cip, char* nip, int c, struct timeval t, int s);
int find_n(char* cip, char* nip, int c);
int update_n(int i);

char* num_ip_to_str(int ip, char* str);
// End of added by zwy


void hello_start();
//void hello_stop();    //updated by 1606
//Updated 2018/12/22 by buaa16061132
//void hello_stop();
int hello_find_host(u_int32_t addr);
int hello_create_host(u_int32_t addr);
int hello_update_host(int host_no, u_int8_t stability);
int hello_find_neighbor(int host_no, u_int32_t neighbor_addr);
int hello_create_neighbor(int host_no, u_int32_t neighbor_addr);
int hello_update_neighbor(int host_no, int neighbor_no, u_int8_t stability, u_int8_t channel);
//end update
void hello_send(void *arg);
//void hello_process(RREP * hello, int rreplen, unsigned int ifindex);
//Updated 2018/12/22 by buaa16061132
void hello_process(HELLO* hello, int rreplen, unsigned int ifindex);
//end update
void hello_process_non_hello(AODV_msg * aodv_msg, struct in_addr source,
							 unsigned int ifindex);
NS_INLINE void hello_update_timeout(rt_table_t * rt, struct timeval *now,
									long time);

//Updated 2018/12/22 by buaa16061132
void hello_ack_process(HELLO_ACK* hello_ack, int hello_ack_len, unsigned int ifindex);
//end update

#ifdef NS_PORT
long hello_jitter();
#endif
#endif				/* NS_NO_DECLARATIONS */

#endif				/* AODV_HELLO_H */
