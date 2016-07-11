//=============================================================================
// File:          flowsp.cpp
// Author:        Zhaojunwei
// CreateTime:    2004/08/10
// Descripiton:   Implementation file for invoking sp in oracle database to 
//                process raw flow data process for web.
// ----------------------------------------------------------------------------
// Version:
//         2005/04/11   1.0 First work release.
//         2005/04/21   1.1 Add PC Session alarm class and modify FlowSP to use it.
//         2004/04/29   1.2 Add inserting alarm to database.
//         2006/07/31   1.3 Change PC Session alarm to a common alarm process
//=============================================================================

#include "flowsp.hpp"

CrontabSP::CrontabSP(const string& strFmt, const string& strTag, const string& strCmd,
	DbParam* pDbpar, ThreadLog* pLog) :
	Crontab(strFmt, strTag, strCmd), m_pDbpar(pDbpar), m_pLog(pLog)
{
}

int CrontabSP::Run(void)
{
	if(m_status == RUNNING)
	{
		m_pLog->error(m_strTag.c_str(), "Previous thread is still running! Discard data and quit.");
		return -1;
	}
	m_status = RUNNING;
	    
	time_t start_t, end_t;
	time(&start_t);
	SetStart(start_t);
	
	Environment 	*penv = NULL ;
	Connection      *pconn= NULL;
    Statement       *pstmt= NULL;
//    ResultSet 		*prs  = NULL;
	m_pLog->debug(m_strTag.c_str(), "Connect oracle database...");
	try
	{
		penv  = Environment::createEnvironment(Environment::THREADED_MUTEXED);
		pconn = penv->createConnection(m_pDbpar->dbUser.c_str(),
    		m_pDbpar->dbPassword.c_str(),
    		m_pDbpar->dbInstance.c_str());
    	if(pconn == NULL)
    	{
	    	throw runtime_error("Connect oracle database failed!");
	    }
		m_pLog->debug(m_strTag.c_str(), "Connect oracle database successfully.");
	}
    catch(SQLException& sqlex)
    {
    	m_pLog->error(m_strTag.c_str(), "Exception thrown for connecting database!");
        m_pLog->error(m_strTag.c_str(), sqlex.getMessage().c_str());

	    Environment::terminateEnvironment(penv);
	    time(&end_t);
	    SetEnd(end_t);
        m_status = IDLE;
        return -1;
    }
    catch (exception& ex)
    {
       	m_pLog->error(m_strTag.c_str(), "Exception thrown for connecting database!");
    	m_pLog->error(m_strTag.c_str(), ex.what());

	    Environment::terminateEnvironment(penv);
	    time(&end_t);
	    SetEnd(end_t);
    	m_status = IDLE;
        return -1;
    }
    
    char szSql[256];
	char szLog[256];
   	snprintf(szSql, sizeof(szSql), "begin %s; end;", m_strCmd.c_str());
   	try
   	{
    	pstmt = pconn->createStatement(szSql);
		pstmt->executeUpdate();
	    pconn->terminateStatement(pstmt);
   	}
    catch(SQLException& sqlex)
    {
    	m_pLog->error(m_strTag.c_str(), "Exception thrown for executing sp!");
        m_pLog->error(m_strTag.c_str(), sqlex.getMessage().c_str());
        m_pLog->error(m_strTag.c_str(), szSql);

        pconn->terminateStatement(pstmt);
    }
    catch (exception& ex)
    {
       	m_pLog->error(m_strTag.c_str(), "Exception thrown for executing sp!");
    	m_pLog->error(m_strTag.c_str(), ex.what());
    }

    penv->terminateConnection(pconn);
    Environment::terminateEnvironment(penv);

    time(&end_t);
    SetEnd(end_t);
	snprintf(szLog, sizeof(szLog), 
		"Execute %s costs %d seconds.", 
		m_strCmd.c_str(), (int)difftime(end_t, start_t));
    m_pLog->debug(m_strTag.c_str(), szLog);

    m_status = IDLE;
	return 0;
}

CrontabRawdataAlarm::CrontabRawdataAlarm(const string& strFmt, const string& strTag, const string& strCmd,
	DbParam* pDbpar, ThreadLog* pLog, bool bOVO, bool bChinese, const string& strSnmpTrap, const string& strOVOServer) :
	Crontab(strFmt, strTag, strCmd), m_pDbpar(pDbpar), m_pLog(pLog), 
		m_bOVO(bOVO), m_bChinese(bChinese), m_strSnmpTrap(strSnmpTrap),	m_strOVOServer(strOVOServer)
{
}

