//=============================================================================
// File:          flowd.cpp
// Author:        Zhaojunwei
// CreateTime:    2005/04/19
// Descripiton:   Implementation file for monitor flowdal and flowsp process.
// ----------------------------------------------------------------------------
// Version:
//         2005/04/19   1.0 First work release.
//=============================================================================

#include <filelog.hpp>
#include <errno.h>
#include "flowd.hpp"
#include "flowstat.hpp"

bool		g_bRun = true;
Process 	proc[2];

void sighandler(int signo)
{
	if(signo == SIGHUP)
	{
		kill(proc[0].pid, SIGHUP);
		kill(proc[1].pid, SIGHUP);
	}
	if(signo == SIGTERM)
	{
		g_bRun = false;
		kill(proc[0].pid, SIGTERM);
		kill(proc[1].pid, SIGTERM);
	}
}

int main(int argc, char* argv[])
{
	const char FLOWD_LOCK_FILE[]	= "/tmp/.flowd.lock";
	const char FLOWDAL_LOCK_FILE[]	= "/tmp/.flowdal.lock";
	const char FLOWSP_LOCK_FILE[]	= "/tmp/.flowsp.lock";
	const char TAG[] = "FLOWD";
	
	proc[0].strProcName = "flowdal";
	proc[0].stat = Process::QUITED;
	proc[1].strProcName = "flowsp";
	proc[1].stat = Process::QUITED;

	#ifdef WIN32
    HANDLE hObject = CreateMutex(NULL, FALSE, "FlowD");
    if(GetLastError() == ERROR_ALREADY_EXISTS)
    {
	    CloseHandle(hObject);
        cerr << "Another instance is running, process quited!" << endl;
        return 2;
	}
    HANDLE hDalObject = CreateMutex(NULL, FALSE, "FlowDAL");
    if(GetLastError() == ERROR_ALREADY_EXISTS)
    {
	    CloseHandle(hDalObject);
		cerr << proc[0].strProcName << " is running, stop it and restart flowd." << endl;
		return 2;
	}
    HANDLE hSpObject = CreateMutex(NULL, FALSE, "FlowSP");
    if(GetLastError() == ERROR_ALREADY_EXISTS)
    {
	    CloseHandle(hSpObject);
		cerr << proc[1].strProcName << " is running, stop it and restart flowd." << endl;
		return 2;
	}
    #else
    if(argc > 1)
    {
   		cerr << "FlowInsight Version 3.0" << endl;
   		cerr << "Daemon v1.0" << endl;
    }
    
    FileLock mylock(FLOWD_LOCK_FILE);
    if(mylock.Lock() != LOCK_SUCCESS)
    {
        cerr << "Another instance is running, process quited!" << endl;
        return 2;
    }

    FileLock dallock(FLOWDAL_LOCK_FILE);
    if(dallock.Lock() != LOCK_SUCCESS)
    {
		cerr << proc[0].strProcName << " is running, stop it and restart flowd." << endl;
		return 2;
    }
    FileLock splock(FLOWSP_LOCK_FILE);
    if(splock.Lock() != LOCK_SUCCESS)
    {
		cerr << proc[1].strProcName << " is running, stop it and restart flowd." << endl;
		return 2;
    }
    #endif //Win32
	char *pOraPath = getenv("ORACLE_HOME");
	if(pOraPath == NULL)
	{
		cerr << "Error in getting ORACLE_HOME enviorment!" << endl;
		return 3;
	}

	char *pEnvPath = getenv("FLOWINSIGHT_HOME");
	if(pEnvPath == NULL)
	{
		cerr << "Error in getting FLOWINSIGHT_HOME enviorment!" << endl;
		return 3;
	}
	
	struct stat filestat;
	int i;

	for(i=0; i<2; i++)
	{
		proc[i].strFullPathName = string(pEnvPath) + string("/bin/") + proc[i].strProcName;
		if(stat(proc[i].strFullPathName.c_str(), &filestat) != 0)
		{
			cerr << proc[i].strFullPathName << " can't be found!" << endl;
			return 4;
		}
	}

	pid_t   pid;
	if((pid = fork()) < 0)
	{	
		cerr << "Error in calling fork()!" << endl;
		return 1;
	}

    if(pid > 0)
	{
		mylock.Unlock();
		return 0;
	}
	
	setsid();
	umask(0);
	chdir("/");
	close(0);
	close(1);
	close(2);
	
	char szBuff[256];
	snprintf(szBuff, sizeof(szBuff), 
		"    Daemon    is running, pid = [%d], started at [%s].\n",
		getpid(),
		GetTime(time(NULL)).c_str());
	mylock.Lock();
    mylock.Truncate(0);
	mylock.Rewind();
	mylock.Write(szBuff, strlen(szBuff));
	
	string strLog(pEnvPath);
	strLog += "/log/flowd.log";
	FileLog mylog(strLog.c_str(), 2, 10240000);
	if(!mylog.OpenLog())
	{
		snprintf(szBuff, sizeof(szBuff), "Cannot open log: %s!\n", strLog.c_str());
		mylock.Write(szBuff, strlen(szBuff));
		exit(-1);
	}
	else
	{
		snprintf(szBuff, sizeof(szBuff), "Flowinsight daemon started successfully.");
		mylog.info(TAG, szBuff);
	}

	for(i=0; i<2; i++)
	{
		if((pid = fork()) < 0)
		{
			return 5;
		}
		if(pid == 0)
		{
			if(execl(proc[i].strFullPathName.c_str(), 
				proc[i].strProcName.c_str(),
				(char*)0) == -1
			  )
			{
				snprintf(szBuff, sizeof(szBuff), "Cannot spawn %s, error is %s.", proc[i].strProcName.c_str(), strerror(errno));
				mylog.fatal(TAG, szBuff);
				_exit(-1);
			}
		}
		proc[i].pid = pid;
		proc[i].stat = Process::RUNNING;
		proc[i].start = time(NULL);
		snprintf(szBuff, sizeof(szBuff), "Start %s successfully, pid=%d.", proc[i].strProcName.c_str(), pid);
		mylog.info(TAG, szBuff);
	}
	
	#ifndef WIN32
	struct sigaction newact;
	newact.sa_handler = sighandler;
	sigemptyset(&newact.sa_mask);
	newact.sa_flags = 0;
	if(sigaction(SIGHUP, &newact, NULL) == -1)
	{
		return 6;
	}
	if(sigaction(SIGTERM, &newact, NULL) == -1)
	{
		return 6;
	}
	#endif //Win32
	
	pid_t die;
	int status;
	int j;
	const int maxfork = 10000;
	for(j=0;(g_bRun&&j<maxfork);j++)
	{
		die = wait(&status);
		snprintf(szBuff, sizeof(szBuff), "%d quited abnormally.", die);
		mylog.error(TAG, szBuff);

		if(die != proc[0].pid && die != proc[1].pid)
			return 0;
		sleep(1);
		if(!g_bRun)
			break;
		
		if((pid = fork()) < 0)
			return 7;
		
		if(pid == 0)
		{
			for(i=0; i<2; i++)
			{
				if(die == proc[i].pid)
				{
					if(execl(proc[i].strFullPathName.c_str(), 
						proc[i].strProcName.c_str(),
						(char*)0) == -1
					)
					{
						snprintf(szBuff, sizeof(szBuff), "Cannot spawn %s, error is %s.", proc[i].strProcName.c_str(), strerror(errno));
						mylog.fatal(TAG, szBuff);
						_exit(-1);
					}
				}
			}
		}
		
		for(i=0; i<2; i++)
		{
			if(die == proc[i].pid)
			{
				proc[i].pid = pid;
				proc[i].start = time(NULL);
				snprintf(szBuff, sizeof(szBuff), "Start %s successfully, pid=%d.", proc[i].strProcName.c_str(), pid);
				mylog.info(TAG, szBuff);
			}
		}
	}
	die = wait(&status);
	
	return 0;
}
