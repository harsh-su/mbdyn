/*
COPYRIGHT (C) 2003  Michele Attolico (attolico@aero.polimi.it)

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
*/

#undef VERBOSE

#define S_FUNCTION_NAME		sfun_mbdyn_com_read
#define S_FUNCTION_LEVEL	2

#ifdef MATLAB_MEX_FILE
#include "mex.h"
#endif
#include "simstruc.h"
		
#define NUMBER_OF_PARAMS	(8)/*da decidere*/

#define HOST_NAME_PARAM		ssGetSFcnParam(S,0)
#define MBD_NAME_PARAM		ssGetSFcnParam(S,1)
#define MBX_NAME_PARAM		ssGetSFcnParam(S,2)
#define MBX_N_CHN_PARAM		ssGetSFcnParam(S,3)
#define SAMPLE_TIME_PARAM	ssGetSFcnParam(S,4)
/*socket stuff*/

#define NET_PARAM		ssGetSFcnParam(S,5)
#define PORT_PARAM		ssGetSFcnParam(S,6)
#define PATH_PARAM		(const char *)ssGetSFcnParam(S,7)


#define WAIT_LOOP  (10000)

#define MBX_N_CHN		((uint_T) mxGetPr(MBX_N_CHN_PARAM)[0])
#define SAMPLE_TIME		((real_T) mxGetPr(SAMPLE_TIME_PARAM)[0])
/*socket stuff*/
#define NET			((uint_T) mxGetPr(NET_PARAM)[0])
#define PORT			((uint_T) mxGetPr(PORT_PARAM)[0])

#define NANO_SAMPLE_TIME	((long int)(SAMPLE_TIME*1000000000))

#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/param.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>


#ifndef MATLAB_MEX_FILE

#define KEEP_STATIC_INLINE
#include <math.h>
#include <rtai_lxrt.h>
#include <rtai_mbx.h>
#include <rtai_netrpc.h>
#include "mbdyn_rtai.h"

extern long int 	MbdynNode[];
extern unsigned long	MbdynName[];
extern bool		MbdynTaskActive[];


//volatile int mbd_read_cnt = 0;
static char 	msg = 't';
extern 	RT_TASK *rt_HostInterfaceTask;
struct mbd_read_str{
	void 	*comptr_in;
	unsigned long 	node;
	int 	port;
	double	*buffer;
	//int	buffer_name;
	int	mbdtask_count;
	int	err_count;
	int 	step_count;
};
#else

#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/poll.h>

#define UNIX_PATH_MAX    108
#define DEFAULT_PORT	5500 /*FIXME:da defineire meglio*/
#define SYSTEM_PORT	1000 /*FIXME:da defineire meglio*/
#define DEFAULT_HOST 	"127.0.0.1"
#define TIME_OUT	10000 /*milliseconds*/

#endif /*MATLAB_MEX_FILE*/

#define MDL_CHECK_PARAMETERS
#if defined(MDL_CHECK_PARAMETERS) && defined(MATLAB_MEX_FILE)
static void mdlCheckParameters(SimStruct *S)
{
	static char_T errMsg[256];
	if (mxGetNumberOfElements(MBX_N_CHN_PARAM) != 1) {
		sprintf(errMsg, "Channel parameter must be a scalar.\n");
		ssSetErrorStatus(S, errMsg);
	}
	 
	if (mxGetNumberOfElements(SAMPLE_TIME_PARAM) != 1) {
		sprintf(errMsg, "Sample time parameter must be a scalar\n");
		ssSetErrorStatus(S, errMsg);
	}
	if (mxGetNumberOfElements(PORT_PARAM) != 1) {
		sprintf(errMsg, "Port parameter must be a scalar\n");
		ssSetErrorStatus(S, errMsg);
	}
}
#endif /*MDL_CHECK_PARAMETERS && MATLAB_MEX_FILE*/