int CrontabRawdataAlarm::Run(void)
{
	if(m_status == RUNNING)
	{
		m_pLog->error(m_strTag.c_str(), "Previous thread is still running! Discard data and quit.");
		return -1;
	}
	
	if(!m_bOVO)
	{
		m_pLog->info(m_strTag.c_str(), "OVO is not integreated, alarm could not processed!");
		return 0;
	}
	
	m_status = RUNNING;
	    
	time_t start_t, end_t;
	time(&start_t);
	SetStart(start_t);
	
	Environment 	*penv = NULL ;
	Connection      *pconn= NULL;
    Statement       *pstmt= NULL;
    ResultSet 		*prs  = NULL;
	m_pLog->debug(m_strTag.c_str(), "Connect oracle database...");
	try
	{
		penv  = Environment::createEnvironment(Environment::THREADED_MUTEXED);
		pconn = penv->createConnection(m_pDbpar->dbUser.c_str(),
    		m_pDbpar->dbPassword.c_str(),
    		m_pDbpar->dbInstance.c_str());
    	if(pconn == NULL)
    	{
	    	Environment::terminateEnvironment(penv);
			throw runtime_error("Connect oracle database failed!");
	    }
		m_pLog->debug(m_strTag.c_str(), "Connect oracle database successfully.");
	}
    catch(SQLException& sqlex)
    {
    	m_pLog->error(m_strTag.c_str(), "Exception thrown for connecting database!");
        m_pLog->error(m_strTag.c_str(), sqlex.getMessage().c_str());

	    Environment::terminateEnvironment(penv);
	    time(&end_t);
	    SetEnd(end_t);
        m_status = IDLE;
        return -1;
    }
    catch (exception& ex)
    {
       	m_pLog->error(m_strTag.c_str(), "Exception thrown for connecting database!");
	    time(&end_t);
	    SetEnd(end_t);
    	m_status = IDLE;
        return -1;
    }
    
    char szSql[1024];
	char szLog[256];
	char szBuff[2048];
	Alarm aRecord;
	vector<Alarm> alarmList;
	set<uint> idList;
	
	try
   	{
    	m_pLog->debug(m_strTag.c_str(), "Get new arrived alarms from iptfa_nf_alarm.");
   		snprintf(szSql, sizeof(szSql), "select alarm_id,event_time,alarm_level,\
alarm_title_c,alarm_title_e,alarm_text_c,alarm_text_e from iptfa_nf_alarm \
where send_flag=0 and process_flag=0");
    	pstmt = pconn->createStatement(szSql);
		prs = pstmt->executeQuery();
        while(prs->next())
        {
        	aRecord.uAlarmID     = prs->getUInt(1);
        	aRecord.uEventTime   = prs->getUInt(2);
        	aRecord.uAlarmLevel  = prs->getUInt(3);
        	aRecord.strCnTitle   = prs->getString(4);
        	aRecord.strEnTitle   = prs->getString(5);
        	aRecord.strCnText    = prs->getString(6);
        	aRecord.strEnText    = prs->getString(7);
        	alarmList.push_back(aRecord);
        	idList.insert(aRecord.uAlarmID);
        }
        pstmt->closeResultSet(prs);
	    pconn->terminateStatement(pstmt);
		sprintf(szLog, "Get %d records for sending alarm.", alarmList.size());
	    m_pLog->info(m_strTag.c_str(), szLog);
	    if(!alarmList.empty())
	    {
	    	m_pLog->debug(m_strTag.c_str(), "Update iptfa_nf_alarm flag for next processing.");
	   		snprintf(szSql, sizeof(szSql), "update iptfa_nf_alarm set process_flag=1 where send_flag=0 and alarm_id between %u and %u",
	   			*(idList.begin()), *(idList.rbegin()));
	    	pstmt = pconn->createStatement(szSql);
		    pstmt->executeUpdate();
		    pconn->commit();
		    pconn->terminateStatement(pstmt);
	    }
   	}
    catch(SQLException& sqlex)
    {
    	m_pLog->error(m_strTag.c_str(), "Exception thrown from checking sessions!");
        m_pLog->error(m_strTag.c_str(), sqlex.getMessage().c_str());
        m_pLog->error(m_strTag.c_str(), szSql);
        pconn->terminateStatement(pstmt);
    }
    catch (exception& ex)
    {
       	m_pLog->error(m_strTag.c_str(), "Exception thrown from checking sessions!");
    	m_pLog->error(m_strTag.c_str(), ex.what());
    }

    penv->terminateConnection(pconn);
    Environment::terminateEnvironment(penv);

    if(!alarmList.empty())
    {
	    m_pLog->debug(m_strTag.c_str(), "Begin to send alarms.");
	
	    vector<Alarm>::const_iterator ita;
	    string strAlarmLevel;
	    
	    for(ita = alarmList.begin(); ita != alarmList.end(); ++ita)
	    {
	    	switch(ita->uAlarmLevel)
		    {
		    case 0:
		    	strAlarmLevel = "Normal";
		    	break;
		    case 1:
		    	strAlarmLevel = "Critical";
		    	break;
		    case 2:
		    	strAlarmLevel = "Major";
		    	break;
		    case 3:
		    	strAlarmLevel = "Minor";
		    	break;
		    case 4:
		    	strAlarmLevel = "Warning";
		    	break;
		    default:
		    	strAlarmLevel = "Major";
		    	break;
		    }
	    	
	   		if(m_bChinese)
	   		{
		    	snprintf(szBuff, sizeof(szBuff),
			     			"%s %s .1.3.6.1.4.1.6884.23 \"\" 6 700 0 \
.1.3.6.1.4.1.11.2.17.2.5.0 OctetString \"%s\" \
.1.3.6.1.4.1.11.2.17.2.4.0 OctetString \"%s\" \
.1.3.6.1.4.1.11.2.17.2.4.0 OctetString \"%s\"",
			     			m_strSnmpTrap.c_str(),
			     			m_strOVOServer.c_str(),
			     			strAlarmLevel.c_str(),
			     			ita->strCnTitle.c_str(),
			     			ita->strCnText.c_str());
	   		}
	   		else
	   		{
		    	snprintf(szBuff, sizeof(szBuff),
			     			"%s %s .1.3.6.1.4.1.6884.23 \"\" 6 700 0 \
.1.3.6.1.4.1.11.2.17.2.5.0 OctetString \"%s\" \
.1.3.6.1.4.1.11.2.17.2.4.0 OctetString \"%s\" \
.1.3.6.1.4.1.11.2.17.2.4.0 OctetString \"%s\"",
			     			m_strSnmpTrap.c_str(),
			     			m_strOVOServer.c_str(),
			     			strAlarmLevel.c_str(),
			     			ita->strEnTitle.c_str(),
			     			ita->strEnText.c_str());
	   		}
	
	    	#ifdef WIN32
           	STARTUPINFO si;
		    PROCESS_INFORMATION pi;
		
		    ZeroMemory(&si, sizeof(si) );
		    si.cb = sizeof(si);
		    si.dwFlags = STARTF_USESHOWWINDOW;
		    si.wShowWindow = SW_HIDE;
		    ZeroMemory(&pi, sizeof(pi) );

           	// Start the snmptrap process. 
		    if(!CreateProcess( NULL, // No module name (use command line). 
		        szBuff,               // Command line. 
		        NULL,                 // Process handle not inheritable. 
		        NULL,                 // Thread handle not inheritable. 
		        FALSE,                // Set handle inheritance to FALSE. 
		        0,                    // No creation flags. 
		        NULL,                 // Use parent's environment block. 
		        NULL,                 // Use parent's starting directory. 
		        &si,                  // Pointer to STARTUPINFO structure.
		        &pi )                 // Pointer to PROCESS_INFORMATION structure.
		    ) 
		    {
		        m_pLog->error(m_strTag.c_str(), szBuff);
		        continue;
		    }
		
		    // Wait until child process exits.
		    WaitForSingleObject(pi.hProcess, INFINITE );
		
		    // Close process and thread handles. 
		    CloseHandle(pi.hProcess );
		    CloseHandle(pi.hThread );
		    m_pLog->debug(m_strTag.c_str(), szBuff);
		    #else
	    	FILE *ftrapout;
     		if((ftrapout = popen(szBuff, "r")) != NULL)
     		{
     			while(fgets(szBuff, sizeof(szBuff), ftrapout) != NULL)
     				;
     			pclose(ftrapout);
			    m_pLog->debug(m_strTag.c_str(), szBuff);
     		}
     		else
     		{
		        m_pLog->error(m_strTag.c_str(), szBuff);
		        continue;
     		}
     		#endif
	    }
    }
    
    time(&end_t);
    SetEnd(end_t);
	snprintf(szLog, sizeof(szLog), 
		"Send %d alarms costs %d seconds.", 
		alarmList.size(), (int)difftime(end_t, start_t));
    m_pLog->debug(m_strTag.c_str(), szLog);

    m_status = IDLE;
	return 0;
}

