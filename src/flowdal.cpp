//=============================================================================
// File:          flowdal.cpp
// Author:        Zhaojunwei
// CreateTime:    2004/08/10
// Descripiton:   Implementation file for flow dal. This is the main module that
//                schedule the flowcollector and dboperator.
// ----------------------------------------------------------------------------
// Version:
//         2004/08/10   1.0 First work release.
//         2005/04/04   1.1 Modify the sole function to class FlowCollector
//=============================================================================

#include "flowdal.hpp"
#ifdef WIN32
#include "license.h"
#endif

using namespace std;

FlowDal::FlowDal()
{
	m_strConfFile		= "";			// Full path name of configuration file
	m_strLogDir			= "";			// Log location
	m_strLogLevel		= "DEBUG";		// Log level, refer to class ThreadLog.
	m_strLogFileName	= "";			// Log file name, will suffix with file seq number.
	m_ulLogMaxSize		= 0;			// Log file max size
	m_strWorkDir		= "";			// For temporary file transfering
	m_uPort 			= 9991;			// UDP port for listening flow PDU.
	m_dbParam.dbUser	= "";			// Database user
	m_dbParam.dbPassword= "";			// Database password
	m_dbParam.dbInstance= "";			// Database SID
	m_dbParam.workDir	= "";			// Raw flow data store
	m_dbParam.mode		= DbParam::INNER;
	m_pFlowCollector	= NULL;			// Collector handler
	m_pLog				= NULL;			// Log handler
	m_bRun				= false;		// Thread schedule flag
	
	m_pSyn				= new Synchronizer();
	
	#ifndef WIN32
	m_pLock				= NULL;
	#endif
	
	for(int i=0; i<DB_THREAD_NUM; ++i)
		m_pDbOperator[i] = NULL;
} // FlowDal::FlowDal()

FlowDal::~FlowDal()
{
	delete m_pSyn;
	delete m_pLog;
}

FlowDal* FlowDal::GetFlowDal(void)
{
	return new FlowDal;
}

void FlowDal::ShowUsage(const char *pszProg)
{
    cout << "\nFlowInsight data collecting program. Copyright BOCO, 2005(C)" << endl;
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
    cout << "       flowinsight collector service may depend on oracle service, so we" << endl;
    cout << "       can install the service as flow:\n" << endl;
    cout << "       " << pszProg << " install \"oracle9i service\"\n" << endl;
    cout << "       The flowinsight collector service will be called only if oracle" << endl;
    cout << "       service is started successfully while the system was rebooting.\n" << endl;
    #else
    cout << "       option = -<h|v>" << endl;
    cout << "       -h: help information." << endl;
    cout << "       -v: version information." << endl;
    #endif
} // void FlowDal::ShowUsage(const char *pszProg)

inline void FlowDal::SetConfFile(const string& strConf)
{
	m_strConfFile = strConf;
}

bool FlowDal::ReadConf(void)
{
	ReadCfg cfg;
	
	if(!cfg.OpenCfg(m_strConfFile))
		return false;
	
	m_strLicense    = cfg.GetValue("FLOWDAL", "license");
	m_strLogDir 		= cfg.GetValue("FLOWDAL", "logdir");
	m_strLogFileName	= cfg.GetValue("FLOWDAL", "logfilename");
	m_strLogLevel		= cfg.GetValue("FLOWDAL", "loglevel");
	m_ulLogMaxSize		= atol(cfg.GetValue("FLOWDAL", "logfilesize").c_str());
	m_uPort				= atoi(cfg.GetValue("FLOWDAL", "udpport").c_str());
	m_dbParam.dbUser	= cfg.GetValue("DATABASE", "user");
	m_dbParam.dbPassword= cfg.GetValue("DATABASE", "password");
	m_dbParam.dbInstance= cfg.GetValue("DATABASE", "service");
	m_dbParam.workDir	= cfg.GetValue("FLOWDAL", "workdir");
	m_dbParam.mode		= cfg.GetValue("DATABASE", "insertmode") == string("SQLLDR") ? DbParam::SQLLDR : DbParam::INNER;
	m_dbParam.tabResrved    = atoi(cfg.GetValue("DATABASE", "tablereserved").c_str());
	
	if(m_strLicense.length() < 1 || m_strLogDir.length() < 1 || m_strLogFileName.length() < 1 ||
		m_strLogLevel.length() < 1 || m_ulLogMaxSize == 0 ||
		m_uPort == 0 || m_dbParam.dbUser.length() < 1 ||
		m_dbParam.dbPassword.length() < 1 || m_dbParam.dbInstance.length() < 1 ||
		m_dbParam.workDir.length() < 1 || m_dbParam.tabResrved == 0)
		return false;
	
	if(m_pLog != NULL)
	{
		m_pLog->setPriority(m_strLogLevel);
		m_pLog->notify("USER", "Change log priority.");
	}
	
	return true;	
} // bool FlowDal::ReadConf(void)

