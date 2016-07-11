//=============================================================================
// File:          flowcollector.cpp
// Author:        Zhaojunwei
// CreateTime:    2004/08/10
// Descripiton:   Implementation file for receiving flow data via UDP packets.
// ----------------------------------------------------------------------------
// Version:
//         2004/08/10   1.0 First work release.
//         2005/04/04   1.1 Modify the sole function to class FlowCollector
//         2005/04/18   1.1 Add Netstream Version 5 format, including out and
//                          inbound flow stat.
//=============================================================================

#ifndef WIN32
#include <unistd.h>             // unix
#include <sys/socket.h>         // bsd socket stuff
#include <netinet/in.h>         // network types
#include <arpa/inet.h>          // arpa types
#else
#pragma warning(disable:4786)
#include <winsock.h>
#endif

#include <map>
#include <vector>
#include <memory>
#include <fstream>
#include <sstream>
#include <iostream>

#include "flowcollector.hpp"

using namespace std;

FlowCollector::FlowCollector(uint port, 
	Synchronizer* pSyn, 
	ThreadLog* pLog, 
	volatile bool* pbRun) : 
	m_port(port), m_pSyn(pSyn), m_pLog(pLog), m_pbRun(pbRun)
{
}

FlowCollector::~FlowCollector()
{
}