FlowSP::FlowSP(const string& strConf) :	
	m_strConfFile(strConf), m_pLog(NULL), m_bRun(true),	m_bOVO(false), m_bChinese(false)
{
}
	
FlowSP::~FlowSP()
{
}

void FlowSP::ShowUsage(const char *pszProg)
{
    cout << "\nFlowInsight scheduling program. Copyright BOCO, 2005(C)" << endl;
    cout << "FlowInsight Version 3.0, Thread model: posix.\n" << endl;
    cout << "Usage:  " << pszProg << " [option]" << endl;
    #ifdef WIN32
    cout << "opt:   help | install [dependency] | uninstall | start | stop | status | debug" << endl;    
    cout << "       help:      show this help message." << endl;
    cout << "       install:   install flowinsight collector as windows service." << endl;
    cout << "       uninstall: uninstall flowinsight collector service." << endl;
    cout << "       start:     start flowinsight collector service." << endl;
    cout << "       stop:      stop flowinsight collecotor service." << endl;
    cout << "       status:    show current services status, started or stoped." << endl;
    cout << "       debug:     run application in console mode rather than service for debug\n" << endl;
    cout << "Note:  install option can take a string as service dependency, which will" << endl;
    cout << "       determine the startup order of associated services. For example," << endl;
    cout << "       flowinsight scheduler service may depend on oracle service, so we" << endl;
    cout << "       can install the service as flow:\n" << endl;
    cout << "       " << pszProg << " install \"oracle9i service\"\n" << endl;
    cout << "       The flowinsight collector service will be called only if oracle" << endl;
    cout << "       service is started successfully while the system was rebooting.\n" << endl;
    #else
    cout << "       option = -<h|v>" << endl;
    cout << "       -h: help information." << endl;
    cout << "       -v: version information." << endl;
    #endif
} // void FlowSP::ShowUsage(const char *pszProg)

