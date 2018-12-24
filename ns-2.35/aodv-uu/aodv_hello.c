/*****************************************************************************
 *
 * Copyright (C) 2001 Uppsala University and Ericsson AB.
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

#ifdef NS_PORT
#include "ns-2/aodv-uu.h"
#else
#include <netinet/in.h>
#include "aodv_hello.h"
#include "aodv_timeout.h"
#include "aodv_rrep.h"
#include "aodv_rreq.h"
#include "routing_table.h"
#include "timer_queue.h"
#include "params.h"
#include "aodv_socket.h"
#include "defs.h"
#include "debug.h"

#include "MarkovPrediction.h"
/*added by zwy*/
#include "../mac/mac-802_11.h"
#include "./ns-2/aodv-uu.h"
#include "./ns-2/svm.h"
#include <stdlib.h>
/*End of added by zwy*/

// added by zwy
int n_index = -1;
int nn_index = -1;
int nn[20] = {0};
int hello_host_no = 0;
// end of added by zwy



extern int unidir_hack, receive_n_hellos, hello_jittering, optimized_hellos;
static struct timer hello_timer;

#endif

/* #define DEBUG_HELLO */
//Updated 2018/12/22 by buaa16061132
//struct hello_link_data hello_host_data[10];  /* Storing all link data */
//int hello_host_no = 0;  /*The number of host in the hello_host_data */
//end update
//Updated 2018/12/22 by buaa16061132
int NS_CLASS hello_find_host(u_int32_t addr){
	int i = 0;
	for(i; i < hello_host_no; i ++){
		if(hello_host_data[i].origin_addr == addr){
			return i;
		}
	}

	return -1;
}

int NS_CLASS hello_create_host(u_int32_t addr){
	hello_host_data[hello_host_no].origin_addr = addr;
	int i = 0;
	for(i; i < 5; i ++){
		hello_host_data[hello_host_no].stability[i] = 1;
	}
	hello_host_data[hello_host_no].send_count = 0;
	hello_host_data[hello_host_no].neighbor_no = 0;
	hello_host_data[hello_host_no].hello_neighbor_update = 0;
	hello_host_no += 1;

	return 0;
}

int NS_CLASS hello_update_host(int host_no, u_int8_t stability){
	hello_host_data[host_no].send_count += 1;
	int i = 0;
	for(i; i < 4; i ++){
		hello_host_data[host_no].stability[i] = hello_host_data[host_no].stability[i + 1];
	}
	hello_host_data[host_no].stability[4] = stability;
	hello_host_data[host_no].hello_neighbor_update = 0;

	return 0;
}

int NS_CLASS hello_find_neighbor(int host_no, u_int32_t neighbor_addr){
	int neighbor_no = hello_host_data[host_no].neighbor_no;
	int i = 0;
	for(i; i < neighbor_no; i ++){
		if(hello_host_data[host_no].neighbors[i].neighbor_addr == neighbor_addr){
			return i;
		}
	}

	return -1;
}

int NS_CLASS hello_create_neighbor(int host_no, u_int32_t neighbor_addr){
	int neighbor_no = hello_host_data[host_no].neighbor_no;
	hello_host_data[host_no].neighbors[neighbor_no].neighbor_addr = neighbor_addr;
	int i = 0, j = 0;
	for(i; i < 5; i ++){
		hello_host_data[host_no].neighbors[neighbor_no].neighbor_stability[i] = 1;
	}
	for(i = 0; i < 10; i ++){
		hello_host_data[host_no].neighbors[neighbor_no].hello_ack_recv_no[i] = 0;
	}
	for(i = 0; i < 10; i ++){
		for(j = 0; j < 5; j ++){
			hello_host_data[host_no].neighbors[neighbor_no].link_time[i][j] = 0;
		}
	}
    hello_host_data[host_no].neighbor_no+=1;
	return 0;
}