int FlowCollector::Run()
{
	const char RECV_TAG[] 		= "RECV";
	const uint RECVBUFF_SIZE	= 4096;
	
	m_pLog->info(RECV_TAG, "Enter flow collecting thread.");
        
    #ifdef WIN32
    WSADATA WSAData;
    (void)WSAStartup(0x0101, &WSAData);
    SOCKET  sock;
    #else
    int     sock;
    #endif

    int status;
    int i = 0;

    // Open a UDP socket
    if((sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
    {
        m_pLog->fatal(RECV_TAG, "Create socket error!");
        return sock;
    }
    m_pLog->info(RECV_TAG, "Create sock success.");
    
    sockaddr_in     sockLocat, socFrom;
    #if defined WIN32 || defined HPUX
    int len = sizeof(struct sockaddr_in);
    #else
    socklen_t len = sizeof(struct sockaddr_in);
    #endif// WIN32
        
    memset((char *)&sockLocat, 0, sizeof(struct sockaddr_in));
    sockLocat.sin_family 		= AF_INET;
    sockLocat.sin_addr.s_addr 	= htonl(INADDR_ANY);
    sockLocat.sin_port 			= htons(m_port);

    // Bind the socket
    if (bind(sock, (struct sockaddr *)&sockLocat, sizeof(struct sockaddr_in)) < 0)
    {
        m_pLog->fatal(RECV_TAG, "Bind socket error!");
        return sock;
    }
    m_pLog->info(RECV_TAG, "Bind socket success.");

    // Set socket option
    uint	uBuffSize = 8192;
    if (setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (char *)&uBuffSize, sizeof(uBuffSize)) < 0)
    {
       	m_pLog->fatal(RECV_TAG, "Set option to socket error!");
        return sock;
    }
        
    m_pLog->info(RECV_TAG, "Beging listening.....");

    // receive data.
    auto_ptr<char> recvBuff(new char[RECVBUFF_SIZE]);
        
    // init mask seek table.
    DWORD	 maskTab[33];
    for(i = 0; i < 33; ++i)
    {
    	maskTab[i] = 0xffffffff << (32 - i);
    }
                
    time_t 		preTime, nowTime;					// For time counting of various step
    ulong 		uSeconds, uPackets = 0;
    DWORD		routerIP;							// Store the router ip in number format
    string		strRouterIP;						// Store the router ip in char fromat
//    DWORD		flowID = 0;
    map<DWORD, ulong> routerMap;					// For stat packets of each router
    map<DWORD, ulong>::const_iterator ItRouter;		// Iterator of the router map
    ostringstream  strStream;

    time(&preTime);
    for(;*m_pbRun;)
    {
	    if((status = recvfrom(sock, recvBuff.get(), RECVBUFF_SIZE, 0, (sockaddr*)&socFrom, &len)) < 0)
	    {
	    	m_pLog->warning(RECV_TAG, "Recive data failed!");
	        continue;
	    }
	    
	    ++uPackets;
	    routerIP = ntohl(socFrom.sin_addr.s_addr);
	    ++(routerMap[routerIP]);
	    
	    time(&nowTime);
	    uSeconds = (ulong)difftime(nowTime, preTime);
	    if(uSeconds >= 60)
	    {
            strStream.str("");
            strStream << "Received " << uPackets << " flow packets in " << uSeconds << " seconds.";
            m_pLog->debug(RECV_TAG, strStream.str().c_str());
            for(ItRouter = routerMap.begin(); ItRouter != routerMap.end(); ItRouter++)
            {
            	strStream.str("");
            	strStream << (uint)((ItRouter->first >> 24) & 0xff) << "." 
            		<< (uint)((ItRouter->first >> 16) & 0xff) << "." 
            		<< (uint)((ItRouter->first >> 8) & 0xff) << "." 
            		<< (uint)(ItRouter->first & 0xff);
            	strRouterIP = strStream.str();
            	strStream.str("");
                strStream << "From " << strRouterIP << " got [" << ItRouter->second << "] flow packets";
                m_pLog->debug(RECV_TAG, strStream.str().c_str());
            }
            uPackets = 0;
            routerMap.clear();
            preTime = nowTime; 
        }
        
	    // In order to resolve Huawei NetStream packets. They use the high bit to denote the flow direction.
	    WORD flowVer = ntohs(*((WORD*)recvBuff.get())) & 0x7fff;

        WORD recordCount = int();
        NetFlow1Msg* v1_flow = (NetFlow1Msg *)recvBuff.get();
        NetFlow5Msg* v5_flow = (NetFlow5Msg *)recvBuff.get();
        
        switch(flowVer){
        case NETFLOW_VERSION_1:
            recordCount = ntohs((u_short)v1_flow->header.count);
            if(NETFLOWS_V1_PER_PAK < recordCount)
            {
            	m_pLog->error(RECV_TAG, "invalide package in version 1");
            	break;
            }

            // add flow record to data list.
            m_pSyn->lock();
            for(i = 0; i < recordCount; ++i)
            {
            	m_datalist.push_back(DataUnit(0,//++flowID,
                    routerIP,
                    nowTime,
                    1,
                    1,
                    ntohl(v1_flow->records[i].srcaddr),
                    ntohl(v1_flow->records[i].dstaddr),
                    ntohs(v1_flow->records[i].input),
                    ntohs(v1_flow->records[i].output),
                    ntohl(v1_flow->records[i].packets),
                    ntohl(v1_flow->records[i].octets),
                    ntohs(v1_flow->records[i].srcport),
                    ntohs(v1_flow->records[i].dstport),
                    v1_flow->records[i].protocol,
                    int(),
                    int(),
                    int(),
                    int(),
                    int(),
                    int(),
                    int(),
                    v1_flow->records[i].tos,
                    ntohl(v1_flow->records[i].nexthop))
                    );
			} // for
			m_pSyn->unlock();
            break;
        case NETFLOW_VERSION_5:
            recordCount = ntohs((u_short)v5_flow->header.count);
            if(NETFLOWS_V5_PER_PAK < recordCount)
            {
            	m_pLog->error(RECV_TAG, "invalide package in version 5");
                break;
            }

            // add flow record to data list.
            m_pSyn->lock();
            for(i = 0; i < recordCount; ++i)
            {
                m_datalist.push_back(DataUnit(0,//++flowID,
                    routerIP,
                    nowTime,
                    1,
                    5,
                    ntohl(v5_flow->records[i].srcaddr),
                    ntohl(v5_flow->records[i].dstaddr),
                    ntohs(v5_flow->records[i].input),
                    ntohs(v5_flow->records[i].output),
                    ntohl(v5_flow->records[i].packets),
                    ntohl(v5_flow->records[i].octets),
                    ntohs(v5_flow->records[i].srcport),
                    ntohs(v5_flow->records[i].dstport),
                    v5_flow->records[i].protocol,
                    ntohs(v5_flow->records[i].srcas),
                    ntohs(v5_flow->records[i].dstas),
                    v5_flow->records[i].srcmask,
                    v5_flow->records[i].dstmask,   
                    ntohl(v5_flow->records[i].srcaddr) & maskTab[v5_flow->records[i].srcmask],
                    ntohl(v5_flow->records[i].dstaddr) & maskTab[v5_flow->records[i].dstmask],
                    v5_flow->records[i].tcpflags,
                    v5_flow->records[i].tos,
                    ntohl(v5_flow->records[i].nexthop))
                    );
            }
            m_pSyn->unlock();
            break;
        default:
                break;
        } // (flowVer)
	} // for(;m_pbRun;)
    
    m_pLog->info(RECV_TAG, "Quit flow collector thread.");

	return 0;
} //FlowCollector::Run

/*inline */vector<DataUnit>& FlowCollector::GetData()
{
	return m_datalist;
}