bool FlowSP::ReadConf(void)
{
	uint i, uSpCount;
	char szSpItem[8];
	ReadCfg cfg;
	
	if(!cfg.OpenCfg(m_strConfFile))
		return false;
	
	m_strLogDir 		= cfg.GetValue("FLOWSP", "logdir");
	m_strLogFileName	= cfg.GetValue("FLOWSP", "logfilename");
	m_strLogLevel		= cfg.GetValue("FLOWSP", "loglevel");
	m_ulLogMaxSize		= atol(cfg.GetValue("FLOWSP", "logfilesize").c_str());
	m_dbParam.dbUser	= cfg.GetValue("DATABASE", "user");
	m_dbParam.dbPassword= cfg.GetValue("DATABASE", "password");
	m_dbParam.dbInstance= cfg.GetValue("DATABASE", "service");
	
	uSpCount			= atoi(cfg.GetValue("FLOWSP", "SP Number").c_str());
	if(uSpCount == 0)
		return false;
	m_timeCmd.clear();
	for(i=0; i<uSpCount; ++i)
	{
		snprintf(szSpItem, sizeof(szSpItem), "SP%d", i+1);
		m_timeCmd.push_back(cfg.GetValue("FLOWSP", szSpItem));
	}
	
	m_bOVO = (cfg.GetValue("ALARM", "OVO")[0] == 'y' || cfg.GetValue("ALARM", "OVO")[0]== 'Y');
	m_strSessionsAlarm	= cfg.GetValue("ALARM", "Schedule");
	if(m_bOVO)
	{
		m_strSnmpTrap		= cfg.GetValue("ALARM", "snmptrap");
		m_strOVOServer		= cfg.GetValue("ALARM", "OVO Server");
		m_bChinese			= (cfg.GetValue("ALARM", "LANG") == "zh" || cfg.GetValue("ALARM", "LANG") == "ZH") ? true : false;
	}
	
	return true;
}

bool FlowSP::OpenLog(void)
{
	string strFullPathName = m_strLogDir+"/"+m_strLogFileName;
	m_pLog = new ThreadLog(strFullPathName.c_str(), 5, m_ulLogMaxSize);
	const char INIT_TAG[] = "INIT";
	if(!m_pLog->OpenLog())
	{
		return false;
	}
	
	char szRec[128];
	m_pLog->info(INIT_TAG, "Process started successfully.");
	m_pLog->info(INIT_TAG, "Read parameter and open log file successfully.");
	snprintf(szRec, sizeof(szRec), "Set log level to [%s].", m_strLogLevel.c_str());
	m_pLog->info(INIT_TAG, szRec);
	m_pLog->setPriority(m_strLogLevel);
	
	return true;
}

int FlowSP::Log(void)
{
    m_pLog->SetThreadMode(true);
    m_pLog->info("LOGT", "Set log mode from file to thread.");

	for(;m_bRun;){
		if(!m_pLog->WriteLog())
			break;
	}

	if(m_bRun)
	{
		m_pLog->SetThreadMode(false);
		m_bRun = false;
		cerr << "Error in Log thread, check disk space!" << endl;
		return -1;
	}
	else
	{
		m_pLog->SetThreadMode(false);
		m_pLog->info("LOGT", "Quit Log thread.");
	}
	return 0;
} // int FlowSP::Log(void)

