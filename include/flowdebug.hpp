//=============================================================================
// File:          flowdebug.hpp
// Author:        Zhaojunwei
// CreateTime:    2005/04/27
// Descripiton:   Head file for debuging flow data via UDP packets.
// ----------------------------------------------------------------------------
// Version:
//         2005/04/27   1.0 First work release.
//=============================================================================

#ifndef FLOW_DEBUG_HPP
#define FLOW_DEBUG_HPP

#ifndef WIN32
#include <unistd.h>             // unix
#include <sys/socket.h>         // bsd socket stuff
#include <netinet/in.h>         // network types
#include <arpa/inet.h>          // arpa types
#else
#pragma warning(disable:4786)
#include <winsock.h>
#endif

#include <iostream>

#include "flowform.hpp"

using namespace std;

#endif