int NS_CLASS hello_update_neighbor(int host_no, int neighbor_no, u_int8_t stability, u_int8_t channel){
	int i = 0, j = 0;
	for(i = 0; i < 4; i ++){
		hello_host_data[host_no].neighbors[neighbor_no].neighbor_stability[i]
				= hello_host_data[host_no].neighbors[neighbor_no].neighbor_stability[i + 1];
		hello_host_data[host_no].neighbors[neighbor_no].neighbor_stability[4] = stability;
	}
	hello_host_data[host_no].neighbors[neighbor_no].hello_ack_recv_no[channel] += 1;
	if(hello_host_data[host_no].hello_neighbor_update == 0){
		for(i = 0; i < 10; i ++){
			for(j = 0; j < 4; j ++){
				hello_host_data[host_no].neighbors[neighbor_no].link_time[i][j]
						= hello_host_data[host_no].neighbors[neighbor_no].link_time[i][j + 1];
			}
			hello_host_data[host_no].neighbors[neighbor_no].link_time[channel][4] = 0;
		}
		hello_host_data[host_no].hello_neighbor_update = 1;
	}
	else{
		hello_host_data[host_no].neighbors[neighbor_no].link_time[channel][4] = HELLO_INTERVAL;
	}

	int arma_prediction_val;
	int markov_prediction_val;
	int ett, ett_reverse;
	u_int32_t la_result = 0;

	ett_reverse = (hello_host_data[host_no].neighbors[neighbor_no].hello_ack_recv_no[channel])
				  / (hello_host_data[host_no].send_count);
	if(ett_reverse == 0){
		ett = 100;
	}
	else{
		ett = (int)((1 / ett_reverse) * 10);
	}

	//arma_prediction_val = ARIMA_prediction((int*)(hello_host_data[host_no].neighbors[neighbor_no].link_time[channel]));
arma_prediction_val=5;
	markov_prediction_val = markov_prediction((int*)(hello_host_data[host_no].stability),
											  (int*)(hello_host_data[host_no].neighbors[neighbor_no].neighbor_stability));

	la_result = ett + arma_prediction_val + markov_prediction_val;
	hello_host_data[host_no].neighbors[neighbor_no].la_cost[channel] = la_result;
	fprintf(stderr, "Calculated LA value between %d and %d in channel %d\n",
			hello_host_data[host_no].origin_addr, hello_host_data[host_no].neighbors[neighbor_no].neighbor_addr,
			channel);
}
//end update


//added by zwy
// 输出调试信息
//void NS_CLASS p_n(int i){
//int t;
//for(t=1; t<20; t++){
//	if(strlen(n_info[t].cnip) != 0 && strlen(n_info[t].nbip) != 0){
//		fprintf(stderr, "N_INFO[%d]::  current ip=%s, neighbor ip=%s, channel=%d, time=%ld.%ld, state=%d\n",
//			t, n_info[t].cnip, n_info[t].nbip, n_info[t].cn, (n_info[t].time).tv_sec, (n_info[t].time).tv_usec, n_info[t].state);
//	}
//}
//}

char * NS_CLASS num_ip_to_str(int ip, char* str){
	int cnt,rev;
	int i;
	char tmp;
	cnt = 0;
	while(ip>0){
		str[cnt] = (ip%10)+'0';
		ip = ip/10;
		cnt++;
	}
	if(cnt<=20){
		for(i=cnt;i<20;i++){
			str[i] = 0;
		}
	}
	cnt = cnt-1;
	for(rev=0;rev<=cnt;rev++){
		tmp = str[cnt];
		str[cnt] = str[rev];
		str[rev] = tmp;
		cnt--;
	}
	//fprintf(stderr, "str in num_ip_to_str:%s\n",str);
	return str;
}

// 查找结构体中是否已经记录了这条记录
int NS_CLASS find_n(char* cip, char* nip, int c){
	int i;
	//fprintf(stderr,"info in find_n: cip:%s, nip:%s, cn:%d\n",cip,nip,c);
	//fprintf(stderr,"addr of n_info in find_n:%d\n",&n_info);
	for(i = 0; i < 20; i++){
		if(strcmp(n_info[i].cnip, cip) == 0 && strcmp(n_info[i].nbip, nip) == 0 && n_info[i].cn == c){
			//fprintf(stderr, "FIND_N:: at n_info[%d]:%s -> %d -> %s already exist.\n", i, cip, c, nip);
			return i;
		}
	}
	// 不存在这个项
	return -1;
}

