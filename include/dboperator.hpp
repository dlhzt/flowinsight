//=============================================================================
// File:          dboperator.hpp
// Author:        Zhaojunwei
// CreateTime:    2004/08/10
// Descripiton:   Head file for storing flow data in database.
// ----------------------------------------------------------------------------
// Version:
//         2004/08/10   1.0 First work release.
//         2005/04/04   1.1 Modify the database from postgres to oracle.
//=============================================================================

#ifndef DBOPERATOR_HPP
#define DBOPERATOR_HPP

#include <stdexcept>

#include <tocci.h>
#include <threadlog.hpp>
#include "flowcollector.hpp"

const int DB_THREAD_NUM = 5;

struct DbParam
{
	enum DbInsertMode				// Insert mode, INNER for occi and
	{INNER, SQLLDR};				// SQLLDR for tools, sqlldr
	
	string			dbUser;			// User name
	string			dbPassword;		// Password
	string			dbInstance;		// Database name or service, SID

	string			workDir;		// Path for storing intermediat data
	DbInsertMode	mode;
	int             tabResrved;  // number of table reserved for retriving raw data
};

class DbOperator 
{
public:
	enum enumDbStatus
	{RUNNING, IDLE};					// Flag denotes the status of method dbOperation
	
public:
	DbOperator(DbParam* pDbPar,
		ThreadLog* pLog,
		int nIndex);
	~DbOperator();
	
	int 				Indb(void);
	vector<DataUnit>& 	GetData(void);
	enumDbStatus		GetStatus(void) const;
	time_t				GetStart(void) const
						{return m_start;};
	time_t				GetEnd(void) const
						{return m_end;};
	int					GetRecords(void) const
						{return m_nReocrds;};
	
private:
	DbParam*			m_pDbpar;		// Database operator configuration parameter
	enumDbStatus		m_status;		// Running status of thread
	vector<DataUnit>	m_datalist;		// All flow record array
	
	ThreadLog*			m_pLog;			// Log handler to record running info.
	int					m_nSequence;	// Index of deoperator.
	char				m_szTag[8];		// Tag of dboperator instance. 
										// For distincte itself to other instance.
	time_t				m_start, m_end; // For performance stat.
	int					m_nReocrds;
};

#endif // DBOPERATOR_HPP
