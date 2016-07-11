//=============================================================================
// File:          dboperator.cpp
// Author:        Zhaojunwei
// CreateTime:    2004/08/10
// Descripiton:   Implementation file for storing flow data in database.
// ----------------------------------------------------------------------------
// Version:
//         2004/08/10   1.0 First work release.
//         2005/04/04   1.1 Modify the database from postgres to oracle.
//=============================================================================

#ifdef WIN32

#else
#include <unistd.h>
#include <sys/types.h>
#include <netinet/in.h>         // network types
#include <arpa/inet.h>          // arpa types
#endif

#include <time.h>

#include <iostream>
#include <string>
#include <sstream>
#include <string.h>
#include "dboperator.hpp"

using namespace std;
using namespace oracle::occi;

DbOperator::DbOperator(DbParam* pDbPar, ThreadLog*	pLog, int nIndex)
{
	m_pDbpar			= pDbPar;
	m_pLog				= pLog;
	m_nSequence			= nIndex;
	snprintf(m_szTag, sizeof(m_szTag), "DBT%d", nIndex);
	m_status = IDLE;
	m_nReocrds = 0;
	m_start = 0;
	m_end = 0;
}

DbOperator::~DbOperator()
{
}
	
/*inline */vector<DataUnit>& DbOperator::GetData(void)
{
	return m_datalist;
}

DbOperator::enumDbStatus DbOperator::GetStatus(void) const
{
	return m_status;
}

