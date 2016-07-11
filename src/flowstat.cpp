//=============================================================================
// File:          flowstat.cpp
// Author:        Zhaojunwei
// CreateTime:    2005/04/19
// Descripiton:   Implementation file for monitoring flowdal and flowsp process
//                running status.
// ----------------------------------------------------------------------------
// Version:
//         2005/04/19   1.0 First work release.
//=============================================================================

#include "flowstat.hpp"

int main(int argc, char* argv[])
{
	const char FLOWD_LOCK_FILE[]	= "/tmp/.flowd.lock";
	const char FLOWDAL_LOCK_FILE[]	= "/tmp/.flowdal.lock";
	const char FLOWSP_LOCK_FILE[]	= "/tmp/.flowsp.lock";
	char buff[2048];
	
	ifstream flowdFile(FLOWD_LOCK_FILE);
	ifstream flowdalFile(FLOWDAL_LOCK_FILE);
	ifstream flowspFile(FLOWSP_LOCK_FILE);
	bool flowd = true;
	bool flowdal = true;
	bool flowsp = true;
	string flowd_stat;
	string flowdal_stat;
	string flowsp_stat;
	if(!flowdFile)
	{
        flowd = false;
	}
	else
	{
		while(flowdFile.getline(buff, sizeof(buff)))
		{
			flowd_stat += buff;
			flowd_stat += "\n";
		}
	}
	if(!flowdalFile)
	{
    	flowdal = false;
	}
	else
	{
		while(flowdalFile.getline(buff, sizeof(buff)))
		{
			flowdal_stat += buff;
			flowdal_stat += "\n";
		}
	}
	if(!flowspFile)
	{
    	flowsp = false;
	}
	else
	{
		while(flowspFile.getline(buff, sizeof(buff)))
		{
			flowsp_stat += buff;
			flowsp_stat += "\n";
		}
	}

    FileLock daemonlock(FLOWD_LOCK_FILE);
    if(daemonlock.Lock() == LOCK_SUCCESS)
    {
        flowd = false;
    }

    FileLock dallock(FLOWDAL_LOCK_FILE);
    if(dallock.Lock() == LOCK_SUCCESS)
    {
    	flowdal = false;
    }
    FileLock splock(FLOWSP_LOCK_FILE);
    if(splock.Lock() == LOCK_SUCCESS)
    {
    	flowsp = false;
    }
	
	cout << "\nFlowInsight Ver 3.0, Copyright 2004-2006" << endl;
	cout << "Current time: " << GetTime(time(NULL));
	cout << "\nService status:" << endl;
	if(flowd)
		cout << flowd_stat << endl;
	else
		cout << "    Daemon    is not running!" << endl;
	if(flowdal)
		cout << flowdal_stat << endl;
	else
		cout << "    Collector is not running!" << endl;
	if(flowsp)
		cout << flowsp_stat << endl;
	else
		cout << "    Scheduler is not running!" << endl;
	cout << endl;
	
	return 0;
}