int FlowSP::Run(void)
{
	if(!OpenLog())
		return 10;
	
	int i;
	const char MAIN_TAG[] = "MAIN";
	#ifndef WIN32
	m_stat[0].name = "Main      thread";
	m_stat[1].name = "Log       thread";
	m_stat[2].name = "Scheduler thread";
	for(i=0; i<3; i++)
	{
		m_stat[i].start = time(NULL);
		m_stat[i].end   = m_stat[i].start;
	}
	struct sigaction newact;
	newact.sa_handler = ::Signal;
	sigemptyset(&newact.sa_mask);
	newact.sa_flags = 0;
	if(sigaction(SIGHUP, &newact, NULL) == -1)
	{
		cerr << "Error in set signal fuction for HUP. Process quited!" << endl;
		m_pLog->fatal(MAIN_TAG, "Set signal HUP function error! Process quited!");
		exit(5);
	}
	if(sigaction(SIGTERM, &newact, NULL) == -1)
	{
		cerr << "Error in set signal fuction for TERM. Process quited!" << endl;
		m_pLog->fatal(MAIN_TAG, "Set signal TERMfunction error! Process quited!");
		exit(5);
	}
	m_pLog->info(MAIN_TAG, "Set singal procdure successfully.");
	#endif //Win32
	
	m_pLog->info(MAIN_TAG, "Start instancing all crontab command configuration.");
	char szBuff[10240], szStatus[256];
	try
	{
		vector<string>::const_iterator its;
		string::size_type pos;
		string strCmd, strFmt;
		for(i=0, its=m_timeCmd.begin(); its!=m_timeCmd.end(); ++its)
		{
			snprintf(szBuff, sizeof(szBuff), "SP%02d", ++i);
			if((pos = its->find('|')) == string::npos)
			{
				snprintf(szBuff, sizeof(szBuff), "Error in get setting of [SP%02d]:[%s], No seperator '|' was found!", i, its->c_str());
				throw runtime_error(szBuff);
			}
			m_timeTable.insert(make_pair(
				new CrontabSP(its->substr(0, pos),
					string(szBuff),
					its->substr(pos+1),
					&m_dbParam,
					m_pLog),
				#ifdef WIN32
				(HANDLE)0
				#else
				(pthread_t)0
				#endif
				)
			);
		}

		m_timeTable.insert(make_pair(
				new CrontabRawdataAlarm(m_strSessionsAlarm,
					string("AL01"),
					string("Alarm Process"),
					&m_dbParam,
					m_pLog,
					m_bOVO,
					m_bChinese,
					m_strSnmpTrap,
					m_strOVOServer),
				#ifdef WIN32
				(HANDLE)0
				#else
				(pthread_t)0
				#endif
				)
		);
	}
    catch(exception& ex)
    {
    	m_pLog->error(MAIN_TAG, ex.what());
    	return 11;
    }
	m_pLog->info(MAIN_TAG, "Instanced all crontab command successfully.");

    m_pLog->info(MAIN_TAG, "Start log thread...");
    #ifdef WIN32
    HANDLE hLogThread = CreateThread(NULL, 0, ::WriteLog, (LPVOID)this, 0, NULL);
    #else
	pthread_t logThreadID;
	pthread_create(&logThreadID, NULL, ::WriteLog, (void *)this);
	#endif
    m_pLog->info(MAIN_TAG, "Create log thread successfully.");

    m_pLog->info(MAIN_TAG, "Enter main loop.");
    #ifndef WIN32
	m_pLock->Truncate(0);
	m_pLock->Rewind();
	snprintf(szBuff, sizeof(szBuff),
			"    Scheduler is running, pid = [%d], started at [%s].\n",
			getpid(), GetTime(m_stat[0].start).c_str());
	m_pLock->Write(szBuff, strlen(szBuff));

	struct timespec interval;
    interval.tv_sec = 0;
    interval.tv_nsec = 500000000;

    pthread_t dbThreadID;
	pthread_attr_t attrDbThread;
	pthread_attr_init(&attrDbThread);
	pthread_attr_setdetachstate(&attrDbThread, PTHREAD_CREATE_DETACHED);
	#endif // WIN32
    time_t current_t, pre_t;
   	struct tm current_tm;
	#ifdef WIN32
   	map<Crontab*,HANDLE>::iterator itc;
   	#else
   	map<Crontab*,pthread_t>::iterator itc;
   	#endif

	for(pre_t=0; m_bRun ;)
    {
    	#ifdef WIN32
		Sleep(500);
		#else
        nanosleep(&interval, NULL);
		#endif //WIN32
		time(&current_t);
		current_tm = *localtime(&current_t);
		if(current_tm.tm_sec != 0 || difftime(current_t, pre_t) < 1.0)
			continue;
		time(&pre_t);
		for(itc=m_timeTable.begin(); itc!=m_timeTable.end(); ++itc)
		{
			if(itc->first->Check(current_tm.tm_min,
				current_tm.tm_hour,
				current_tm.tm_mday,
				current_tm.tm_mon,
				current_tm.tm_wday))
			{
				snprintf(szBuff, sizeof(szBuff), "Start thread [%s]{%s}..",
					itc->first->GetTag().c_str(),
					itc->first->GetCmd().c_str());
				m_pLog->debug(MAIN_TAG, szBuff);
				#ifdef WIN32
				if(itc->first->GetStatus() == Crontab::RUNNING)
				{
					m_pLog->error(MAIN_TAG, "Previous thread is still running!");
					continue;
				}
				if(itc->second != NULL)
				{
					CloseHandle(itc->second);
				}
				itc->second = CreateThread(NULL,
		            0,
		            ::CallSP,
		            (LPVOID)(itc->first),
		            0,
		            NULL
		        );
				if(itc->second == NULL)
				{
					m_pLog->fatal(MAIN_TAG, "Cannot create thread for scheduling task!");
					continue;
				}
				#else
		        pthread_create(&dbThreadID,
					&attrDbThread,
					::CallSP, 
					(void *)(itc->first)
				);
				#endif
			}
		}

		#ifdef WIN32
		Sleep(1000);
		#else
	    sleep(1);
		#endif //WIN32

		strncpy(szBuff, "Thread pool status: ", sizeof(szBuff));
		for(itc=m_timeTable.begin(); itc!=m_timeTable.end(); ++itc)
		{
			snprintf(szStatus, sizeof(szStatus), "[%c]",
				(itc->first)->GetStatus() == Crontab::RUNNING ? 'R' : 'I');
			strncat(szBuff, szStatus, sizeof(szBuff));
		}
		m_pLog->info(MAIN_TAG, szBuff);
		
		#ifndef WIN32
		m_pLock->Truncate(0);
		m_pLock->Rewind();
		snprintf(szBuff, sizeof(szBuff),
			"    Scheduler is running, pid = [%d], started at [%s].\n    Threads status:\n",
			getpid(), GetTime(m_stat[0].start).c_str());
		for(i=0; i<3; i++)
		{
			snprintf(szStatus, sizeof(szStatus),
				"\t\t%s is running, started at [%s].\n",
				m_stat[i].name.c_str(), GetTime(m_stat[i].start).c_str());
			strncat(szBuff, szStatus, sizeof(szBuff));
		}
		for(i=0, itc=m_timeTable.begin(); itc!=m_timeTable.end(); ++itc)
		{
			snprintf(szStatus, sizeof(szStatus), 
				"\t\tPool thread [%d]{%s} status:[%s], ",
				i++,itc->first->GetTag().c_str(),
				itc->first->GetStatus() == Crontab::RUNNING ? "running" : "idle");
			strncat(szBuff, szStatus, sizeof(szBuff));
			if(itc->first->GetStatus() == Crontab::RUNNING)
			{	
				snprintf(szStatus, sizeof(szStatus),
					"started at %s.\n", GetTime(itc->first->GetCost().first).substr(11).c_str());
			}
			else
			{
				snprintf(szStatus, sizeof(szStatus),
					"previous run costed %d seconds.\n",
					(int)difftime(itc->first->GetCost().second, itc->first->GetCost().first));
			}
			strncat(szBuff, szStatus, sizeof(szBuff));
		}
		m_pLock->Write(szBuff, strlen(szBuff));
		#endif // WIN32
    }
 
	m_pLog->info(MAIN_TAG, "Waiting all sp thread to quit...");
	
	for(itc=m_timeTable.begin(); itc!=m_timeTable.end(); ++itc)
	{
		while(itc->first->GetStatus() == Crontab::RUNNING)
		{
			#ifdef WIN32
			Sleep(1000);
			#else
	        sleep(1);
			#endif //WIN32
		}
		snprintf(szBuff, sizeof(szBuff), "Quit thread [%s]{%s}.", 
			itc->first->GetTag().c_str(),
			itc->first->GetCmd().c_str());
		m_pLog->info(MAIN_TAG, szBuff);
	}

	m_pLog->info(MAIN_TAG, "Start to delete all crontab command instances...");
	for(itc=m_timeTable.begin(); itc!=m_timeTable.end(); ++itc)
	{
		delete itc->first;
	}
	m_pLog->info(MAIN_TAG, "Deleting all crontab command instnaces successfully.");

	#ifdef WIN32
	HANDLE hThreads[] = {hLogThread};
	WaitForMultipleObjects(1, hThreads, false, INFINITE);
	#else
    pthread_join(logThreadID, NULL);
	m_pLog->info(MAIN_TAG, "Main thread received log thread quit message.");
	#endif

    #ifdef WIN32
	//CloseHandle(hThread);
	#else
    pthread_attr_destroy(&attrDbThread);
    #endif
	m_pLog->info(MAIN_TAG, "Quit process.");
	
	return 0;
} // int FlowSP::Run(void)

