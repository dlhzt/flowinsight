//=============================================================================
// File:          flowdebug.cpp
// Author:        Zhaojunwei
// CreateTime:    2005/04/27
// Descripiton:   Head file for debuging flow data via UDP packets.
// ----------------------------------------------------------------------------
// Version:
//         2005/04/27   1.0 First work release.
//=============================================================================

#include "flowdebug.hpp"

int main(int argc, char* argv[])
{
	const int RECVBUFF_SIZE = 4096;
	int port;
	
	if(argc > 1)
		port = atoi(argv[1]);
	else
		port = 9991;
	
	#ifdef WIN32
    WSADATA WSAData;
    (void)WSAStartup(0x0101, &WSAData);
    SOCKET  sock;
    #else
    int     sock;
    #endif

    int status;

    // Open a UDP socket
    if((sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
    {
        cerr << "error in opening socket!" << endl;
    	return sock;
    }
    
    sockaddr_in     sockLocat, socFrom;
    #if defined WIN32 || defined HPUX
    int len = sizeof(struct sockaddr_in);
    #else
    socklen_t len = sizeof(struct sockaddr_in);
    #endif// WIN32
        
    memset((char *)&sockLocat, 0, sizeof(struct sockaddr_in));
    sockLocat.sin_family 		= AF_INET;
    sockLocat.sin_addr.s_addr 	= htonl(INADDR_ANY);
    sockLocat.sin_port 			= htons(port);

    // Bind the socket
    if (bind(sock, (struct sockaddr *)&sockLocat, sizeof(struct sockaddr_in)) < 0)
    {
        cerr << "Bind socket error!" << endl;
        return sock;
    }

    // Set socket option
    uint	uBuffSize = 8192;
    if (setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (char *)&uBuffSize, sizeof(uBuffSize)) < 0)
    {
       	cerr << "Set socket option error!" << endl;
        return sock;
    }
        

    // receive data.
    auto_ptr<char> recvBuff(new char[RECVBUFF_SIZE]);
        
    DWORD		routerIP, flowVer;
    for(int i=0;i<100;i++)
    {
	    if((status = recvfrom(sock, recvBuff.get(), RECVBUFF_SIZE, 0, (sockaddr*)&socFrom, &len)) < 0)
	    {
	        continue;
	    }
	    
	    routerIP = ntohl(socFrom.sin_addr.s_addr);
	    flowVer = ntohs(*((WORD*)recvBuff.get())) & 0x7fff;
	    NetFlow5Msg* v5_flow = (NetFlow5Msg *)recvBuff.get();
	    cout << i << ":: Router[" << (int)((routerIP>>24)&0xff) << "."
	    	<< (int)((routerIP>>16)&0xff) << "."
	    	<< (int)((routerIP>>8)&0xff) << "."
	    	<< (int)(routerIP&0xff)
	    	<< "] Ver[" << flowVer 
	    	<< "] Count[" << ntohs((u_short)v5_flow->header.count) 
	    	<< "]" << endl;
    }
}
