//=============================================================================
// File:          flowsp.hpp
// Author:        Zhaojunwei
// CreateTime:    2004/08/10
// Descripiton:   Head file for invoking sp in oracle database to process raw
//                flow data process for web.
// ----------------------------------------------------------------------------
// Version:
//         2005/04/11   1.0 First work release.
//=============================================================================

#ifndef FLOW_SP_HPP
#define FLOW_SP_HPP

#ifdef WIN32
#pragma warning(disable:4786)
#include <service.hpp>
#include <limits>
#else
#include <limits.h>
#include <unistd.h>
#include <signal.h>
#endif // WIN32

#include <stdlib.h>

#include <set>
#include <string>
#include <vector>
#include <string.h>

#include <readcfg.hpp>
#include <threadlog.hpp>
#include <filelock.hpp>
#include <crontab.hpp>
#include <tocci.h>
#include "flowform.hpp"
#include "dboperator.hpp"

#ifndef WIN32
#include "flowstat.hpp"
#endif

using namespace std;
using namespace oracle::occi;

class CrontabSP : public Crontab
{
public:
	CrontabSP(const string& strFmt, const string& strTag, const string& strCmd,
		DbParam* pDbpar, ThreadLog* pLog);
	virtual ~CrontabSP(){};
	
	virtual int Run(void);
private:
	DbParam*	m_pDbpar;
	ThreadLog*	m_pLog;
};

class CrontabRawdataAlarm : public Crontab
{
public:
	CrontabRawdataAlarm(const string& strFmt, const string& strTag, const string& strCmd,
		DbParam* pDbpar, ThreadLog* pLog,
		bool bOVO, bool bChinese, const string& strSnmpTrap, const string& strOVOServer);
	virtual ~CrontabRawdataAlarm(){};
	
	virtual int Run(void);
private:
	typedef struct _alarm
	{
		uint   uAlarmID;
		time_t uEventTime; 
		uint   uAlarmLevel;
		string strCnTitle;
		string strEnTitle;
		string strCnText;
		string strEnText; 
	} Alarm;
	
	DbParam*	m_pDbpar;
	ThreadLog*	m_pLog;
	bool		m_bOVO;
	bool		m_bChinese;
	string		m_strSnmpTrap;
	string		m_strOVOServer;
};

class FlowSP
{
public:
	FlowSP(const string& strConf);
	~FlowSP();
	
	static void	ShowUsage(const char* pszProg);		// Show Help and version information

	bool ReadConf(void);
	bool OpenLog(void);
	int Log(void);
	int Run(void);
	#ifndef WIN32
	void SingalProcess(int signo);
	#endif 

	#ifndef WIN32
	void			SetLockFile(FileLock* pLock)		// To record running stat information under unix.
					{m_pLock=pLock;};
	#endif
	
private:
	string		m_strConfFile;
	string		m_strLogDir;
	string		m_strLogFileName;
	string		m_strLogLevel;
	uint		m_ulLogMaxSize;
	DbParam		m_dbParam;
	ThreadLog*	m_pLog;
	
	vector<string> 		m_timeCmd;
	#ifdef WIN32
	map<Crontab*,HANDLE> m_timeTable;
	#else
	map<Crontab*,pthread_t>	m_timeTable;
	#endif

	volatile bool m_bRun;
	
	bool		m_bOVO;
	bool		m_bChinese;
	string		m_strSessionsAlarm;
	string		m_strSnmpTrap;
	string		m_strOVOServer;
	#ifndef WIN32
	FileLock*		m_pLock;
	// For stat information of threads include main, log, scheduler.
	// Thread of dboperators will get form its methor of Crontab::GetCost
	STAT_INFO		m_stat[3];
	#endif
};

#ifdef WIN32
DWORD WINAPI CallSP(LPVOID lpPar);
#else
void* CallSP(void* psoc);
#endif // WIN32

#ifdef WIN32
DWORD WINAPI WriteLog(LPVOID lpSelf);
#else
void* WriteLog(void* psoc);
#endif // WIN32

#ifdef WIN32
void QuitService(DWORD dwErrCode);
void WINAPI ServiceMain(DWORD argc, LPTSTR *argv);
void WINAPI ControlHandler(DWORD dwCtrlCode);
bool ServiceInit();
bool ServiceQuit();
DWORD ServiceThread(LPDWORD param);
#else
void Signal(int signo);	// Global Sinagal function, will call FlowSP::SingalProcess()
#endif // WIN32

#endif // FLOW_SP_HPP