// 更新结构体中已经存在的项
int NS_CLASS update_n(int i){
	//fprintf(stderr,"n_info addr in update_n:%d\n",&n_info);
	struct timeval now;
	gettimeofday(&now, NULL);
	// 状态:之前时刻已经存在由1变为0
	n_info[i].state = 0;
	// 更新计时器的时间
	(n_info[i].time).tv_sec = now.tv_sec;
	(n_info[i].time).tv_usec = now.tv_usec;
}


// 在结构体中插入一条新的记录
int NS_CLASS insert_n(char* cip, char* nip, int c, struct timeval t, int s){
	fprintf(stderr, "------------------------* insert into n_info *------------------------\n");
	//fprintf(stderr, "before insert:\n");
	//p_n(0);
	//fprintf(stderr, "start insert:\n");
	int i = 0;
	n_index++;
	//fprintf(stderr, "curent n_index is: %d\n", n_index);
	//fprintf(stderr,"info in insert_n: cip:%s, nip:%s, cn:%d, state:%d\n",cip,nip,c,s);
	//fprintf(stderr,"addr of n_info in insert_n:%d",&n_info);
	strcpy(n_info[n_index].cnip, cip);
	strcpy(n_info[n_index].nbip, nip);
	n_info[n_index].cn = c;
	(n_info[n_index].time).tv_sec = t.tv_sec;
	(n_info[n_index].time).tv_usec = t.tv_usec;
	n_info[n_index].state = s;
	//fprintf(stderr, "after insert:\n");
	//p_n(0);
	//fprintf(stderr, "-------------------------------------------------------------------\n");
	return n_index;
}

// End of added by zwy


long NS_CLASS hello_jitter()
{
    if (hello_jittering) {
#ifdef NS_PORT
	return (long) (((float) Random::integer(RAND_MAX + 1) / RAND_MAX - 0.5)
		       * JITTER_INTERVAL);
#else
	return (long) (((float) random() / RAND_MAX - 0.5) * JITTER_INTERVAL);
#endif
    } else
	return 0;
}