static void mdlInitializeSizes(SimStruct *S)
{
	uint_T i;

	ssSetNumSFcnParams(S, NUMBER_OF_PARAMS);
#if defined(MATLAB_MEX_FILE)
	if (ssGetNumSFcnParams(S) == ssGetSFcnParamsCount(S)) {
		mdlCheckParameters(S);
		if (ssGetErrorStatus(S) != NULL) {
			return;
		}
	} else {
		return;
	}
#endif /*MATLAB_MEX_FILE*/
	for (i = 0; i < NUMBER_OF_PARAMS; i++) {
		ssSetSFcnParamNotTunable(S, i);
	}
	ssSetNumInputPorts(S, 0);
	ssSetNumOutputPorts(S, MBX_N_CHN);
	for (i = 0; i < MBX_N_CHN; i++) {
		ssSetOutputPortWidth(S, i, 1);
	}
	
	
	
	ssSetNumContStates(S, 0);
	ssSetNumDiscStates(S, 0);
	ssSetNumSampleTimes(S, 1);
	ssSetNumPWork(S,1);
#ifdef MATLAB_MEX_FILE
	ssSetNumIWork(S,2);
#endif /*MATLAB_MEX_FILE*/
}

static void mdlInitializeSampleTimes(SimStruct *S)
{
	ssSetSampleTime(S, 0, SAMPLE_TIME);
	ssSetOffsetTime(S, 0, 0.0);
}

#define MDL_START 
#if defined(MDL_START)
static void mdlStart(SimStruct *S)
{
#ifndef MATLAB_MEX_FILE
	struct mbd_read_str *ptrstr=NULL;
	
	void	*mbdtask;
	//void *comptr_in;
	static char_T errMsg[256];
	register int i;
	char mbx_name[7], mbd_name[7];

	struct in_addr addr;
	char host_name[MAXHOSTNAMELEN];

	int timerflag = 0, count;
		
	mxGetString(MBD_NAME_PARAM, mbd_name, 7);
	mxGetString(MBX_NAME_PARAM, mbx_name, 7);

	/******************************************************************
	*alloc struct 
	******************************************************************/
	
/*	for (i = mbd_read_cnt; i < 999; i++) {
		sprintf(myname, "com%d",i);
		if (!rt_get_adr(nam2num(myname))) break;
	}*/
	if (!(ptrstr = (struct mbd_read_str *)malloc(sizeof(struct mbd_read_str)))){
		sprintf(errMsg, "\nCANNOT ALLOC MEMORY OF STRUCTURE\n");
		ssSetErrorStatus(S, errMsg);
		printf("%s",errMsg);
	}
	ssGetPWork(S)[0] = ptrstr;
	/*ssGetIWork(S)[0] = nam2num(myname);*/
		
			
	/******************************************************************
	converting host_name into node
	******************************************************************/
	mxGetString(HOST_NAME_PARAM, host_name, MAXHOSTNAMELEN);
	
printf("\nread requesting hard_port \n");
	inet_aton(host_name, &addr);

	if(((ptrstr -> node) = addr.s_addr)){/*node*/
		if (!(ptrstr -> port = rt_request_hard_port(ptrstr -> node))){
			ssSetErrorStatus(S, errMsg);
			printf("%s",errMsg);
			return;
		}

printf("\nread ()GOT hard_port %ld\n", ptrstr -> port);
	}
	else{
		ptrstr -> port = 0;
	};


	if (!rt_is_hard_timer_running()){ /*start real-time timer*/
		printf("start timer\n");
		rt_set_oneshot_mode();
		start_rt_timer(0);
		timerflag = 1;
	}

	/******************************************************************
	*find mbdyn task
	******************************************************************/
	count = 0;
printf("\nread: nam2num(mbd_name) %x\n", nam2num(mbd_name));
	while(count < MAX_MBDYN_TASK){
		if ((MbdynNode[count] == ptrstr -> node) &&
		    (MbdynName[count] == nam2num(mbd_name)))
		{
printf("\nread: RT_get MBDYN_task addy node: %x, port %d\n",ptrstr -> node,ptrstr -> port);
			mbdtask = (void *)RT_get_adr(ptrstr -> node, 
							ptrstr -> port,mbd_name);
printf("\nread: RT_GOT MBDYN_task addy!\n");
			ptrstr -> mbdtask_count = count;
			break;
			
		}
		if ((MbdynNode[count] == 1) &&(MbdynName[count] == 0xFFFFFFFF)){
			i = WAIT_LOOP;
			printf("\nread: Connecting to MBDYN task!\n");
	
			while (!(mbdtask = (void *)RT_get_adr(ptrstr -> node, 
									ptrstr -> port,mbd_name))) {
	
				if (!i){
					sprintf(errMsg, "\nCANNOT FIND MBDyn TASK\n");
					ssSetErrorStatus(S, errMsg);
					printf("%s",errMsg);
					return;
				}
				rt_sleep(nano2count(1000000));
				i--;
			}
			
			MbdynNode[count] = ptrstr -> node;
			MbdynName[count] = nam2num(mbd_name);
			MbdynTaskActive[count] = true;
			ptrstr -> mbdtask_count = count;
			break;

		}

		count++;
	}
	
	if (count == MAX_MBDYN_TASK){
		printf("\nToo many mbdyn_task!\n");
		ssSetErrorStatus(S, errMsg);
		printf("%s",errMsg);
		return;
	}
	
	
	/******************************************************************
	*find mailbox 
	******************************************************************/
	
	i = WAIT_LOOP;

	while (!(ptrstr -> comptr_in = (void *)RT_get_adr(ptrstr -> node,
							  ptrstr -> port, mbx_name))) { 
		if (!i){
			
			sprintf(errMsg, "\nCANNOT FIND INPUT MAILBOX: %s\n",mbx_name);
			ssSetErrorStatus(S, errMsg);
			printf("%s",errMsg);
			//rt_send(rt_HostInterfaceTask, (int)msg);
			return;
		}
		rt_sleep(nano2count(1000000));
		i--;
	}
	

	
	/******************************************************************
	*alloc buffer memory 
	******************************************************************/
	/*i = mbd_read_cnt;
	
	for (i = mbd_read_cnt; i < 999; i++) {
		sprintf(myname, "buf%d",i);
		if (!rt_get_adr(nam2num(myname))) break;
	}
	mbd_read_cnt = i;*/
	
	if (!(ptrstr -> buffer = malloc(sizeof(double)*MBX_N_CHN))){
		sprintf(errMsg, "\nCANNOT ALLOC BUFFER MEMORY:\n");
		ssSetErrorStatus(S, errMsg);
		printf("%s",errMsg);
		return;
	}
	/*ptrstr -> buffer_name = nam2num(myname);*/	
	
	ptrstr -> err_count = 0;
	ptrstr -> step_count = 0;
	
	printf("\nsfun_mbdyn_com_read:\n\n");
	/*
	printf("MbdynNode:%ld\n",MbdynNode[ptrstr -> mbdtask_count]);
	printf("MbdynName:%ld\n",MbdynName[ptrstr -> mbdtask_count]);
	printf("MbdynTaskActive:%d\n\n",MbdynTaskActive[ptrstr -> mbdtask_count]);
	*/
	printf("Host name:%s\n",host_name);
	printf("node: %lx\n",ptrstr -> node);
	printf("port: %d\n",ptrstr -> port);
	printf("mbdtask: %p\n",mbdtask);
	printf("mbx name:%s\n",mbx_name);
	printf("mbx: %p\n",ptrstr -> comptr_in);
	printf("mbx channel:%d\n",MBX_N_CHN);
	printf("NANO_SAMPLE_TIME:%ld\n",NANO_SAMPLE_TIME);


	if (timerflag){
		stop_rt_timer();
	}
#else
	/*inzilizza la socket*/
	int sock = 0;
	int conn = 0;
	/*salva la socket*/
	ssGetIWork(S)[0] = (int_T)sock;
	/*imposta conn a 0*/
	ssGetIWork(S)[1] = (int_T)conn;
#endif /*MATLAB_MEX_FILE*/
}
#endif /*MDL_START*/

