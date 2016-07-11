#include   <windows.h>   
#include   <winbase.h>   
#include   <stdio.h>
#include   <time.h>
  
DWORD GetDiskSerialNo(void)   
{   
LPCTSTR   lpRootPathName="c:\\";
char      VolumeNameBuffer[12];
DWORD   nVolumeNameSize=12;   
  
DWORD   VolumeSerialNumber;
DWORD   MaximumComponentLength;   
char    FileSystemNameBuffer[10];   
DWORD   nFileSystemNameSize=10;   
DWORD   FileSystemFlags;   
GetVolumeInformation(lpRootPathName,   
		VolumeNameBuffer,   nVolumeNameSize,   
		&VolumeSerialNumber,   &MaximumComponentLength,   
		&FileSystemFlags,   
		FileSystemNameBuffer,   nFileSystemNameSize);   
VolumeSerialNumber^=0x93939393;
return     VolumeSerialNumber;   
}   
  
DWORD CreateLock(void)   
{   
return GetDiskSerialNo();   
}   
  
DWORD TestKey(char *sSN, char* msg)   
{   
	int               i,j;
	unsigned   long   dwSN,dwKey;
  char              c,szKey[3][9];
  time_t            tBegin, tEnd, tNow;
  struct     tm     tmBegin, tmEnd;
  char              szBegin[20], szEnd[20];
  
	if(strlen(sSN)==0){return 1;}   
	
	sscanf(sSN,"%8s-%8s-%8s",szKey[0],szKey[1],szKey[2]);
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
	
	dwSN = CreateLock();
	time(&tNow);
	
	if(dwSN != dwKey)
		return 1;
	
	if(tNow > tEnd || tNow < tBegin)
		return 2;
	
	if(msg)
	{
		tmBegin=*localtime(&tBegin);
		tmEnd=*localtime(&tEnd);
		sprintf(szBegin, "%04d-%02d-%02d", tmBegin.tm_year + 1900, tmBegin.tm_mon + 1, tmBegin.tm_mday);
		sprintf(szEnd, "%04d-%02d-%02d", tmEnd.tm_year + 1900, tmEnd.tm_mon + 1, tmEnd.tm_mday);
		sprintf(msg, "Valid license. From %s to %s.", szBegin, szEnd);
	}
	   
  return   0;   
}   
  
