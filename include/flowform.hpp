//=============================================================================
// File:          flowform.hpp
// Author:        Zhaojunwei
// CreateTime:    2004/08/10
// Descripiton:   Head file for descripation of flow data pdu.
// ----------------------------------------------------------------------------
// Version:
//         2004/08/10   1.0 First work release.
//         2005/04/04   1.1 Remove the Netflow V6, V7, V8 and modify the data
//                          type to WORD, DWORD to avoid inconstant size of 
//                          long while migration from 32bit to 64bit platform.
//                          The sturct names are also modified to be more 
//                          appropriate thinking of other flow like netstream.
//=============================================================================

#ifndef FLOWFORM_HPP
#define FLOWFORM_HPP

typedef unsigned char	uchar;
typedef unsigned short  ushort;
typedef unsigned int	uint;
typedef unsigned long   ulong;
#ifndef WIN32
typedef unsigned char   BYTE;
typedef unsigned short	WORD;
typedef unsigned int    DWORD;
#endif

////////////////////////////////////////////////////////////////////////////////
//	Netflow version 1
//  Pdu defination

#define NETFLOW_VERSION_1		            1
#define NETFLOWS_V1_PER_PAK		            24

typedef struct
{
    WORD     version;         // 1 for version 1
    WORD     count;           // The number of records in PDU
    DWORD    sysuptime;       // Current time in millisecs since router booted
    DWORD    unixsecs;        // Current seconds since 0000 UTC 1970
    DWORD    unixnsecs;       // Residual nanoseconds since 0000 UTC 1970
} NetFlow1Hdr;

typedef struct
{
    DWORD    srcaddr;        // Source IP Address 
    DWORD    dstaddr;        // Destination IP Address 
    DWORD    nexthop;        // Next hop router's IP Address 
    WORD     input;          // Input interface index 
    WORD     output;         // Output interface index 
    
    DWORD    packets;        // Packets sent in Duration 
    DWORD    octets;         // Octets sent in Duration. 
    DWORD    firsttime;      // SysUptime at start of flow 
    DWORD    lasttime;       // and of last packet of flow 

    WORD     srcport;        // TCP/UDP source port number or equivalent
    WORD     dstport;        // TCP/UDP destination port number or equivalent 
    WORD     pad;
    BYTE     protocol;       // IP protocol, e.g., 6=TCP, 17=UDP, ... 
    BYTE     tos;            // IP Type-of-Service 
    
    BYTE     flags;          // Reason flow was discarded, etc...  
    BYTE     tcpretxcnt;     // Number of mis-seq with delay > 1sec 
    BYTE     tcpretxsecs;    // Cumulative seconds between mis-sequenced pkts 
    BYTE     tcpmisseqcnt;   // Number of mis-sequenced tcp pkts seen 
    DWORD    reserved;
} NetFlow1Rec;

typedef struct
{
    NetFlow1Hdr header;
    NetFlow1Rec records[NETFLOWS_V1_PER_PAK];
} NetFlow1Msg;

#define MAX_V1_NETFLOW_PAK_SIZE (sizeof(NetFlow1Hdr) + sizeof(NetFlow1Rec) * NETFLOWS_V1_PER_PAK)

////////////////////////////////////////////////////////////////////////////////
//	Netflow version 5
//  Pdu defination

#define NETFLOW_VERSION_5		            5
#define NETFLOWS_V5_PER_PAK		            30

typedef struct
{
	//BYTE	 nsid
   // BYTE     version;   
    WORD     version; 
    WORD     count;          
    DWORD    sysuptime;      
    DWORD    unixsecs;       
    DWORD    unixnsecs;      
    DWORD    streamseq;   
    BYTE     enginetype;     
    BYTE     slotid;       
    WORD     reserved;
} NetFlow5Hdr;

typedef struct
{
    DWORD    srcaddr;        // Source IP Address 
    DWORD    dstaddr;        // Destination IP Address 
    DWORD    nexthop;        // Next hop router's IP Address
    WORD     input;          // Input interface index 
    WORD     output;         // Output interface index 
    
    DWORD    packets;        // Packets sent in Duration
    DWORD    octets;         // Octets sent in Duration.
    DWORD    firsttime;      // SysUptime at start of flow 
    DWORD    lasttime;       // and of last packet of flow 
    
    WORD     srcport;        // TCP/UDP source port number or equivalent 
    WORD     dstport;        // TCP/UDP destination port number or equivalent 
    BYTE     pad;
    BYTE     tcpflags;       // Cumulative OR of tcp flags
    BYTE     protocol;       // IP protocol, e.g., 6=TCP, 17=UDP, ...
    BYTE     tos;            // IP Type-of-Service
    WORD     srcas;          // originating AS of source address
    WORD     dstas;          // originating AS of destination address
    BYTE     srcmask;        // source address prefix mask bits
    BYTE     dstmask;        // destination address prefix mask bits
    WORD     reserved;
} NetFlow5Rec;

typedef struct
{
    NetFlow5Hdr header;
    NetFlow5Rec  records[NETFLOWS_V5_PER_PAK];
} NetFlow5Msg;

#define MAX_V5_NETFLOW_PAK_SIZE (sizeof(NetFlow5Hdr) + sizeof(NetFlow5Rec) * NETFLOWS_V5_PER_PAK)
 
#endif // FLOWFORM_HPP