void NS_CLASS hello_start()
{
    if (hello_timer.used)
	return;

    gettimeofday(&this_host.fwd_time, NULL);

    DEBUG(LOG_DEBUG, 0, "Starting to send HELLOs!");
    timer_init(&hello_timer, &NS_CLASS hello_send, NULL);
    timer_set_timeout(&hello_timer,HELLO_INTERVAL/10);
    //hello_send(NULL);
}
/*
void NS_CLASS hello_stop()
{
    DEBUG(LOG_DEBUG, 0,
	  "No active forwarding routes - stopped sending HELLOs!");
    timer_remove(&hello_timer);
}
*/
void NS_CLASS hello_send(void *arg)
{

	fprintf(stderr, "WE ARE IN HELLO_SEND!!!!!\n");
	//Updated 2018/12/22 by buaa16061132
	//RREP *rrep;
	HELLO* hello;
	//end update

    RREP *rrep;
    AODV_ext *ext = NULL;
    u_int8_t flags = 0;
    struct in_addr dest;
    long time_diff, jitter;
    struct timeval now;
    int msg_size = RREP_SIZE;
    int i;
//Updated 2018/12/22 by buaa16061132
	int channel = -1;  /* -1 indicates broadcasting */
	//end update
	//added by zwy
	int l,r;
	int sum_0 = 0;
	int sum_1 = 0;
	int sum_2 = 0;
	float ncr;
	float ucn, nbr, iir;
	float result;
	u_int16_t final;
	float flag,flag0,flag1,flag2;
	double power1, power2, power0,power;
	//end of added by zwy


	gettimeofday(&now, NULL);


	//Updated 2018/12/22 by buaa16061132
	/*
    if (optimized_hellos &&
    timeval_diff(&now, &this_host.fwd_time) > ACTIVE_ROUTE_TIMEOUT) {
    hello_stop();
    return;
    }


    time_diff = timeval_diff(&now, &this_host.bcast_time);
      */
	//end update
    jitter = hello_jitter();

    /* This check will ensure we don't send unnecessary hello msgs, in case
       we have sent other bcast msgs within HELLO_INTERVAL */
	/*
    if (time_diff >= HELLO_INTERVAL) {
    */
	//end update

	for (i = 0; i < MAX_NR_INTERFACES; i++) {
	    if (!DEV_NR(i).enabled)
		continue;
#ifdef DEBUG_HELLO
	    DEBUG(LOG_DEBUG, 0, "sending Hello to 255.255.255.255");
#endif
		//add by zwy
		// 统计三种状态的项目的个数
		//fprintf(stderr, "curent ip(DEV_NR(i).ipaddr) is %s\n", ip_to_str(DEV_NR(i).ipaddr));
		//fprintf(stderr, "n_info address in hello_send sum calc:%d\n",&n_info);
		for(l = 1; l < 20; l++){
			if(strcmp(n_info[l].cnip, ip_to_str(DEV_NR(i).ipaddr)) == 0 && n_info[l].state == 0){
				sum_0++; //fprintf(stderr, "neighbor ip: %s\n", n_info[l].nbip);
			}
			if(strcmp(n_info[l].cnip, ip_to_str(DEV_NR(i).ipaddr)) == 0 && n_info[l].state == 1){
				sum_1++; //fprintf(stderr, "neighbor ip: %s\n", n_info[l].nbip);
			}
			if(strcmp(n_info[l].cnip, ip_to_str(DEV_NR(i).ipaddr)) == 0 && n_info[l].state == 2){
				sum_2++;
			}
		}
		for(l=0;l<20;l++){
			//fprintf(stderr,"---------------------------hello send  cnip:%s | nbip:%s | state:%d\n",n_info[l].cnip,n_info[l].nbip,n_info[l].state);
		}
		fprintf(stderr, ">>>[%s]:sum_0=%d ", ip_to_str(DEV_NR(i).ipaddr), sum_0);
		fprintf(stderr, "sum_1=%d ", sum_1);
		fprintf(stderr, "sum_2=%d ", sum_2);

		// 计算邻居节点变化，calculate neighbor change rate
		sum_0 + sum_1 + sum_2 == 0 ? (ncr = -1) : (ncr = sum_0 / (sum_1 + sum_2 + sum_0));
		fprintf(stderr, "| ncr = %.2lf ", ncr);
		// 计算可用信道数，calculate usable channels number
		ucn = sum_0 + sum_1; /*可用信道数*/
		// 归一化
		(ucn / 10 > 1 ) ? (ucn = 1) : (ucn = ucn / 10);
		(ucn == 0) ? (ucn = -1) : (ucn = ucn);

		fprintf(stderr, "| ucn = %.2lf", ucn);
		// 计算信号干扰，calculate neighbor break rate

		flag0 = maclist[0]->getNoiseFlag();
		flag1 = maclist[1]->getNoiseFlag();
		flag2 = maclist[2]->getNoiseFlag();
        fprintf(stderr,"maclist[0]= %d |" ,maclist[0]);
        fprintf(stderr,"maclist[1]= %d |" ,maclist[1]);
        fprintf(stderr,"maclist[2]= %d \n" ,maclist[2]);


        fprintf(stderr, "| flag0 = %.2f", flag0);
		fprintf(stderr, "| flag1 = %.2f", flag1);
		fprintf(stderr, "| flag2 = %.2f", flag2);
		flag = maclist[0]->getNoiseFlag()+maclist[1]->getNoiseFlag()+maclist[2]->getNoiseFlag();
		fprintf(stderr, "| flag_sum = %.2f", flag);
		flag = (3.0 - flag)/3.0;
		fprintf(stderr, "| flag = %.2f", flag);
		// 计算流间干扰，calculate interflow interference rate
		power0 = maclist[0]->getNoisePower()*1.00e10;
		fprintf(stderr, "| power0 = %.2lf", power0);
		power1 = maclist[1]->getNoisePower()*1.00e10;
		fprintf(stderr, "| power1 = %.2lf", power1);
		power2 = maclist[2]->getNoisePower()*1.00e10;
		fprintf(stderr, "| power2 = %.2lf", power2);
		power = (power0*power0+power1*power1+power2*power2)/((power0+power1+power2)*(power0+power1+power2)+1.0);
		fprintf(stderr, "| power = %.2lf", power);

		final = svm_predict_main(ncr,ucn, flag, power);	// svm predict'
		//final = 1;

		fprintf(stderr, "| final = %d\n", final);

		strcpy(n_info[0].cnip, ip_to_str(DEV_NR(i).ipaddr));    // this two lines copy itself to the first entry
		strcpy(n_info[0].nbip, ip_to_str(DEV_NR(i).ipaddr));

		//end of added by zwy

		/*
               rrep = rrep_create(flags, 0, 0, DEV_NR(i).ipaddr,
                          this_host.seqno,
                          DEV_NR(i).ipaddr,
                          ALLOWED_HELLO_LOSS * HELLO_INTERVAL,0);// added by 16231053
               */
		//Updated 2018/12/22 by buaa16061132
		/*
        rrep = rrep_create(flags, 0, 0, DEV_NR(i).ipaddr,
                   this_host.seqno,
                   DEV_NR(i).ipaddr,
                   ALLOWED_HELLO_LOSS * HELLO_INTERVAL);
        */
		hello = hello_create(DEV_NR(i).ipaddr, this_host.seqno, channel);
		//end update

/*	       rrep = rrep_create(flags, 0, 0, DEV_NR(i).ipaddr,
			       this_host.seqno,
			       DEV_NR(i).ipaddr,
			       ALLOWED_HELLO_LOSS * HELLO_INTERVAL,0);//16231053+++++++++++++++++++++++++++++++++++
*/
	    /* Assemble a RREP extension which contain our neighbor set... */

		//added by zwy
		char current[20];
		strcpy(current, ip_to_str(DEV_NR(i).ipaddr));

		//end of added by zwy
/*
	    if (unidir_hack) {
		int i;

		if (ext)
		    ext = AODV_EXT_NEXT(ext);
		else
		    ext = (AODV_ext *) ((char *) rrep + RREP_SIZE);

		ext->type = RREP_HELLO_NEIGHBOR_SET_EXT;
		ext->length = 0;

		for (i = 0; i < RT_TABLESIZE; i++) {
		    list_t *pos;
		    list_foreach(pos, &rt_tbl.tbl[i]) {
			rt_table_t *rt = (rt_table_t *) pos;


			if (rt->hello_timer.used) {
#ifdef DEBUG_HELLO
			    DEBUG(LOG_INFO, 0,
				  "Adding %s to hello neighbor set ext",
				  ip_to_str(rt->dest_addr));
#endif
			    memcpy(AODV_EXT_DATA(ext), &rt->dest_addr,
				   sizeof(struct in_addr));
			    ext->length += sizeof(struct in_addr);
			}
		    }
		}
		if (ext->length)
		    msg_size = RREP_SIZE + AODV_EXT_SIZE(ext);
	    }
*/	    dest.s_addr = AODV_BROADCAST;
		//Updated 2018/12/22 by buaa16061132
		int is_host_find = hello_find_host(DEV_NR(i).ipaddr.s_addr);
		if(is_host_find < 0){
			hello_create_host(DEV_NR(i).ipaddr.s_addr);
			int stability = 1;
			hello_update_host(hello_host_no - 1, stability);
		}
		else{
			int stability = 1;
			hello_update_host(is_host_find, stability);
		}
		fprintf(stderr, "Send HELLO package to 255.255.255.255 in channel %d by host %d", hello->origin_addr, hello->channel);
		//Updated end

		aodv_socket_send((AODV_msg *) hello, dest, msg_size, 1, &DEV_NR(i));
	}

	timer_set_timeout(&hello_timer, HELLO_INTERVAL + jitter);
}
//Updated 2018/12/22 by buaa16061132
/*
} else {
if (HELLO_INTERVAL - time_diff + jitter < 0)
    timer_set_timeout(&hello_timer,
              HELLO_INTERVAL - time_diff - jitter);
else
    timer_set_timeout(&hello_timer,
              HELLO_INTERVAL - time_diff + jitter);
}
*/
//end update



