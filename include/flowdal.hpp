//=============================================================================
// File:          flowdal.hpp
// Author:        Zhaojunwei
// CreateTime:    2004/08/10
// Descripiton:   Head file for flow dal. This is the main module that schedule
//                the flowcollector and dboperator.
// ----------------------------------------------------------------------------
// Version:
//         2004/08/10   1.0 First work release.
//         2005/04/04   1.1 Modify the sole function to class FlowCollector
//=============================================================================

#ifndef FLOWDAL_HPP
#define FLOWDAL_HPP

#include <iostream>
#include <fstream>
#include <string>
#include <sstream>

#ifdef WIN32
#include <limits>
#else
#include <limits.h>
#include <unistd.h>
#endif // WIN32

#ifdef WIN32
#pragma warning(disable:4786)
#include <service.hpp>
#else
#include <signal.h>
#endif

#include <stdlib.h>

#include <threadlog.hpp>
#include <filelock.hpp>
#include <readcfg.hpp>

#include "flowcollector.hpp"
#include "dboperator.hpp"

#ifndef WIN32
#include "flowstat.hpp"
#endif // WIN32

using std::string;

class FlowDal
{
protected:
	FlowDal();											// Protected constructor to avoid multi-instancing.
	FlowDal(const FlowDal&a);
	FlowDal& operator=(const FlowDal&a);

public:
	~FlowDal();
	
	static FlowDal*	GetFlowDal();						// For producting a new FlowDal instance.
	static void		ShowUsage(const char* pszProg);		// Show Help and version information

	void			SetConfFile(const string& strConf);	// Set full path name of config file.
	bool			ReadConf(void); 					// Read configuration file
	int				Schedule(void);						// Create dboperator thread each minute.
	bool			OpenLog(void);						// Set Log handler shared in all thread.
	int				Log(void);							// Create log thread to write all log data.
	int 			Run(void);							// Default process module
	
	#ifndef WIN32
	void			SetLockFile(FileLock* pLock)		// To record running stat information under unix.
					{m_pLock=pLock;};
	#endif
	
	#ifdef WIN32
	#define WIN32_SERVICE
	#else
	void			SingalProcess(int signo);
	#endif //WIN32
	
private:
	// Common setting
	string  m_strLicense;
	string	m_strConfFile;		// Full path name of configuration file
	string	m_strLogDir;		// Log location
	string	m_strLogLevel;		// Log level, refer to class ThreadLog.
	string	m_strLogFileName;	// Log file name, will suffix with file seq number.
	ulong	m_ulLogMaxSize;		// Log file max size, will cycle to next file while
								// current log file reache this limit.
	string	m_strWorkDir;		// For temporary file transfering.
	
	// Flow setting
	uint	m_uPort;			// UDP port for listening flow PDU.
	
	// Database setting
	DbParam	m_dbParam;			// Database operator configuration param.
	
	// Running setting
	FlowCollector* 	m_pFlowCollector;
	DbOperator* 	m_pDbOperator[DB_THREAD_NUM];
	#ifdef WIN32
	HANDLE			m_hDbOperator[DB_THREAD_NUM];
	#endif
	ThreadLog*		m_pLog;		// Log file handler will share in all working thread.
	Synchronizer*	m_pSyn;		// Synchronizor among all thread.
	volatile bool 	m_bRun;		// Flag to notify all thread to quit if set to false.
	
	#ifndef WIN32
	FileLock*		m_pLock;
	// For stat information of threads include main, log, collector, scheduler.
	// Thread of dboperators will get form its methor of Crontab::GetCost
	STAT_INFO		m_stat[4];
	#endif
};


#ifdef WIN32
void QuitService(DWORD dwErrCode);
void WINAPI ServiceMain(DWORD argc, LPTSTR *argv);
void WINAPI ControlHandler(DWORD dwCtrlCode);
bool ServiceInit();
bool ServiceQuit();
DWORD ServiceThread(LPDWORD param);
#else
void Signal(int signo);	// Global Sinagal function, will call FlowDal::SingalProcess()
#endif // WIN32

#ifdef WIN32
DWORD WINAPI InDb(LPVOID lpDb);
#else
void* InDb(void* psoc);
#endif // WIN32

#ifdef WIN32
DWORD WINAPI FlowCol(LPVOID lpFlow);
#else
void* FlowCol(void* psoc);
#endif // WIN32

#ifdef WIN32
DWORD WINAPI MainLoop(LPVOID lpSelf);
#else
void* MainLoop(void* psoc);
#endif // WIN32

#ifdef WIN32
DWORD WINAPI WriteLog(LPVOID lpSelf);
#else
void* WriteLog(void* psoc);
#endif // WIN32

#endif // FLOWDAL_HPP