#ifndef WIN32
void FlowSP::SingalProcess(int signo)
{
	if(signo == SIGHUP)
	{
		m_pLog->notify("SYSTEM", "Received HUP signal, update running configuration.");
		ReadConf();
	}
	if(signo == SIGTERM)
	{
		m_pLog->notify("SYSTEM", "Received TERM signal, prepare to quit...");
		m_bRun = false;
	}
} // FlowSP::SingalProcess
#endif // WIN32

FlowSP* g_pFlowsp;
		
#ifdef WIN32
DWORD ServiceThread(LPDWORD param)
#else
int main(int argc, char* argv[])
#endif
{
	#ifdef WIN32
    HANDLE hObject = CreateMutex(NULL, FALSE, "FlowSP");
    if(GetLastError() == ERROR_ALREADY_EXISTS)
    {
	    CloseHandle(hObject);
        cerr << "Another instance is running, process quited!" << endl;
        exit(1);
	}
    #else
    if(argc > 1)
    {
    	if(string(argv[1])==string("-v"))
    	{
    		cout << "FlowInsight Version 3.0  BOCO 2004-2005" << endl;
    		cout << "Flow scheduler 3.0, release 060801";
    		exit(0);
    	}
    	else
    	{
    		FlowSP::ShowUsage(argv[0]);
   		    exit(0);
    	}
    }
    
    FileLock mylock("/tmp/.flowsp.lock");

    if(mylock.Lock() != LOCK_SUCCESS)
    {
        cerr << "Another instance is running, process quited!" << endl;
        exit(1);
    }
    mylock.Truncate(0);
    #endif //Win32

    #ifdef WIN32
    const char cDelimiter = '\\';
    const char szDelimter[] = "\\";
    #else
    const char cDelimiter = '/';
    const char szDelimter[] = "/";
    #endif

	char *pEnvPath = getenv("ORACLE_HOME");
	if(pEnvPath == NULL)
	{
		cerr << "Error in getting ORACLE_HOME enviorment!" << endl;
		exit(2);
	}
	pEnvPath = getenv("FLOWINSIGHT_HOME");
	if(pEnvPath == NULL)
	{
		cerr << "Error in getting FLOWINSIGHT_HOME enviorment!" << endl;
		exit(3);
	}
	string strConf = pEnvPath;
	if(strConf[strConf.length() - 1] != cDelimiter)
	{
		strConf += szDelimter;
	}
	strConf += "conf";
    strConf += szDelimter;
    strConf += "flowinsight.conf";

   	g_pFlowsp = new FlowSP(strConf);
   	#ifndef WIN32
	g_pFlowsp->SetLockFile(&mylock);
	#endif
	if(!g_pFlowsp->ReadConf())
	{
		cerr << "Error in open configuration: " << strConf << endl;
		exit(4);
	}

	int nReturnCode = g_pFlowsp->Run();
	delete g_pFlowsp;
	return nReturnCode;
}