/* Process a hello message */
void NS_CLASS hello_process(/* Updated 2018/12/18 by buaa16061132 */HELLO* hello/* end update */, int rreplen, unsigned int ifindex)
{
    u_int32_t hello_seqno, timeout, hello_interval = HELLO_INTERVAL;
    u_int8_t state, flags = 0;
    struct in_addr ext_neighbor, hello_dest;
    rt_table_t *rt;
    AODV_ext *ext = NULL;
    int i;
    struct timeval now;
	int t;

	/*added for bold move by zwy*/
	int find_result;
	char str1[20],str2[20];


//16231053+++++++++++++++++++++++++++++++++++++++
    //u_int32_t rreq_Cost,rreq_Channel;
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    gettimeofday(&now, NULL);
	//Updated 2018/12/22 by buaa16061132
	hello_dest.s_addr = hello->origin_addr;
	//end update
	hello_seqno = ntohl(hello->dest_seqno);
  //  hello_dest.s_addr = hello->dest_addr;
  //  hello_seqno = ntohl(hello->dest_seqno);
//16231053++++++++++++++++++++++++++++++++++++++++++++++
    //rreq_Cost = Func_La(hello_dest,DEV_IFINDEX(ifindex).ipaddr);
    //rreq_Channel = Func_Cha(hello_dest,DEV_IFINDEX(ifindex).ipaddr);
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    rt = rt_table_find(hello_dest);

    if (rt)
	flags = rt->flags;
/*
    if (unidir_hack)
	flags |= RT_UNIDIR;


    ext = (AODV_ext *) ((char *) hello + RREP_SIZE);

    while (rreplen > (int) RREP_SIZE) {
	switch (ext->type) {
	case RREP_HELLO_INTERVAL_EXT:
	    if (ext->length == 4) {
		memcpy(&hello_interval, AODV_EXT_DATA(ext), 4);
		hello_interval = ntohl(hello_interval);
#ifdef DEBUG_HELLO
		DEBUG(LOG_INFO, 0, "Hello extension interval=%lu!",
		      hello_interval);
#endif

	    } else
		alog(LOG_WARNING, 0,
		     __FUNCTION__, "Bad hello interval extension!");
	    break;
	case RREP_HELLO_NEIGHBOR_SET_EXT:

#ifdef DEBUG_HELLO
	    DEBUG(LOG_INFO, 0, "RREP_HELLO_NEIGHBOR_SET_EXT");
#endif
	    for (i = 0; i < ext->length; i = i + 4) {
		ext_neighbor.s_addr =
		    *(in_addr_t *) ((char *) AODV_EXT_DATA(ext) + i);

		if (ext_neighbor.s_addr == DEV_IFINDEX(ifindex).ipaddr.s_addr)
		    flags &= ~RT_UNIDIR;
	    }
	    break;
	default:
	    alog(LOG_WARNING, 0, __FUNCTION__,
		 "Bad extension!! type=%d, length=%d", ext->type, ext->length);
	    ext = NULL;
	    break;
	}
	if (ext == NULL)
	    break;

	rreplen -= AODV_EXT_SIZE(ext);
	ext = AODV_EXT_NEXT(ext);
    }
*/
#ifdef DEBUG_HELLO
    DEBUG(LOG_DEBUG, 0, "rcvd HELLO from %s, seqno %lu",
	  ip_to_str(hello_dest), hello_seqno);
#endif
    /* This neighbor should only be valid after receiving 3
       consecutive hello messages... */
/*    if (receive_n_hellos)
	state = INVALID;
    else
	state = VALID;

    timeout = ALLOWED_HELLO_LOSS * hello_interval + ROUTE_TIMEOUT_SLACK;
*/
	// added by zwy
	//fprintf(stderr, "ifindex in hello_process:%d\n",ifindex);
	if (receive_n_hellos){
		state = INVALID;
		// 邻居节点状态变为不可用，更新结构体
		// 所有有这个ip地址的项状态都设置为2
		for(t = 0; t < 20; t++){
			if(strcmp(n_info[t].cnip, ip_to_str(hello_dest)) == 0
			   || strcmp(n_info[t].nbip, ip_to_str(hello_dest)) == 0){
				// 更新计时器时间
				gettimeofday(&now, NULL);
				(n_info[t].time).tv_sec = now.tv_sec;
				(n_info[t].time).tv_usec = now.tv_usec;
				// 更新状态
				n_info[t].state = 2;
			}
		}
		/*added by zwy for debug*/
		for(t = 0; t < 20; t++){
			fprintf(stderr,"cnip:%s | nbip:%s | hello_dest: %s | state:%d\n",n_info[t].cnip,n_info[t].nbip,ip_to_str(hello_dest),n_info[t].state);
		}
		/*end of debug fprintf*/
	}

	else{
		state = VALID;
		//fprintf(stderr,"addr of n_info in hello process:%d\n",&n_info);
		/*a bold move by zwy*/
		memset(str1,0,sizeof(char)*20);
		memset(str2,0,sizeof(char)*20);
		//find_result = find_n(num_ip_to_str(hello->orig_addr,str1), num_ip_to_str(hello->dest_addr,str2), ifindex);
		//fprintf(stderr,"DEV_IFINDEX(ifindex).ipaddr: %d, hello->dest_addr: %d\n",DEV_IFINDEX(ifindex).ipaddr,hello->dest_addr);
		//find_result = find_n(ip_to_str(DEV_IFINDEX(ifindex).ipaddr), num_ip_to_str(hello->dest_addr,str2), ifindex);
		find_result = find_n(ip_to_str(DEV_IFINDEX(ifindex).ipaddr), ip_to_str(hello_dest), ifindex);
		memset(str1,0,sizeof(char)*20);
		memset(str2,0,sizeof(char)*20);
		//fprintf(stderr, "find_result in hello_process:%d\n",find_result);
		if(find_result!=-1){
			//fprintf(stderr, "before update_n\n");
			update_n(find_result);
			//fprintf(stderr, "after update_n\n");
		}
		else{
			//fprintf(stderr, "before insert_n\n");
			//insert_n(ip_to_str(DEV_IFINDEX(ifindex).ipaddr), num_ip_to_str(hello->dest_addr,str2),ifindex, now, 1);
			insert_n(ip_to_str(DEV_IFINDEX(ifindex).ipaddr), ip_to_str(hello_dest),ifindex, now, 1);
			memset(str1,0,sizeof(char)*20);
			memset(str2,0,sizeof(char)*20);
			//fprintf(stderr, "after insert_n\n");
		}
		for(t = 0; t < 20; t++){
			//fprintf(stderr,"***********hello process*************cnip:%s | nbip:%s | hello_dest: %s | state:%d\n",n_info[t].cnip,n_info[t].nbip,ip_to_str(hello_dest),n_info[t].state);
		}
		/*end of bold move*/
	}
	//end of added by zwy

	timeout = ALLOWED_HELLO_LOSS * hello_interval + ROUTE_TIMEOUT_SLACK;



	if (!rt) {
	/* No active or expired route in the routing table. So we add a
	   new entry... */

	rt = rt_table_insert(hello_dest, hello_dest, 1,
			     hello_seqno, timeout, state, flags, ifindex,-1,-1);//16231053++++++++++++++++++++++++++++

	if (flags & RT_UNIDIR) {
	    DEBUG(LOG_INFO, 0, "%s new NEIGHBOR, link UNI-DIR",
		  ip_to_str(rt->dest_addr));
	} else {
	    DEBUG(LOG_INFO, 0, "%s new NEIGHBOR!", ip_to_str(rt->dest_addr));
	}
	rt->hello_cnt = 1;

    } else {

	if ((flags & RT_UNIDIR) && rt->state == VALID && rt->hcnt > 1) {
	    goto hello_update;
	}

	if (receive_n_hellos && rt->hello_cnt < (receive_n_hellos - 1)) {
	    if (timeval_diff(&now, &rt->last_hello_time) <
		(long) (hello_interval + hello_interval / 2))
		rt->hello_cnt++;
	    else
		rt->hello_cnt = 1;

	    memcpy(&rt->last_hello_time, &now, sizeof(struct timeval));
	    return;
	}
	rt_table_update(rt, hello_dest, 1, hello_seqno, timeout, VALID, flags,-1,-1);//16231053+++++++++++++++++++++++++++++++++++++
    }

  hello_update:

	//Updated 2018/12/22 by buaa16061132
	//hello_update_timeout(rt, &now, ALLOWED_HELLO_LOSS * hello_interval);
	//end update

	//Updated 2018/12/22 by buaa16061132
	u_int8_t channel = hello->channel;
	u_int8_t stability = 1;
	struct in_addr hello_ack_origin_addr;
	struct in_addr hello_ack_dest_addr;
	hello_ack_dest_addr.s_addr = hello->origin_addr;
//	hello_ack_origin_addr.s_addr = DEV_IFINDEX(ifindex).ipaddr.s_addr;

	HELLO_ACK* hello_ack;

	for (i = 0; i < MAX_NR_INTERFACES; i++) {
		if (!DEV_NR(i).enabled)
			continue;
        hello_ack_origin_addr.s_addr = DEV_NR(i).ipaddr.s_addr;
		hello_ack = hello_ack_create(hello_ack_dest_addr, hello_ack_origin_addr, channel, stability);
		struct in_addr dest;
		dest.s_addr = hello_ack_dest_addr.s_addr;
		int msg_size = HELLO_ACK_SIZE;
		fprintf(stderr, "Send HELLO_ACK package to %d in channel %d by host %d",
				hello_ack->dest_addr, hello_ack->channel, hello_ack->origin_addr);
		aodv_socket_send((AODV_msg *) hello_ack, dest, msg_size, 1, &DEV_NR(i));
	}
	//end update
	return;
}