int DbOperator::Indb(void)
{
	if(m_status == RUNNING)
	{
		m_pLog->error(m_szTag, "Previous thread is still running! Discard data and quit.");
		return -1;
	}
	m_status = RUNNING;

	time_t start_t, end_t;
	time_t current_t, table_t;
	struct tm current_tm;
	time(&start_t);
	m_start = start_t;
	
	Environment 	*penv = NULL ;
	Connection      *pconn= NULL;
    Statement       *pstmt= NULL;
    ResultSet 		*prs  = NULL;
	m_pLog->debug(m_szTag, "Connect oracle database...");
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
		m_pLog->debug(m_szTag, "Connect oracle database successfully.");
	}
    catch(SQLException& sqlex)
    {
    	m_pLog->error(m_szTag, "Exception thrown for connecting database!");
        m_pLog->error(m_szTag, sqlex.getMessage().c_str());

	    Environment::terminateEnvironment(penv);
    	m_end = time(NULL);
    	m_nReocrds = 0;
        m_status = IDLE;
        return -1;
    }
    catch (exception& ex)
    {
       	m_pLog->error(m_szTag, "Exception thrown for connecting database!");
    	m_pLog->error(m_szTag, ex.what());

	    Environment::terminateEnvironment(penv);
    	m_end = time(NULL);
    	m_nReocrds = 0;
    	m_status = IDLE;
        return -1;
    }
	
	time(&current_t);
	start_t = current_t;
	current_tm = *localtime(&current_t);
	
	// Set the time to the boundary of 5 mins.
	current_tm.tm_sec = 0;
	current_tm.tm_min /= 5;
	current_tm.tm_min *= 5;
	current_tm.tm_min += 5;
	current_t = mktime(&current_tm);
	
	char szTable[20];
	char szSql[1024];
	char szLog[256];
	uint uTables = 0;
	table_t = 0;
	map<DWORD,DWORD> routerid;
	
    try
    {
		m_pLog->debug(m_szTag, "Check the 5 miniute table in database...");
		snprintf(szTable, sizeof(szTable), "iptpc_nf_%u", current_t);
		snprintf(szSql, sizeof(szSql), "select max(itime_stamp),count(*) from iptcc_nf_time");

    	pstmt = pconn->createStatement(szSql);
        prs = pstmt->executeQuery();
        
        if(prs->next())
        {
        	table_t = prs->getUInt(1);
        	uTables = prs->getUInt(2);
        }
        pstmt->closeResultSet(prs);
        pconn->terminateStatement(pstmt);
 
        // If 5 minitues table is too much, stop indb to avoid fill the disk.
        if(uTables > m_pDbpar->tabResrved * 12) 
        {
        	snprintf(szLog, sizeof(szLog), "Raw tables exceed to %u, discard flow data!", uTables);
			m_pLog->error(m_szTag, szLog);
	       penv->terminateConnection(pconn);
	       Environment::terminateEnvironment(penv);
	       m_status = IDLE;
	       m_end = time(NULL);
	       return -1;
        }       
        // If no data in iptcc_nf_time or the table name is not the same with current table,
        // the new table shoudl be created and the status of iptcc_nf_time should be updated.
        if(uTables == 0 || table_t < current_t)
        {
			time(&start_t);
        	snprintf(szLog, sizeof(szLog), "Create new 5 minitue table: [%s].", szTable);
        	m_pLog->debug(m_szTag, szLog);

			// Create new 5 miniutes table
        	snprintf(szSql, sizeof(szSql), 
          		 "create table %s(flowid number(10),routerip number(10),int_id number(10),\
coltime number(10),flowtype number(2),flowversion number(2),srcaddr number(10),dstaddr number(10),\
input number(6),output number(6),packets number(10),octets number(10),srcport number(6),\
dstport number(6),prot number(3),srcas number(6),dstas number(6),srcmask number(2),dstmask number(2),\
srcprefix number(10),dstprefix number(10),tcpflags number(2),tos number(3),nexthop number(10))\
tablespace NF_PC nologging pctfree 10 initrans 1 maxtrans 255\
storage (initial 2M next 1M minextents 1 maxextents unlimited pctincrease 0)",
		        szTable);
	        pstmt = pconn->createStatement(szSql);
	        pstmt->executeUpdate();
	        pconn->terminateStatement(pstmt);

  			snprintf(szLog, sizeof(szLog), "Update iptcc_nf_time [%lu]=[0].", current_t);
        	m_pLog->debug(m_szTag, szLog);

	        // Insert the table record to iptcc_nf_time
	        snprintf(szSql, sizeof(szSql),
	        	"insert into iptcc_nf_time(itime_stamp, sum_flag) values(%lu, 0)", 
	        	current_t);
	       	pstmt = pconn->createStatement(szSql);
	        pstmt->executeUpdate();
	        pconn->commit();
	        pconn->terminateStatement(pstmt);

	        time(&end_t);
  			snprintf(szLog, sizeof(szLog), 
  				"Creating table cost %d seconds.",
  				(int)difftime(end_t, start_t));
        	m_pLog->debug(m_szTag, szLog);
        } // if(uTables == 0 || table_t < current_t)
        
        m_pLog->debug(m_szTag, "Get router and interface configuration...");
		snprintf(szSql, sizeof(szSql), "select if_ipaddr,router_id from iptca_nf_interface");

    	pstmt = pconn->createStatement(szSql);
        prs = pstmt->executeQuery();
        
        while(prs->next())
        {
        	routerid.insert(pair<DWORD,DWORD>(prs->getUInt(1), prs->getUInt(2)));
        }
        pstmt->closeResultSet(prs);
        pconn->terminateStatement(pstmt);
		snprintf(szLog, sizeof(szLog), "Get %d records from iptca_nf_interface.", routerid.size());        
        m_pLog->debug(m_szTag, szLog);
    }
    catch(SQLException& sqlex)
    {
    	m_pLog->error(m_szTag, "Exception thrown form getting database configuration!");
        m_pLog->error(m_szTag, sqlex.getMessage().c_str());
        m_pLog->error(m_szTag, szSql);
	    
        penv->terminateConnection(pconn);
    	Environment::terminateEnvironment(penv);
    	m_end = time(NULL);
    	m_nReocrds = 0;
        m_status = IDLE;
        return -1;
    }
    catch (exception& ex)
    {
       	m_pLog->error(m_szTag, "Exception thrown form getting database configuration!");
    	m_pLog->error(m_szTag, ex.what());

    	penv->terminateConnection(pconn);
    	Environment::terminateEnvironment(penv);

    	m_end = time(NULL);
    	m_nReocrds = 0;
		m_status = IDLE;
        return -1;
   	}

	m_pLog->debug(m_szTag, "Begin insert data...");
    time(&start_t);
    try
    {
    	if(m_pDbpar->mode == DbParam::INNER)
    	{
	    	snprintf(szSql, sizeof(szSql),
	    		"insert into /*+ APPEND */ %s values\
	    		(:v1,:v2,:v3,:v4,:v5,:v6,:v7,:v8,:v9,:v10,:v11,:v12,\
	    		:v13,:v14,:v15,:v16,:v17,:v18,:v19,:v20,:v21,:v22,:v23,:v24)",
	    		szTable);
	    	pstmt = pconn->createStatement(szSql);
	    	vector<DataUnit>::const_iterator itr;
	    	for(itr = m_datalist.begin(); itr != m_datalist.end(); itr++)
	        {
	            pstmt->setUInt( 1, itr->flowid      );
	            pstmt->setUInt( 2, itr->routerip    );
	            pstmt->setUInt( 3, routerid[itr->routerip]);
	            pstmt->setUInt( 4, itr->coltime     );
	            pstmt->setUInt( 5, itr->flowtype    );
	            pstmt->setUInt( 6, itr->flowversion );
	            pstmt->setUInt( 7, itr->srcaddr     );
	            pstmt->setUInt( 8, itr->dstaddr     );
	            pstmt->setUInt( 9, itr->input       );
	            pstmt->setUInt(10, itr->output      );
	            pstmt->setUInt(11, itr->packets     );
	            pstmt->setUInt(12, itr->octets      );
	            pstmt->setUInt(13, itr->srcport     );
	            pstmt->setUInt(14, itr->dstport     );
	            pstmt->setUInt(15, itr->protocol    );
	            pstmt->setUInt(16, itr->srcas       );
	            pstmt->setUInt(17, itr->dstas       );
	            pstmt->setUInt(18, itr->srcmask     );
	            pstmt->setUInt(19, itr->dstmask     );
	            pstmt->setUInt(20, itr->srcprefix   );
	            pstmt->setUInt(21, itr->dstprefix   );
	            pstmt->setUInt(22, itr->tcpflags    );
	            pstmt->setUInt(23, itr->tos         );
	            pstmt->setUInt(24, itr->nexthop     );
	
	            pstmt->executeUpdate();
	        }
	        pconn->commit();
	        pconn->terminateStatement(pstmt);
    	}
		else // Use sqlldr to import flow data
		{
            char szDataFile[128], szCtrlFile[128], szLogFile[128], szBadFile[128];
			char szBuff[512];
			#ifdef WIN32
			snprintf(szDataFile, sizeof(szDataFile), "%s\\flow.%d", m_pDbpar->workDir.c_str(), m_nSequence);
			snprintf(szCtrlFile, sizeof(szCtrlFile), "%s\\flow.ctl", m_pDbpar->workDir.c_str());
			snprintf(szLogFile, sizeof(szLogFile), "%s\\flow.log", m_pDbpar->workDir.c_str());
			snprintf(szBadFile, sizeof(szBadFile), "%s\\flow.bad", m_pDbpar->workDir.c_str());
			#else
			snprintf(szDataFile, sizeof(szDataFile), "%s/flow.%d", m_pDbpar->workDir.c_str(), m_nSequence);
			snprintf(szCtrlFile, sizeof(szCtrlFile), "%s/flow.ctl", m_pDbpar->workDir.c_str());
			snprintf(szLogFile, sizeof(szLogFile), "%s/flow.log", m_pDbpar->workDir.c_str());
			snprintf(szBadFile, sizeof(szBadFile), "%s/flow.bad", m_pDbpar->workDir.c_str());
			#endif
			
			ofstream fctl(szCtrlFile);
			snprintf(szBuff, sizeof(szBuff),
				"load data infile '%s'\n\
append into table %s\nfields terminated by ','\ntrailing nullcols\n(flowid,routerip,int_id,coltime,flowtype,\
flowversion,srcaddr,dstaddr,input,output,packets,octets,srcport,dstport,prot,srcas,dstas,srcmask,dstmask,\
srcprefix,dstprefix,tcpflags,tos,nexthop)",
				szDataFile, szTable);
			fctl << szBuff;
			fctl.close();
			
			ofstream fdat(szDataFile);
	    	vector<DataUnit>::const_iterator itr;
			for(itr = m_datalist.begin(); itr != m_datalist.end(); itr++)
            {
                snprintf(szBuff, sizeof(szBuff), 
                "%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u\n",
                itr->flowid,
                itr->routerip,
                routerid[itr->routerip],
                itr->coltime,
                itr->flowtype,
                itr->flowversion,
                itr->srcaddr,
                itr->dstaddr,
                itr->input,
                itr->output,
                itr->packets,
                itr->octets,
                itr->srcport,
                itr->dstport,
                itr->protocol,
                itr->srcas,
                itr->dstas,
                itr->srcmask,
                itr->dstmask,
                itr->srcprefix,
                itr->dstprefix,
                itr->tcpflags,
                itr->tos,
                itr->nexthop);
                fdat << szBuff;
            }
            fdat.close();

     		#ifdef WIN32
            snprintf(szBuff, sizeof(szBuff),
            	"%s\\bin\\sqlldr %s/%s@%s control=%s log=%s bad=%s direct=TRUE",
				getenv("ORACLE_HOME"),
				m_pDbpar->dbUser.c_str(),
				m_pDbpar->dbPassword.c_str(),
				m_pDbpar->dbInstance.c_str(),
				szCtrlFile,
				szLogFile,
				szBadFile);
     		#else
            snprintf(szBuff, sizeof(szBuff),
            	"%s/bin/sqlldr %s/%s@%s control=%s log=%s bad=%s direct=TRUE",
				getenv("ORACLE_HOME"),
				m_pDbpar->dbUser.c_str(),
				m_pDbpar->dbPassword.c_str(),
				m_pDbpar->dbInstance.c_str(),
				szCtrlFile,
				szLogFile,
				szBadFile);
           	#endif
           	
           	#ifdef WIN32
           	STARTUPINFO si;
		    PROCESS_INFORMATION pi;
		
		    ZeroMemory(&si, sizeof(si) );
		    si.cb = sizeof(si);
		    si.dwFlags = STARTF_USESHOWWINDOW;
		    si.wShowWindow = SW_HIDE;
		    ZeroMemory(&pi, sizeof(pi) );

           	// Start the sqlldr process. 
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
		        m_pLog->error(m_szTag, szBuff);
		        throw runtime_error("Create sqlldr process failed!");
		    }
		
		    // Wait until child process exits.
		    WaitForSingleObject(pi.hProcess, INFINITE );
		
		    // Close process and thread handles. 
		    CloseHandle(pi.hProcess );
		    CloseHandle(pi.hThread );
           	#else
     		FILE *fsqlout;
     		int nLineChar;
     		if((fsqlout = popen(szBuff, "r")) != NULL)
     		{
     			while(fgets(szBuff, sizeof(szBuff), fsqlout) != NULL)
     			{
     				if((nLineChar = strlen(szBuff)) > 1)
     				{
     					szBuff[nLineChar - 1] = '\0';
     					m_pLog->debug(m_szTag, szBuff);
     				}
     			}
     			pclose(fsqlout);
     		}
     		else
     		{
		        m_pLog->error(m_szTag, szBuff);
		        throw runtime_error("Create sqlldr process failed!");
     		}
     		#endif
		} // if(m_pDbpar->mode == DbParam::INNER)
	    
		time(&end_t);
		snprintf(szLog, sizeof(szLog), 
			"Inserting %u records costs %d seconds.", 
			m_datalist.size(), (int)difftime(end_t, start_t));
	    m_pLog->debug(m_szTag, szLog);
    }
    catch(SQLException& sqlex)
    {
    	m_pLog->error(m_szTag, "Exception thrown for insert data!");
        m_pLog->error(m_szTag, sqlex.getMessage().c_str());
    }
    catch (exception& ex)
    {
       	m_pLog->error(m_szTag, "Exception thrown for insert data!");
    	m_pLog->error(m_szTag, ex.what());
    }

    try
    {
    	penv->terminateConnection(pconn);
    	Environment::terminateEnvironment(penv);
    }
    catch(SQLException& sqlex)
    {
    	m_pLog->error(m_szTag, "Exception thrown for disconnecting database!");
        m_pLog->error(m_szTag, sqlex.getMessage().c_str());
    }
    catch (exception& ex)
    {
       	m_pLog->error(m_szTag, "Exception thrown for disconnecting database!");
    	m_pLog->error(m_szTag, ex.what());
    }
    
    m_end = end_t;
    m_nReocrds = m_datalist.size();
    m_status = IDLE;
	return 0;
}