static void mdlOutputs(SimStruct *S, int_T tid)
{
#ifndef MATLAB_MEX_FILE
	struct mbd_read_str *ptrstr= (struct mbd_read_str *)ssGetPWork(S)[0];

	register int i;
	double 	y[MBX_N_CHN];
	int flag;

	if((ptrstr -> step_count) < 2){//to synchronize
		flag = RT_mbx_receive_timed(ptrstr -> node,ptrstr -> port,
			ptrstr -> comptr_in, y,
			sizeof(double)*MBX_N_CHN, NANO_SAMPLE_TIME);
		ptrstr -> step_count++;
	} else {
		flag = RT_mbx_receive_if(ptrstr -> node,ptrstr -> port,
			ptrstr -> comptr_in, y,
			sizeof(double)*MBX_N_CHN);
	}
	


	if (flag == 0){
		for (i=0; i<MBX_N_CHN; i++){
			ssGetOutputPortRealSignal(S,i)[0] = (ptrstr -> buffer)[i] = y[i];
		}
 	} else if (flag == sizeof(double)*MBX_N_CHN){
		for (i=0; i<MBX_N_CHN; i++){
			ssGetOutputPortRealSignal(S,i)[0] = (ptrstr -> buffer)[i];
		}
		ptrstr -> err_count++;
	} else {
		MbdynTaskActive[ptrstr -> mbdtask_count] = false;
		rt_send(rt_HostInterfaceTask, (int)msg);
	}
#else
	register int i;
	double y[MBX_N_CHN];
	int sock = ssGetIWork(S)[0];
	int conn = ssGetIWork(S)[1];
	static char_T errMsg[256];
	if(conn == 0) {
		if (sock == 0) {
			if(NET) {
				/*usa il protocollo tcp/ip*/
				struct sockaddr_in addr;
				char host[MAXHOSTNAMELEN];
				int flags;
				mxGetString(HOST_NAME_PARAM, host, MAXHOSTNAMELEN);
		
				/*crea una socket*/
				sock = socket(PF_INET, SOCK_STREAM, 0);
				if (sock < 0) {
					sprintf(errMsg, "\nCANNOT CREATE A INET SOCKET\n");
					ssSetErrorStatus(S, errMsg);
					printf("%s",errMsg);
					return;	
				}

				addr.sin_family = AF_INET;
				if(PORT == 0) {
					addr.sin_port = htons (DEFAULT_PORT);	
				} else {
					addr.sin_port = htons (PORT);
				}
				if (inet_aton(host, &(addr.sin_addr)) == 0) {
					sprintf(errMsg, "\nUNKNOW HOST\n");
					ssSetErrorStatus(S, errMsg);
					printf("%s",errMsg);
					return;
				}
				
				/*imposta la socket come non bloccante*/
        			flags = fcntl(sock, F_GETFL, 0);
				if (flags == -1) {
					sprintf(errMsg, "\nREAD sfunction: unable to get socket flags\n");
					ssSetErrorStatus(S, errMsg);
					printf("%s",errMsg);
					return;				
				}
				flags |= O_NONBLOCK;
				if (fcntl(sock, F_SETFL, flags) == -1){
					sprintf(errMsg, "\nREAD sfunction: unable to set socket flags\n");
					ssSetErrorStatus(S, errMsg);
					printf("%s",errMsg);
					return;								
				}
							
				
				/*connect*/
				if (connect(sock,(struct sockaddr *) &addr, sizeof (addr)) != -1) {
					conn = 1;
					/*reimposta la socket come bloccante*/
					flags &= (~O_NONBLOCK);
					if (fcntl(sock, F_SETFL, flags) == -1){
						sprintf(errMsg, "\nREAD sfunction: unable to set socket flags\n");
						ssSetErrorStatus(S, errMsg);
						printf("%s",errMsg);
						return;								
					}
				}
			} else {
				/*usa le socket local*/
				struct sockaddr_un addr;
				char path[UNIX_PATH_MAX];
				int flags;
				/*crea una socket*/
				sock = socket(PF_LOCAL, SOCK_STREAM, 0);
				if (sock < 0) {
					sprintf(errMsg, "\nCANNOT CREATE A LOCAL SOCKET\n");
					ssSetErrorStatus(S, errMsg);
					printf("%s",errMsg);
					return;	
				}
				addr.sun_family = AF_UNIX;
				strncpy(path, PATH_PARAM, UNIX_PATH_MAX);
								
				/*imposta la socket come non bloccante*/
	        		flags = fcntl(sock, F_GETFL, 0);
	        		if (flags == -1) {
					sprintf(errMsg, "\nREAD sfunction: unable to get socket flags\n");
					ssSetErrorStatus(S, errMsg);
					printf("%s",errMsg);
					return;				
				}
				flags |= O_NONBLOCK;
				if (fcntl(sock, F_SETFL, flags) == -1){
					sprintf(errMsg, "\nREAD sfunction: unable to set socket flags\n");
					ssSetErrorStatus(S, errMsg);
					printf("%s",errMsg);
					return;								
				}
				
				/*connect*/
				if (connect(sock,(struct sockaddr *) &addr, sizeof (addr)) != -1) {
					conn == 1;
					/*reimposta la socket come bloccante*/
					flags &= (~O_NONBLOCK);
					if (fcntl(sock, F_SETFL, flags) == -1){
						sprintf(errMsg, "\nREAD sfunction: unable to set socket flags\n");
						ssSetErrorStatus(S, errMsg);
						printf("%s",errMsg);
						return;								
					}	
				}
			} 
			/*salva la socket*/
			ssGetIWork(S)[0] = (int_T)sock;
		} else {
			/*poll*/
			struct pollfd	ufds;
			
			ufds.fd = sock;
			ufds.events = (POLLIN | POLLOUT);
			switch (poll(&ufds ,1 , TIME_OUT)) {
			case -1:
				sprintf(errMsg, "\nREAD sfunction: POLL error\n");
				ssSetErrorStatus(S, errMsg);
				printf("1\n");
				printf("%s",errMsg);
				return;
			case 0:
				sprintf(errMsg, "\nREAD sfunction: timeout connection reached\n");
				ssSetErrorStatus(S, errMsg);
				printf("%s",errMsg);			
				return;
			default :
				{
				int flags;
				if(ufds.revents & (POLLERR | POLLHUP | POLLNVAL)) {
					sprintf(errMsg, "\nREAD sfunction: POLL error\n");
					ssSetErrorStatus(S, errMsg);
					printf("2\n");
					printf("%s",errMsg);
					return;
				}
				conn = 1;
				/*reimposta la socket come bloccante*/
        			flags = fcntl(sock, F_GETFL, 0);
        			if (flags == -1) {
					sprintf(errMsg, "\nREAD sfunction: unable to get socket flags\n");
					ssSetErrorStatus(S, errMsg);
					printf("%s",errMsg);
					return;				
				}
				flags &= (~O_NONBLOCK);
				if (fcntl(sock, F_SETFL, flags) == -1){
					sprintf(errMsg, "\nREAD sfunction: unable to set socket flags\n");
					ssSetErrorStatus(S, errMsg);
					printf("%s",errMsg);
					return;
				}
				}							
			}
			/*imposta conn a 1*/

		} /*socket == 0*/
		ssGetIWork(S)[1] = (int_T)conn;	
	} /*conn == 0*/
	
	if (conn) {
		/*legge dalla socket*/
		if(recv(sock, y, sizeof(double)*MBX_N_CHN, 0) == -1){
			sprintf(errMsg, "\nComunication closed by host\n");
			ssSetStopRequested(S, 1);
			printf("%s",errMsg);
			return;
		}				
	
		for (i=0; i<MBX_N_CHN; i++){
			ssGetOutputPortRealSignal(S,i)[0] = y[i];
		}
	}
#endif /*MATLAB_MEX_FILE*/
}

