//=============================================================================
// File:          flowstat.hpp
// Author:        Zhaojunwei
// CreateTime:    2005/04/19
// Descripiton:   Head file for monitoring flowdal and flowsp process running
//                status.
// ----------------------------------------------------------------------------
// Version:
//         2005/04/19   1.0 First work release.
//=============================================================================

#ifndef FLOWSTAT_HPP
#define FLOWSTAT_HPP

#include <stdlib.h>
#include <unistd.h>

#include <iostream>
#include <fstream>
#include <string>

#include <filelock.hpp>

using namespace std;

struct STAT_INFO
{
	string	name;
	time_t	start;
	time_t	end;
};

const string GetTime(time_t t)
{
	struct tm current_tm;
    current_tm = *localtime(&t);
    char buff[20];
    snprintf(buff, 20, "%04u/%02u/%02u %02u:%02u:%02u",
    	current_tm.tm_year+1900,
    	current_tm.tm_mon+1,
    	current_tm.tm_mday,
    	current_tm.tm_hour,
    	current_tm.tm_min,
    	current_tm.tm_sec);
    return string(buff);
}

#endif // FLOWSTAT_HPP