#ifdef WIN32
DWORD WINAPI CallSP(LPVOID lpPar)
{
	CrontabSP* pCrontabSP = (CrontabSP*)lpPar;
	return (DWORD)pCrontabSP->Run();
}
#else
void* CallSP(void* psoc)
{
	CrontabSP* pCrontabSP = (CrontabSP*)psoc;
	return (void *)pCrontabSP->Run();
}
#endif // WIN32

#ifdef WIN32
DWORD WINAPI WriteLog(LPVOID lpSelf)
{
	FlowSP* pSelf = (FlowSP*)lpSelf;
	return (DWORD)pSelf->Log();
}
#else
void* WriteLog(void* psoc)
{
	FlowSP* pSelf = (FlowSP*)psoc;
	return (void*)pSelf->Log();
}
#endif // WIN32

#ifdef WIN32
//Service operation
const char    SERVICE_NAME[]    = "FlowInsight Scheduler";
const char    DISPLAY_NAME[]    = "FlowInsight Database SP Schedule Service";
const char    DESCRIPTION[]     = "FlowInsight数据处理服务，完成数据库计算处理和告警发送等功能。"; 
HANDLE        g_terminateEvent  = NULL;
HANDLE        g_serviceThread   = NULL;
ServiceOperation srvOp(SERVICE_NAME, DISPLAY_NAME);

