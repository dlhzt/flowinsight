#ifndef	WIN32
#include <unistd.h>				// unix
#include <sys/socket.h>			// bsd socket stuff
#include <netinet/in.h>			// network types
#include <arpa/inet.h>			// arpa	types
#else
#pragma	warning(disable:4786)
#include <winsock.h>
#endif

#include "flowform.hpp"
#include <iostream>
#include <string>
#include <strings.h>
#include <time.h>

using namespace	std;

int	main(int argc, char* argv[])
{
	if(argc	!= 3)
	{
		cerr <<	"Usage:	" << argv[0] <<	" ipaddr flow_per_min" << endl;
		cerr << "Note: the number of flow should larger than 1800." << endl;
		return 0;
	}

    #ifdef WIN32
    WSADATA WSAData;
    (void)WSAStartup(0x0101, &WSAData);
    SOCKET  sock;
    #else
	int	sock;
	#endif
	
	struct sockaddr_in servaddr;
    #ifdef WIN32
    int len = sizeof(struct sockaddr_in);
    memset(&servaddr, 0, sizeof(servaddr));
    #else
	socklen_t len =	sizeof(struct sockaddr_in);
	bzero(&servaddr, sizeof(servaddr));	
	#endif
	servaddr.sin_family	= AF_INET; //IPv4
	servaddr.sin_port =	htons(9991); //9991¶Ë¿Ú
	#ifdef WIN32
	if((servaddr.sin_addr.S_un.S_addr = inet_addr(argv[1])) == INADDR_NONE)
	#else
	if(inet_pton(AF_INET, argv[1], &servaddr.sin_addr) != 1)
	#endif
	{
			cerr <<	"Invalid ip	address!" << endl;
			return -1;
	}

	if((sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
	{
		cerr <<	"Create	socket error!" << endl;
		return -1;
	}

	NetFlow5Msg data;
	int	dataLen	= sizeof(data);
	time_t start_t,	current_t, pre_t;
	time(&start_t);
	current_t =	start_t;
	pre_t =	start_t	- 60;
	#ifdef WIN32
	#define srand48 srand
	#define lrand48 rand
	#endif
	srand48(start_t);
		
	uchar protocol[] = {1, 6, 6, 17};
	ulong network[]	= {3396366081UL, 3402369021UL, 1030754049UL, 2216232705UL};
	ulong address[4][254];
	int	i,j;
	
	for(i=0; i<4; i++)
		for(j=0; j<254;	j++)
			address[i][j] =	network[i] + j;

	data.header.version	= htons(5);
	data.header.count =	htons(NETFLOWS_V5_PER_PAK);
	data.header.unixsecs =	htonl(start_t-7200);
	data.header.sysuptime =	htonl(0);
	
	ulong count, limit;
	count =	0;
	limit =	atol(argv[2]) /	NETFLOWS_V5_PER_PAK;
	
	time_t time1, time2;
	#ifdef WIN32
	int interval = (int)((double)1000/((double)limit/60));
	#else
	struct timespec interval;
    interval.tv_sec  = 0;
    interval.tv_nsec = (int)((double)1000000000/((double)limit/60));
	#endif

	for(;;)
	{
		time(&time1);
		for(count=0; count<limit; count++)
		{
			for(int	i=0; i<NETFLOWS_V5_PER_PAK;	i++)
			{
				data.records[i].firsttime =	0;
				data.records[i].srcaddr	= htonl(address[lrand48()%4][lrand48()%254]);
				data.records[i].dstaddr	= htonl(address[lrand48()%4][lrand48()%254]);
				data.records[i].input =	0;
				data.records[i].output = 1;
				data.records[i].packets =	htonl(lrand48()%2048 + 1);
				data.records[i].octets	= htonl(data.records[i].packets << 6);
				data.records[i].srcport	= htons(lrand48()%65536);
				data.records[i].dstport	= htons(lrand48()%1024);
				data.records[i].protocol = protocol[lrand48()%4];
				data.records[i].srcas = 0;
				data.records[i].dstas = 0;
				data.records[i].srcmask = 24;
				data.records[i].dstmask = 24;
				data.records[i].tcpflags =	16;
				data.records[i].tos	= 0;
				data.records[i].nexthop	= 0;
			}
			sendto(sock, (const char*)&data,	dataLen, 0,	(struct	sockaddr*)&servaddr, len);
			#ifdef WIN32
			Sleep(interval);
			#else
			nanosleep(&interval, NULL);
			#endif
		}
		time(&time2);
		cout << "Send " << limit*NETFLOWS_V5_PER_PAK << " records in " << (int)difftime(time2, time1) << " seconds." << endl;	
	}
	
	return 0;
}