//Updated 2018/12/22 by buaa16061132
void NS_CLASS hello_ack_process(HELLO_ACK* hello_ack, int hello_ack_len, unsigned int ifindex){
	u_int8_t channel = hello_ack->channel;
	u_int8_t neighbor_stability = hello_ack->stability;
	u_int32_t origin_addr = hello_ack->dest_addr;  //The address of this host
	u_int32_t neighbor_addr = hello_ack->origin_addr;  //The address of the neighbor of this host

	int i, j;

	i = hello_find_host(origin_addr);
	j = hello_find_neighbor(i, neighbor_addr);
	if(j < 0){
		hello_create_neighbor(i, neighbor_addr);
		hello_update_neighbor(i, (hello_host_data[i].neighbor_no - 1), neighbor_stability, channel);
	}
	else{
		hello_update_neighbor(i, j, neighbor_stability, channel);
	}
}
//end update

#define HELLO_DELAY 50		/* The extra time we should allow an hello
				   message to take (due to processing) before
				   assuming lost . */

NS_INLINE void NS_CLASS hello_update_timeout(rt_table_t * rt,
					     struct timeval *now, long time)
{
    timer_set_timeout(&rt->hello_timer, time + HELLO_DELAY);
    memcpy(&rt->last_hello_time, now, sizeof(struct timeval));
}