int main(int argc, char* argv[])
{
    SERVICE_TABLE_ENTRY DispatchTable[] = {
        {(LPTSTR)SERVICE_NAME, (LPSERVICE_MAIN_FUNCTION)ServiceMain},
        {NULL, NULL}
    };
    
    if(argc > 1)
    {
        string strPar(argv[1]);
        char strBinPath[256];
        GetModuleFileName(NULL, strBinPath, sizeof(strBinPath));
        
        if(strPar == "install")
        {
            
            if(srvOp.InstallService(strBinPath, argv[2], DESCRIPTION))
                cout << "Install service successfully." << endl;
            else
                cerr << "Install servcie failed!" << endl;
        }
        else
        if(strPar == "uninstall")
        {
            if(srvOp.RemoveService())
                cout << "Uninstall service successfully." << endl;
            else
                cerr << "Uninstall servcie failed!" << endl;
        }
        else
        if(strPar == "start")
        {
            if(srvOp.StartService())
                cout << "Start service successfully." << endl;
            else
                cerr << "Start servcie failed!" << endl;
        }
        else
        if(strPar == "stop")
        {
            if(srvOp.StopService())
                cout << "Stop service successfully." << endl;
            else
                cerr << "Stop servcie failed!" << endl;
        }
        else
       	if(strPar == "status")
       	{
       		cout << srvOp.ShowStatus() << endl;
       	}
       	else
        if(strPar == "debug")
        {
        	ServiceThread(0);
        }
        else
        {
            FlowSP::ShowUsage(argv[0]);
        }
    }
    else
    {
        if(!StartServiceCtrlDispatcher(DispatchTable))
        {
            DWORD dwErr = GetLastError();

            switch(dwErr)
            {
            case ERROR_FAILED_SERVICE_CONTROLLER_CONNECT:
                 cout << "The program is being run as a console application rather than as a service. " << endl;
                 cout << "Type \"" << argv[0] << " help\" for details.";
                 break;
            case ERROR_INVALID_DATA:
                 cerr << "The dispatch table contains entries that are not in the proper format." << endl;
                 break;
            case ERROR_SERVICE_ALREADY_RUNNING:
                 cerr << "The process has already called StartServiceCtrlDispatcher. " << endl;
                 break;
            default:
                 break;
            }
        }
    }
    
    return 0;
}

void QuitService(DWORD dwErrCode)
{
    SetEvent(g_terminateEvent);
}

void WINAPI ServiceMain(DWORD argc, LPTSTR *argv) 
{ 
    srvOp.GetStatusHandle() = RegisterServiceCtrlHandler(SERVICE_NAME, ControlHandler);
    if(srvOp.GetStatusHandle() == (SERVICE_STATUS_HANDLE)0)
        return;

    if(!srvOp.SendStatusToSCM(SERVICE_START_PENDING, NO_ERROR, 0, 1, 1000)) 
        return;

    g_terminateEvent = CreateEvent(NULL, true, false, NULL);
    if(!g_terminateEvent)
        return;
    
    srvOp.SendStatusToSCM(SERVICE_START_PENDING, NO_ERROR, 0, 2, 1000);
    
    bool bInit = ServiceInit();
    if(!bInit)
    {
        QuitService(GetLastError());
        srvOp.SendStatusToSCM(SERVICE_STOPPED, NO_ERROR, 0, 0, 0);
        return;
    }
    else
    {
        srvOp.SendStatusToSCM(SERVICE_RUNNING, NO_ERROR, 0, 0, 0);
    }
    WaitForSingleObject(g_terminateEvent, INFINITE);

    CloseHandle(g_terminateEvent);
    ServiceQuit();
}

void WINAPI ControlHandler(DWORD dwCtrlCode)
{
    switch(dwCtrlCode)
    {
        case SERVICE_CONTROL_STOP:
            srvOp.SendStatusToSCM(SERVICE_STOP_PENDING, NO_ERROR, 0, 1, 3000);
            QuitService(0);
            srvOp.SendStatusToSCM(SERVICE_STOPPED, NO_ERROR, 0, 0, 0);
            break;
        case SERVICE_CONTROL_INTERROGATE:
            srvOp.SendStatusToSCM(SERVICE_RUNNING, NO_ERROR, 0, 0, 0);
            break;
        default:
            break;     
    }
    
    return;
}

bool ServiceInit()
{
    bool bInit;
    g_serviceThread = CreateThread(NULL,
                      0,
                      (LPTHREAD_START_ROUTINE)ServiceThread,
                      0,
                      0,
                      NULL);
    
    bInit = (g_serviceThread == NULL ? false: true);
    return bInit;
}

bool ServiceQuit()
{
    if(g_serviceThread)
        CloseHandle(g_serviceThread);

    return true;
}

#else // WIN32
void Signal(int signo)
{
	if(g_pFlowsp != NULL)
		g_pFlowsp->SingalProcess(signo);
	return;
}
#endif // WIN32