int FlowDal::Schedule(void)
{
	const char SCHEDULE_TAG[] = "SCDL";

	int nMinute = 0;
	#ifndef WIN32
	struct timespec interval;
    interval.tv_sec = 0;
    interval.tv_nsec = 500000000;
    #endif
	time_t current_t, pre_t=0;
	struct tm current_tm;

	#ifdef WIN32
	//HANDLE hThread;
	#else
	pthread_t dbThreadID;
	pthread_attr_t attrDbThread;
	pthread_attr_init(&attrDbThread);
	pthread_attr_setdetachstate(&attrDbThread, PTHREAD_CREATE_DETACHED);
	#endif
		
	char szBuff[2048], szStatus[128];
	int i;
	
	for(i=0; i<DB_THREAD_NUM; ++i)
	{	
		m_pDbOperator[i] = new DbOperator(&m_dbParam, m_pLog,i);
		#ifdef WIN32
		m_hDbOperator[i] = NULL;
		#endif
	}
	#ifndef WIN32
	m_pLock->Truncate(0);
	m_pLock->Rewind();
	snprintf(szBuff, sizeof(szBuff),
		"    Collector is running, pid = [%d], started at [%s].\n",
		getpid(), GetTime(m_stat[0].start).c_str());
		m_pLock->Write(szBuff, strlen(szBuff));
	#endif // WIN32
	
	m_pLog->info(SCHEDULE_TAG, "Schedule thread started.");		
	for(;m_bRun;){
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

		m_pLog->debug(SCHEDULE_TAG, "Get data from collect thread.");		
		m_pSyn->lock();
		m_pDbOperator[nMinute]->GetData().swap(m_pFlowCollector->GetData());
		m_pFlowCollector->GetData().clear();
		m_pSyn->unlock();
		snprintf(szBuff, sizeof(szBuff), "Invoke dboperator thread [%d].", nMinute); 
		m_pLog->debug(SCHEDULE_TAG, szBuff);	
		
		if(m_pDbOperator[nMinute]->GetStatus() == DbOperator::RUNNING)
		{
			m_pLog->warning(SCHEDULE_TAG, "Previous thread is still running!");
			++nMinute;
			nMinute %= DB_THREAD_NUM;
			continue;
		}
		#ifdef WIN32
		if(m_hDbOperator[nMinute] != NULL)
		{
			CloseHandle(m_hDbOperator[nMinute]);
		}
		m_hDbOperator[nMinute] = CreateThread(NULL,
            0,
            ::InDb,
            (LPVOID)(void *)m_pDbOperator[nMinute],
            0,
            NULL
        );
		if(m_hDbOperator[nMinute] == NULL)
		{
			m_pLog->fatal(SCHEDULE_TAG, "Cannot create thread for indb operation!");
			continue;
		}
		#else
        pthread_create(&dbThreadID,
			&attrDbThread,
			::InDb, 
			(void *)m_pDbOperator[nMinute]
		);
		#endif

		// Wait 1 second to insure the indb thread to start first.
		#ifdef WIN32
		Sleep(1000);
		#else
        sleep(1);
		#endif //WIN32

		strncpy(szBuff, "Thread pool status: ", sizeof(szBuff));
		for(i=0; i<DB_THREAD_NUM; i++)
		{
			snprintf(szStatus, sizeof(szStatus), "[%c]",
				m_pDbOperator[i]->GetStatus() == DbOperator::RUNNING ? 'R' : 'I');
			strncat(szBuff, szStatus, sizeof(szBuff));
		}
		m_pLog->debug(SCHEDULE_TAG, szBuff);	
		
		// Record running stat in lock file when running in unix
		#ifndef WIN32
		m_pLock->Truncate(0);
		m_pLock->Rewind();
		snprintf(szBuff, sizeof(szBuff),
			"    Collector is running, pid = [%d], started at [%s].\n    Threads status:\n",
			getpid(), GetTime(m_stat[0].start).c_str());
		for(i=0; i<4; i++)
		{
			snprintf(szStatus, sizeof(szStatus),
				"\t\t%s is running, started at [%s].\n",
				m_stat[i].name.c_str(), GetTime(m_stat[i].start).c_str());
			strncat(szBuff, szStatus, sizeof(szBuff));
		}
		for(i=0; i<DB_THREAD_NUM; i++)
		{
			snprintf(szStatus, sizeof(szStatus), 
				"\t\tPool thread [%d] status:[%s], ",
				i,
				m_pDbOperator[i]->GetStatus() == DbOperator::RUNNING ? "running" : "idle");
			strncat(szBuff, szStatus, sizeof(szBuff));
			if(m_pDbOperator[i]->GetStatus() == DbOperator::RUNNING)
			{	
				snprintf(szStatus, sizeof(szStatus),
					"started at %s.\n", GetTime(m_pDbOperator[i]->GetStart()).substr(11).c_str());
			}
			else
			{
				snprintf(szStatus, sizeof(szStatus),
					"previous run inserted %d flows and costed %d seconds.\n",
					m_pDbOperator[i]->GetRecords(),
					(int)difftime(m_pDbOperator[i]->GetEnd(), m_pDbOperator[i]->GetStart()));
			}
			strncat(szBuff, szStatus, sizeof(szBuff));
		}
		m_pLock->Write(szBuff, strlen(szBuff));
		#endif // WIN32
	
		++nMinute;
		nMinute %= DB_THREAD_NUM;	
	} // end for(;m_bRun;)

	m_pLog->debug(SCHEDULE_TAG, "Wait all indb thread...");
	
	for(i=0; i<DB_THREAD_NUM; i++)
	{	
		for(;m_pDbOperator[i]->GetStatus() == DbOperator::RUNNING;)
		{	
			#ifdef WIN32
			Sleep(1000);
			#else
	        sleep(1);
			#endif //WIN32
		}
		
		snprintf(szBuff, sizeof(szBuff), "Quit dboperator thread [%d].", i); 
		m_pLog->info(SCHEDULE_TAG, szBuff);
	
		delete m_pDbOperator[i];
	}

	#ifdef WIN32
	//CloseHandle(hThread);
	#else
    pthread_attr_destroy(&attrDbThread);
    #endif
	
	m_pLog->info(SCHEDULE_TAG, "Quit scheduling thread.");
	
	return 0;
} // int FlowDal::Schedule(void)

