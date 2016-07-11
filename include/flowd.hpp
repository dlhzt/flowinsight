//=============================================================================
// File:          flowd.hpp
// Author:        Zhaojunwei
// CreateTime:    2005/04/19
// Descripiton:   Head file for monitor flowdal and flowsp process.
// ----------------------------------------------------------------------------
// Version:
//         2005/04/19   1.0 First work release.
//=============================================================================

#ifndef FLOWD_HPP
#define FLOWD_HPP

#include <stdio.h>
#include <stdlib.h>

#ifndef WIN32
#include <unistd.h>
#include <signal.h>
#endif

#include <string>
#include <vector>
#include <iostream>
#include <fstream>

#include <filelock.hpp>

using namespace std;

struct Process
{
	string 			strProcName;
	string 			strFullPathName;
//	vector<string>	vArgs;
	pid_t			pid;
	time_t			start;
	
	enum Status
	{
		RUNNING, 
		QUITED
	} stat;
};

#endif // FLOWD_HPP