static void mdlTerminate(SimStruct *S)
{
#ifndef MATLAB_MEX_FILE
	struct 	mbd_read_str *ptrstr= (struct mbd_read_str *)ssGetPWork(S)[0];
	void	*mbdtask;
	char 	mbd_name[7];
	printf("\nsfun_mbdyn_com_read, %s:\n",ssGetModelName(S));
	printf("Number of comunication error: %d\n\n",ptrstr -> err_count);
	
	if(ptrstr){
	/****************************************************************
	*terminate mbdyn task
	****************************************************************/
		if(MbdynTaskActive[ptrstr -> mbdtask_count]){
			mxGetString(MBD_NAME_PARAM, mbd_name, 7);
			mbdtask = (void *)RT_get_adr(ptrstr -> node, 
							ptrstr -> port, mbd_name);
			printf("MBDyn is stopped by read s-function!\n");
			RT_send_timed(ptrstr -> node, ptrstr -> port, mbdtask, 1, NANO_SAMPLE_TIME*2);
			MbdynTaskActive[ptrstr -> mbdtask_count] = false;
	
		}
	/****************************************************************
	*free buffer memory
	****************************************************************/

		if(ptrstr -> buffer){
			free(ptrstr -> buffer);
		}
	/****************************************************************
	*release port
	****************************************************************/
	
		if(ptrstr ->node){
			rt_release_port(ptrstr -> node, ptrstr -> port);
		}
	
	/****************************************************************
	*free structure memory
	****************************************************************/

		free(ptrstr);
	}
#else
#ifdef VERBOSE
	fprintf(stderr,"READ:Chiudo la socket!\n");
#endif /*VERBOSE*/
	int sock = ssGetIWork(S)[0];
	double y[MBX_N_CHN];
#ifdef VERBOSE
	fprintf(stderr,"%d\n",recv(sock, y, sizeof(double)*MBX_N_CHN, 0));
#endif /*VERBOSE*/
	/*chiude la socket*/	
	shutdown(sock,SHUT_RDWR);
#endif /*MATLAB_MEX_FILE*/
}

#ifdef  MATLAB_MEX_FILE
#include "simulink.c"
#else
#include "cg_sfun.h"
#endif