bool FlowDal::OpenLog(void)
{
	string strFullPathName;
	strFullPathName = m_strLogDir + "/" + m_strLogFileName;
	m_pLog = new ThreadLog(strFullPathName.c_str(), 5, m_ulLogMaxSize);
	if(m_pLog == NULL)
	{
		return false;
	}
	else
	{
		if(!m_pLog->OpenLog())
			return false;
		m_pLog->info("INIT", "Process started successfully.");
		m_pLog->info("INIT", "Read parameter and open log file successfully.");
	}
	
	return true;
} // bool FlowDal::OpenLog(void)

int FlowDal::Log(void)
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
} // int FlowDal::Log(void)

int FlowDal::Run(void)
{
	if(!OpenLog())
    {
    	cerr << "Error in opening log file!" << endl;
    	return 10;
    }
    
	const char MAIN_TAG[] = "MAIN";
	#ifdef WIN32
	char license[32];
	char msg[128];
	DWORD code;
	strncpy(license, m_strLicense.c_str(), 32);
	if((code=TestKey(license,msg)) != 0)
	{
		if(code == 1)
			m_pLog->fatal(MAIN_TAG, "License invalid, pls make sure the license key in flowinsight be valid!");
		else
			m_pLog->fatal(MAIN_TAG, "License expired, pls make sure the license key in flowinsight be valid!");
		m_pLog->info(MAIN_TAG, "Program quited because no valid license found.");
		return -127;
	}
	else
	{
		m_pLog->info(MAIN_TAG, msg);
	}
	#else
	struct sigaction newact;
	newact.sa_handler = ::Signal;
	sigemptyset(&newact.sa_mask);
	newact.sa_flags = 0;
	if(sigaction(SIGHUP, &newact, NULL) == -1){
		cerr << "Error in set signal fuction for HUP. Process quited!" << endl;
		m_pLog->fatal(MAIN_TAG, "Set signal HUP function error! Process quited!");
		exit(5);
	}
	if(sigaction(SIGTERM, &newact, NULL) == -1){
		cerr << "Error in set signal fuction for TERM. Process quited!" << endl;
		m_pLog->fatal(MAIN_TAG, "Set signal TERMfunction error! Process quited!");
		exit(5);
	}
	#endif //Win32
	
	m_bRun = true;
	
	#ifndef WIN32
	time_t current_t;
	time(&current_t);
	for(int i=0; i<4; i++)
	{
		m_stat[i].start = current_t;
		m_stat[i].end = 0;
	}
	m_stat[0].name = "Main      thread";
	m_stat[1].name = "Collector thread";
	m_stat[2].name = "Scheduler thread";
	m_stat[3].name = "Log       thread";
	#endif // WIN32
	
	m_pLog->info(MAIN_TAG, "Start log thread...");
    #ifdef WIN32
    HANDLE hLogThread = CreateThread(NULL, 0, ::WriteLog, (LPVOID)this, 0, NULL);
    #else
	pthread_t logThreadID;
	pthread_create(&logThreadID, NULL, ::WriteLog, (void *)this);
	#endif
    m_pLog->info(MAIN_TAG, "Create log thread successfully.");

	m_pLog->setPriority(m_strLogLevel);
	m_pLog->notify(MAIN_TAG, "Change log priority.");
	
	m_pLog->info(MAIN_TAG, "Start flow collector thread...");
	m_pFlowCollector = new FlowCollector(m_uPort, m_pSyn, m_pLog, &m_bRun);
	#ifdef WIN32
	HANDLE hRecvThread = CreateThread(NULL, 0, ::FlowCol, (LPVOID)m_pFlowCollector, 0, NULL);
	#else
	pthread_t receiveThreadID;
	pthread_create(&receiveThreadID, NULL, ::FlowCol, (void*)m_pFlowCollector);
	#endif //WIN32
	m_pLog->info(MAIN_TAG, "Create flow collector thread successfully.");

	m_pLog->info(MAIN_TAG, "Start schedule thread...");
	#ifdef WIN32
	HANDLE hScheduleThread = CreateThread(NULL, 0, ::MainLoop, (LPVOID)this, 0, NULL);
	#else
    pthread_t scheduleThreadID;
	pthread_create(&scheduleThreadID, NULL, ::MainLoop, (void *)this);
	#endif
    m_pLog->info(MAIN_TAG, "Create schedule thread successfully.");
	
	m_pLog->info(MAIN_TAG, "Main thread go to sleep..zZZ zZZ");
	#ifdef WIN32
	HANDLE hThreads[] = {hLogThread, hRecvThread, hScheduleThread};
	while(1)
	{
		if(WaitForMultipleObjects(3, hThreads, false, 3600000) == WAIT_TIMEOUT)
		{
				if((code=TestKey(license,msg)) != 0)
				{
						m_pLog->fatal(MAIN_TAG, "License expired, pls make sure the license key in flowinsight be valid!");
						return -127;
				}
				else
				{
					m_pLog->info(MAIN_TAG, "Valid license success.");
					continue;
				}
		}
		else
		{
			break;
		}
	}
	#else
	pthread_join(receiveThreadID, NULL);
	m_pLog->info(MAIN_TAG, "Main thread received collect thread quit message.");
    pthread_join(logThreadID, NULL);
	m_pLog->info(MAIN_TAG, "Main thread received log thread quit message.");
	pthread_join(scheduleThreadID, NULL);
	m_pLog->info(MAIN_TAG, "Main thread received schedule thread quit message.");
	#endif
	m_pLog->info(MAIN_TAG, "Quit main thread.");
	m_pLog->info(MAIN_TAG, "Quit process.");
	
	delete m_pFlowCollector;
	
	return 0;
} // int FlowDal::Run(void)

