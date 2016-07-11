#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main(int argc, char* argv[])
{
  int               i,j;
  unsigned   int    dwSN, dwKey;
  unsigned   int    nYear, nMonth, nDay;
  unsigned   int    nDuration;
  unsigned   char   c,szKey[3][9];
  struct     tm     tmBegin, tmEnd;
  time_t            tBegin, tEnd;
  unsigned   char   szBegin[20],szEnd[20];

  if(argc == 5 && argv[1][1]=='c')
  {
  	sscanf(argv[2],"%8lX",&dwSN);
  	sscanf(argv[3],"%04d-%02d-%02d",&nYear, &nMonth, &nDay);
  	sscanf(argv[4],"%d",&nDuration);
    
    dwSN=~dwSN;
    dwSN^=0x13572468;
    
  	tmBegin.tm_sec = 0;
		tmBegin.tm_min = 0;
		tmBegin.tm_hour = 0;
		tmBegin.tm_mday = nDay;
		tmBegin.tm_mon = nMonth - 1;
		tmBegin.tm_year = nYear - 1900;
		tBegin = mktime(&tmBegin);
		tEnd = tBegin + nDuration * 24 * 3600;
		
		tBegin=~tBegin;
		tBegin^=0x24681357;
		tEnd=~tEnd;
		tEnd^=0x12345678;
		
		sprintf(szKey[0], "%08lX", dwSN);
		sprintf(szKey[1], "%08lX", tBegin);
		sprintf(szKey[2], "%08lX", tEnd);

		for(i=0;i<2;i++)
		{
			for(j=0;j<8;j+=2)
			{
				c=szKey[0][i+j];
				szKey[0][i+j]=szKey[i+1][i+j];
				szKey[i+1][i+j]=c;
			}
		}
		
    printf("该用户的密匙是   %s-%s-%s\n",szKey[0],szKey[1],szKey[2]);
    exit(0);
  }

  if(argc == 4 && argv[1][1]=='t')
  {
		sscanf(argv[2],"%8lX",&dwSN);
		sscanf(argv[3],"%8s-%8s-%8s",szKey[0],szKey[1],szKey[2]);
		
		for(i=0;i<2;i++)
		{
			for(j=0;j<8;j+=2)
			{
				c=szKey[0][i+j];
				szKey[0][i+j]=szKey[i+1][i+j];
				szKey[i+1][i+j]=c;
			}
		}

		sscanf(szKey[0],"%8lX",&dwKey);
		sscanf(szKey[1],"%8lX",&tBegin);
		sscanf(szKey[2],"%8lX",&tEnd);
		
		dwKey=~dwKey;
		dwKey^=0x13572468;
		tBegin=~tBegin;
		tBegin^=0x24681357;
		tEnd=~tEnd;
		tEnd^=0x12345678;
		
		tmBegin=*localtime(&tBegin);
		tmEnd=*localtime(&tEnd);
		sprintf(szBegin, "%04d-%02d-%02d", tmBegin.tm_year + 1900, tmBegin.tm_mon + 1, tmBegin.tm_mday);
		sprintf(szEnd, "%04d-%02d-%02d", tmEnd.tm_year + 1900, tmEnd.tm_mon + 1, tmEnd.tm_mday);
		if(dwSN == dwKey)
			printf("有效License，开始时间：%s 结束时间：%s.\n", szBegin, szEnd);
		else
			printf("无效License!\n");
  	exit(0);
  }


  printf("Usage:   %s [-c|-t] id [<starttime duration>|key]\n", argv[0]);
  printf("Option:  -c|-t\n");
  printf("         -c   to create license key. MUST provide id, starttime and duration.\n");
  printf("         -t   to test license key. MUST provide id and key.\n");
  printf("Examples: \n");
  printf("         1. Create license for id=882E13DC, valid time from 2007-07-01 to 2007-07-30\n");
  printf("            %s -c 882E13DC 2007-07-01 30\n", argv[0]);
  printf("         2. Test license for id=882E13DC, key=9B1592A7-6D81C248-A466A88B\n");
  printf("            %s -t 882E13DC 9B1592A7-6D81C248-A466A88B\n", argv[0]);
  exit(-1);
}
