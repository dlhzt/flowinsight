//=============================================================================
// File:          flowcollector.hpp
// Author:        Zhaojunwei
// CreateTime:    2004/08/10
// Descripiton:   Head file for receiving flow data via UDP packets.
// ----------------------------------------------------------------------------
// Version:
//         2004/08/10   1.0 First work release.
//         2005/04/04   1.1 Modify the sole function to class FlowCollector
//=============================================================================

#ifndef FLOWCOLLECTOR_HPP
#define FLOWCOLLECTOR_HPP

#include <vector>
#include <threadlog.hpp>
#include <synchronizer.hpp>
#include "flowform.hpp"

using namespace std;

struct DataUnit{
	DWORD		 flowid;			// sequence ID
	DWORD        routerip;		    // ipaddress of the flow originor
	                                           
	DWORD        coltime;			// flow collected time
	BYTE      	 flowtype;			// flow type including UNKNOWN-0, NETFLOW-1, NETSTREAM-2, CFLOW-3, SFLOW-4....
	BYTE         flowversion;		// flow version, now its only illstrate netflow
	DWORD        srcaddr;			// source ip address
	DWORD        dstaddr;			// destination ip address
	WORD         input;				// input interface index
	WORD         output;			// output interface index
	DWORD        packets;			// packets send in duration
	DWORD        octets;			// octets send in duration
	WORD         srcport;			// source port
	WORD         dstport;			// destination port
	BYTE         protocol;			// ip protocal, e.g., 6=TCP, 17=UDP, ...
	WORD         srcas;				// originating AS of source address
	WORD         dstas;				// originating AS of destination address
	BYTE         srcmask;			// mask of source address
	BYTE         dstmask;			// mask of destination address
	DWORD        srcprefix;			// 
	DWORD        dstprefix;			// 
	BYTE         tcpflags;			// tcp flags
	BYTE         tos;				// type of service
	DWORD        nexthop;			// next hop ip address
	
	DataUnit(DWORD in_flowid      = int(),	
		DWORD      in_routerip    = int(),
		DWORD      in_coltime     = int(),	
		WORD       in_flowtype    = int(),	
		WORD       in_flowversion = int(),
		DWORD      in_srcaddr     = int(),	
		DWORD      in_dstaddr     = int(),	
		WORD       in_input       = int(),		
		WORD       in_output      = int(),	
		DWORD      in_packets     = int(),	
		DWORD      in_octets      = int(),	
		WORD       in_srcport     = int(),	
		WORD       in_dstport     = int(),	
		BYTE       in_protocol    = int(),	
		WORD       in_srcas       = int(),		
		WORD       in_dstas       = int(),		
		BYTE       in_srcmask     = int(),	
		BYTE       in_dstmask     = int(),	
		DWORD      in_srcprefix   = int(),	
		DWORD      in_dstprefix   = int(),	
		BYTE       in_tcpflags    = int(),	
		BYTE       in_tos         = int(),		
		DWORD      in_nexthop     = int())
		:flowid(in_flowid), routerip(in_routerip),
		coltime(in_coltime), flowtype(in_flowtype),
		flowversion(in_flowversion), srcaddr(in_srcaddr),
		dstaddr(in_dstaddr), input(in_input),
		output(in_output), packets(in_packets),
		octets(in_octets), srcport(in_srcport),
		dstport(in_dstport), protocol(in_protocol),
		srcas(in_srcas), dstas(in_dstas),
		srcmask(in_srcmask), dstmask(in_dstmask),
		srcprefix(in_srcprefix), dstprefix(in_dstprefix),
		tcpflags(in_tcpflags), tos(in_tos), nexthop(in_nexthop)
		{};
	~DataUnit(){};
};           

class FlowCollector
{
public:
	FlowCollector(uint port, Synchronizer* pSyn, ThreadLog* pLog, volatile bool* pbRun);
	~FlowCollector();
	
	int Run();								// Work thread method for collecting flow data.
											// must be called in form of pthread_create.
	vector<DataUnit>& GetData();			// Return the data buffer, be not defined as const
											// for performance optimizing via vector::swap().

private:
	const uint			m_port;				// UDP listen port
	Synchronizer*		m_pSyn;				// Synchronizer for data vector accessing
	ThreadLog*			m_pLog;				// Log handler to record the working status in other thread
	volatile bool*		m_pbRun;			// Running flag reference to keep thread in loop.
	vector<DataUnit>	m_datalist;			// Flow data container, should be synchronized in
											// multi-thread accessing via mutex.
};

#endif // FLOWCOLLECTOR_HPP