#ifndef WIN32
void FlowDal::SingalProcess(int signo)
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
} // FlowDal::SingalProcess
#endif // WIN32

FlowDal* g_pFlowDal = NULL;

#ifdef WIN32
DWORD ServiceThread(LPDWORD param)
#else
int main(int argc, char* argv[])
#endif
{
	#ifdef WIN32
    HANDLE hObject = CreateMutex(NULL, FALSE, "FlowDAL");
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
    		cout << "FlowInsight Version 3.0  BOCO 2004-2006" << endl;
    		cout << "Flow data collector 3.0, release 061216" << endl;
    		exit(0);
    	}
    	else
    	{
    		FlowDal::ShowUsage(argv[0]);
   		    exit(0);
    	}
    }
    
    FileLock mylock("/tmp/.flowdal.lock");

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
		exit(2);
	}

	string strConf = pEnvPath;
	if(strConf[strConf.length() - 1] != cDelimiter)
	{
		strConf += szDelimter;
	}
	strConf += "conf";
    strConf += szDelimter;
    strConf += "flowinsight.conf";
    
    g_pFlowDal = FlowDal::GetFlowDal();
    #ifndef WIN32
    g_pFlowDal->SetLockFile(&mylock);
    #endif
    g_pFlowDal->SetConfFile(strConf);
    if(!g_pFlowDal->ReadConf())
    {
    	cerr << "Error in reading configuration: " << strConf << "!" << endl;
    	exit(3);
    }

    int nCode = g_pFlowDal->Run();
    delete g_pFlowDal;
    return nCode;
} // main()

#ifdef WIN32
//Service operation
const char    SERVICE_NAME[]    = "FlowInsight Collector";
const char    DISPLAY_NAME[]    = "FlowInsight Data collecting Service";
const char    DESCRIPTION[]     = "FlowInsight后台采集入库服务，用于采集路由器Netflow/NetStream流量数据。"; 
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
        	FlowDal::ShowUsage(argv[0]);
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
	if(g_pFlowDal != NULL)
		g_pFlowDal->SingalProcess(signo);
	
	return;
}
#endif // WIN32

#ifdef WIN32
DWORD WINAPI InDb(LPVOID lpDb)
{
	DbOperator*	pDB = (DbOperator*)lpDb;
	return (DWORD)pDB->Indb();
}
#else
void* InDb(void* psoc){
	DbOperator*	pDB = (DbOperator*)psoc;
	return (void*)pDB->Indb();
}
#endif // WIN32

#ifdef WIN32
DWORD WINAPI FlowCol(LPVOID lpFlow)
{
	FlowCollector*	pFlowCol = (FlowCollector*)lpFlow;
	return (DWORD)pFlowCol->Run();
}
#else
void* FlowCol(void* psoc)
{
	FlowCollector*	pFlowCol = (FlowCollector*)psoc;
	return (void*)pFlowCol->Run();
}
#endif // WIN32

#ifdef WIN32
DWORD WINAPI MainLoop(LPVOID lpSelf)
{
	FlowDal* pSelf = (FlowDal*)lpSelf;
	return (DWORD)pSelf->Schedule();
}
#else
void* MainLoop(void* psoc)
{
	FlowDal* pSelf = (FlowDal*)psoc;
	return (void*)pSelf->Schedule();
}
#endif // WIN32

#ifdef WIN32
DWORD WINAPI WriteLog(LPVOID lpSelf)
{
	FlowDal* pSelf = (FlowDal*)lpSelf;
	return (DWORD)pSelf->Log();
}
#else
void* WriteLog(void* psoc)
{
	FlowDal* pSelf = (FlowDal*)psoc;
	return (void*)pSelf->Log();
}
#endif // WIN32
