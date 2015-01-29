// Copyright 2013 CSIRO  ( http://www.csiro.au/ ) 
//   Licensed under the Apache License, Version 2.0 (the "License");
//   you may not use this file except in compliance with the License.
//   You may obtain a copy of the License at
//       http://www.apache.org/licenses/LICENSE-2.0
//
//   Unless required by applicable law or agreed to in writing, software
//   distributed under the License is distributed on an "AS IS" BASIS,
//   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//   See the License for the specific language governing permissions and
//   limitations under the License
//   Please contact stuart.stephen@csiro.au for support or 
//   to submit modifications to this source
#include "stdafx.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#if _WIN32
#include <process.h>
#include "../libbiokanga/commhdrs.h"
#else
#include <sys/mman.h>
#include <pthread.h>
#include "../libbiokanga/commhdrs.h"
#endif

#include "pacbiokanga.h"

#include "../libbiokanga/bgzf.h"
//#include "./Kangadna.h"
#include "../libbiokanga/SSW.h"
#include "PBFilter.h"


int
ProcPacBioFilter(etPBPMode PMode,	// processing mode
		double MaxScore,			// only accept reads with scores <= this score
		int MinSeedCoreLen,			// use seed cores of this length when identifying putative antisense subsequences indicative of SMRTBell read past onto antisense
		int MaxCoreSeparation,      // max core separation between cores in the same cluster
		int MinClustLen,			// any putative sense overlap with antisense subsequence cluster must be of at least this length
		int Trim5,					// 5' trim accepted reads by this many bp
		int Trim3,					// 3' trim accepted reads by this many bp
		int MinReadLen,				// read sequences must be at least this length after any end timming
		char *pszInputFile,			// name of input file containing reads to be filtered
		char *pszOutFile,			// name of file in which to write filter accepted and trimmed sequences
		int NumThreads);			// maximum number of worker threads to use


#ifdef _WIN32
int ProcFilter(int argc, char* argv[])
{
// determine my process name
_splitpath(argv[0],NULL,NULL,gszProcName,NULL);
#else
int
ProcFilter(int argc, char** argv)
{
// determine my process name
CUtility::splitpath((char *)argv[0],NULL,gszProcName);
#endif

int iFileLogLevel;			// level of file diagnostics
int iScreenLogLevel;		// level of file diagnostics
char szLogFile[_MAX_PATH];	// write diagnostics to this file
int Rslt = 0;   			// function result code >= 0 represents success, < 0 on failure

int PMode;					// processing mode
double MaxScore;			// only accept reads with scores <= this score
int MinSeedCoreLen;			// use seed cores of this length when identifying putative antisense subsequences indicative of SMRTBell read past onto antisense
int MaxCoreSeparation;      // max core separation between cores in the same cluster
int MinClustLen;			// any putative sense overlap with antisense subsequence cluster must be of at least this length
int Trim5;					// 5' trim accepted reads by this many bp
int Trim3;					// 3' trim accepted reads by this many bp
int MinReadLen;				// read sequences must be at least this length after any end trimming
char szInputFile[_MAX_PATH]; // name of input file containing reads to be filtered
char szOutFile[_MAX_PATH];	// name of file in which to write filter accepted and trimmed sequences

int NumberOfProcessors;		// number of installed CPUs
int NumThreads;				// number of threads (0 defaults to number of CPUs)

char szSQLiteDatabase[_MAX_PATH];	// results summaries to this SQLite file
char szExperimentName[cMaxDatasetSpeciesChrom+1];			// experiment name
char szExperimentDescr[1000];		// describes experiment

struct arg_lit  *help    = arg_lit0("h","help",                 "print this help and exit");
struct arg_lit  *version = arg_lit0("v","version,ver",			"print version information and exit");
struct arg_int *FileLogLevel=arg_int0("f", "FileLogLevel",		"<int>","Level of diagnostics written to screen and logfile 0=fatal,1=errors,2=info,3=diagnostics,4=debug");
struct arg_file *LogFile = arg_file0("F","log","<file>",		"diagnostics log file");

struct arg_int *pmode = arg_int0("m","pmode","<int>",			"processing mode - 0 default");
struct arg_dbl *maxscore = arg_dbl0("s","maxscore","<dbl>",		"only accept reads with scores <= this score (default 0.1, range 0.01 to 1000.0)");

struct arg_int *minseedcorelen = arg_int0("c","minseedcorelen","<int>",	"use seed cores of this length for SMRTBell hairpin read through detection (default 10, 0 to disable, range 8 to 12)");
struct arg_int *maxcoreseparation = arg_int0("C","maxcoreseparation","<int>",	"max core separation between cores in the same cluster (default 700, range 50 to 2000)");

struct arg_int *minclustlen = arg_int0("L","minclustlen","<int>",	"any putative sense overlap with antisense subsequence cluster must be of at least this length bp (default 1000, range 500 to 10000");
struct arg_int *trim5 = arg_int0("z","trim5","<int>",			"5' trim accepted reads by this many bp (default 250, range 0 to 10000)");
struct arg_int *trim3 = arg_int0("Z","trim3","<int>",			"3' trim accepted reads by this many bp (default 250, range 0 to 10000)");
struct arg_int *minreadlen = arg_int0("l","minreadlen","<int>",		"read sequences must be at least this length after any end trimming (default 5000, range 1000 to 20000)");

struct arg_file *inputfile = arg_file1("i","in","<file>",		"input file containing PacBio long reads to be filtered");
struct arg_file *outfile = arg_file1("o","out","<file>",			"output accepted filtered reads to this file");

struct arg_int *threads = arg_int0("T","threads","<int>",		"number of processing threads 0..n (defaults to 0 which sets threads to number of CPU cores, max 64)");

struct arg_file *summrslts = arg_file0("q","sumrslts","<file>",				"Output results summary to this SQLite3 database file");
struct arg_str *experimentname = arg_str0("w","experimentname","<str>",		"experiment name SQLite3 database file");
struct arg_str *experimentdescr = arg_str0("W","experimentdescr","<str>",	"experiment description SQLite3 database file");

struct arg_end *end = arg_end(200);

void *argtable[] = {help,version,FileLogLevel,LogFile,
					pmode,maxscore,minseedcorelen,maxcoreseparation,
					minclustlen,trim5,
					trim3,minreadlen,summrslts,experimentname,experimentdescr,
					inputfile,outfile,threads,
					end};

char **pAllArgs;
int argerrors;
argerrors = CUtility::arg_parsefromfile(argc,(char **)argv,&pAllArgs);
if(argerrors >= 0)
	argerrors = arg_parse(argerrors,pAllArgs,argtable);

/* special case: '--help' takes precedence over error reporting */
if (help->count > 0)
        {
		printf("\n%s %s %s, Version %s\nOptions ---\n", gszProcName,gpszSubProcess->pszName,gpszSubProcess->pszFullDescr,cpszProgVer);
        arg_print_syntax(stdout,argtable,"\n");
        arg_print_glossary(stdout,argtable,"  %-25s %s\n");
		printf("\nNote: Parameters can be entered into a parameter file, one parameter per line.");
		printf("\n      To invoke this parameter file then precede it's name with '@'");
		printf("\n      e.g. %s %s @myparams.txt\n",gszProcName,gpszSubProcess->pszName);
		printf("\nPlease report any issues regarding usage of %s to stuart.stephen@csiro.au\n\n",gszProcName);
		return(1);
        }

    /* special case: '--version' takes precedence error reporting */
if (version->count > 0)
        {
		printf("\n%s %s Version %s\n",gszProcName,gpszSubProcess->pszName,cpszProgVer);
		return(1);
        }

if (!argerrors)
	{
	if(FileLogLevel->count && !LogFile->count)
		{
		printf("\nError: FileLogLevel '-f%d' specified but no logfile '-F<logfile>\n'",FileLogLevel->ival[0]);
		exit(1);
		}

	iScreenLogLevel = iFileLogLevel = FileLogLevel->count ? FileLogLevel->ival[0] : eDLInfo;
	if(iFileLogLevel < eDLNone || iFileLogLevel > eDLDebug)
		{
		printf("\nError: FileLogLevel '-l%d' specified outside of range %d..%d\n",iFileLogLevel,eDLNone,eDLDebug);
		exit(1);
		}

	if(LogFile->count)
		{
		strncpy(szLogFile,LogFile->filename[0],_MAX_PATH);
		szLogFile[_MAX_PATH-1] = '\0';
		}
	else
		{
		iFileLogLevel = eDLNone;
		szLogFile[0] = '\0';
		}

	// now that log parameters have been parsed then initialise diagnostics log system
	if(!gDiagnostics.Open(szLogFile,(etDiagLevel)iScreenLogLevel,(etDiagLevel)iFileLogLevel,true))
		{
		printf("\nError: Unable to start diagnostics subsystem\n");
		if(szLogFile[0] != '\0')
			printf(" Most likely cause is that logfile '%s' can't be opened/created\n",szLogFile);
		exit(1);
		}

	gDiagnostics.DiagOut(eDLInfo,gszProcName,"Subprocess %s Version %s starting",gpszSubProcess->pszName,cpszProgVer);
	gExperimentID = 0;
	gProcessID = 0;
	gProcessingID = 0;
	szSQLiteDatabase[0] = '\0';
	szExperimentName[0] = '\0';
	szExperimentDescr[0] = '\0';


	if(experimentname->count)
		{
		strncpy(szExperimentName,experimentname->sval[0],sizeof(szExperimentName));
		szExperimentName[sizeof(szExperimentName)-1] = '\0';
		CUtility::TrimQuotedWhitespcExtd(szExperimentName);
		CUtility::ReduceWhitespace(szExperimentName);
		}
	else
		szExperimentName[0] = '\0';

	gExperimentID = 0;
	gProcessID = 0;
	gProcessingID = 0;
	szSQLiteDatabase[0] = '\0';
	szExperimentDescr[0] = '\0';

	if(summrslts->count)
		{
		strncpy(szSQLiteDatabase,summrslts->filename[0],sizeof(szSQLiteDatabase)-1);
		szSQLiteDatabase[sizeof(szSQLiteDatabase)-1] = '\0';
		CUtility::TrimQuotedWhitespcExtd(szSQLiteDatabase);
		if(strlen(szSQLiteDatabase) < 1)
			{
			gDiagnostics.DiagOut(eDLFatal,gszProcName,"Error: After removal of whitespace, no SQLite database specified with '-q<filespec>' option");
			return(1);
			}

		if(strlen(szExperimentName) < 1)
			{
			gDiagnostics.DiagOut(eDLFatal,gszProcName,"Error: After removal of whitespace, no SQLite experiment name specified with '-w<str>' option");
			return(1);
			}
		if(experimentdescr->count)
			{
			strncpy(szExperimentDescr,experimentdescr->sval[0],sizeof(szExperimentDescr)-1);
			szExperimentDescr[sizeof(szExperimentDescr)-1] = '\0';
			CUtility::TrimQuotedWhitespcExtd(szExperimentDescr);
			}
		if(strlen(szExperimentDescr) < 1)
			{
			gDiagnostics.DiagOut(eDLFatal,gszProcName,"Error: After removal of whitespace, no SQLite experiment description specified with '-W<str>' option");
			return(1);
			}

		gExperimentID = gSQLiteSummaries.StartExperiment(szSQLiteDatabase,false,true,szExperimentName,szExperimentName,szExperimentDescr);
		if(gExperimentID < 1)
			return(1);
		gProcessID = gSQLiteSummaries.AddProcess((char *)gpszSubProcess->pszName,(char *)gpszSubProcess->pszName,(char *)gpszSubProcess->pszFullDescr);
		if(gProcessID < 1)
			return(1);
		gProcessingID = gSQLiteSummaries.StartProcessing(gExperimentID,gProcessID,(char *)cpszProgVer);
		if(gProcessingID < 1)
			return(1);
		gDiagnostics.DiagOut(eDLInfo,gszProcName,"Initialised SQLite database '%s' for results summary collection",szSQLiteDatabase);
		gDiagnostics.DiagOut(eDLInfo,gszProcName,"SQLite database experiment identifier for '%s' is %d",szExperimentName,gExperimentID);
		gDiagnostics.DiagOut(eDLInfo,gszProcName,"SQLite database process identifier for '%s' is %d",(char *)gpszSubProcess->pszName,gProcessID);
		gDiagnostics.DiagOut(eDLInfo,gszProcName,"SQLite database processing instance identifier is %d",gProcessingID);
		}
	else
		{
		szSQLiteDatabase[0] = '\0';
		szExperimentDescr[0] = '\0';
		}

	PMode = (etPBPMode)(pmode->count ? pmode->ival[0] : (int)ePBPMFilter);
	if(PMode < ePBPMFilter || PMode > ePBPMFilter)
		{
		gDiagnostics.DiagOut(eDLFatal,gszProcName,"Error: Processing mode '-m%d' must be in range 0..%d",PMode,ePBPMFilter);
		return(1);
		}

	MaxScore = maxscore->count ? maxscore->dval[0] : 0.1;
	if(MaxScore < 0.01 || MaxScore > 1000.0)
		{
		gDiagnostics.DiagOut(eDLFatal,gszProcName,"Error: max accepted score '-s%f' must be in range 0.01..1000.0",MaxScore);
		return(1);
		}

	MinSeedCoreLen = minseedcorelen->count ? minseedcorelen->ival[0] : cDfltFiltSeedCoreLen;
	if(MinSeedCoreLen != 0 && (MinSeedCoreLen < cMinFiltSeedCoreLen || MinSeedCoreLen > cMaxFiltSeedCoreLen))
		{
		gDiagnostics.DiagOut(eDLFatal,gszProcName,"Error: seed core length '-c%d' must be 0 to disable or in range %d..%d",MinSeedCoreLen,cMinFiltSeedCoreLen,cMaxFiltSeedCoreLen);
		return(1);
		}

	if(MinSeedCoreLen > 0)
		{
		MaxCoreSeparation = maxcoreseparation->count ? maxcoreseparation->ival[0] : cDfltCoreSeparation;
		if(MaxCoreSeparation < 50 || MaxCoreSeparation > 2000)
			{
			gDiagnostics.DiagOut(eDLFatal,gszProcName,"Error: max core separation between cores in the same cluster '-C%d' must be in range 50..2000",MaxCoreSeparation);
			return(1);
			}

		MinClustLen = minclustlen->count ? minclustlen->ival[0] : cDfltMinClustLen;
		if(MinClustLen < 500 || MinClustLen > 10000)
			{
			gDiagnostics.DiagOut(eDLFatal,gszProcName,"Error: minimum cluster length '-L%d' must be in range 500..10000",MinClustLen);
			return(1);
			}
		}
	else
		{
		MaxCoreSeparation = 0;
		MinClustLen = 0;
		}

	Trim5 = trim5->count ? trim5->ival[0] : cDfltTrim5;
	if(Trim5 < 0 || Trim5 > 10000)
		{
		gDiagnostics.DiagOut(eDLFatal,gszProcName,"Error: 5' end trim '-z%d' must be in range 0..10000",Trim5);
		return(1);
		}

	Trim3 = trim3->count ? trim3->ival[0] : cDfltTrim3;
	if(Trim3 < 0 || Trim3 > 10000)
		{
		gDiagnostics.DiagOut(eDLFatal,gszProcName,"Error: 3' end trim '-Z%d' must be in range 0..10000",Trim3);
		return(1);
		}

	MinReadLen = minreadlen->count ? minreadlen->ival[0] : cDfltMinReadLen;
	if(MinReadLen < 1000 || MinReadLen > 20000)
		{
		gDiagnostics.DiagOut(eDLFatal,gszProcName,"Error: minimum read length after any trim '-l%d' must be in range 1000..20000",MinReadLen);
		return(1);
		}

	strcpy(szInputFile,inputfile->filename[0]);
	strcpy(szOutFile,outfile->filename[0]);

#ifdef _WIN32
	SYSTEM_INFO SystemInfo;
	GetSystemInfo(&SystemInfo);
	NumberOfProcessors = SystemInfo.dwNumberOfProcessors;
#else
	NumberOfProcessors = sysconf(_SC_NPROCESSORS_CONF);
#endif
	int MaxAllowedThreads = min(cMaxWorkerThreads,NumberOfProcessors);	// limit to be at most cMaxWorkerThreads
	if((NumThreads = threads->count ? threads->ival[0] : MaxAllowedThreads)==0)
		NumThreads = MaxAllowedThreads;
	if(NumThreads < 0 || NumThreads > MaxAllowedThreads)
		{
		gDiagnostics.DiagOut(eDLWarn,gszProcName,"Warning: Number of threads '-T%d' specified was outside of range %d..%d",NumThreads,1,MaxAllowedThreads);
		gDiagnostics.DiagOut(eDLWarn,gszProcName,"Warning: Defaulting number of threads to %d",MaxAllowedThreads);
		NumThreads = MaxAllowedThreads;
		}

	gDiagnostics.DiagOut(eDLInfo,gszProcName,"Processing parameters:");

	char *pszMode;
	switch(PMode) {
		case ePBPMFilter:									// identify PacBio reads which have retained hairpins
			pszMode = (char *)"Filter reads";
			break;
	}

	gDiagnostics.DiagOutMsgOnly(eDLInfo,"processing mode: '%s'",pszMode);

	gDiagnostics.DiagOutMsgOnly(eDLInfo,"accepting reads with maximum score: %f",MaxScore);
	gDiagnostics.DiagOutMsgOnly(eDLInfo,"use seed cores of this length for SMRTBell hairpin read through detection: %dbp",MinSeedCoreLen);
	gDiagnostics.DiagOutMsgOnly(eDLInfo,"max core separation between cores in the same cluster: %dbp",MaxCoreSeparation);

	gDiagnostics.DiagOutMsgOnly(eDLInfo,"sense overlap with antisense subsequence cluster must be of at least this length: %dbp",MinClustLen);
	gDiagnostics.DiagOutMsgOnly(eDLInfo,"5' trim accepted reads by: %dbp",Trim5);
	gDiagnostics.DiagOutMsgOnly(eDLInfo,"3' trim accepted reads by: %dbp",Trim3);
	gDiagnostics.DiagOutMsgOnly(eDLInfo,"read sequences must be at least this length after any end trimming: %dbp",MinReadLen);

	gDiagnostics.DiagOutMsgOnly(eDLInfo,"name of input file containing reads to be filtered: '%s'",szInputFile);
	gDiagnostics.DiagOutMsgOnly(eDLInfo,"name of file in which to write filter accepted and trimmed sequences: '%s'",szOutFile);

	if(szExperimentName[0] != '\0')
		gDiagnostics.DiagOutMsgOnly(eDLInfo,"This processing reference: %s",szExperimentName);

	gDiagnostics.DiagOutMsgOnly(eDLInfo,"number of threads : %d",NumThreads);

	if(gExperimentID > 0)
		{
		int ParamID;
		ParamID = gSQLiteSummaries.AddParameter(gProcessingID,ePTText,(int)strlen(szLogFile),"log",szLogFile);

		ParamID = gSQLiteSummaries.AddParameter(gProcessingID,ePTInt32,(int)sizeof(PMode),"pmode",&PMode);
		ParamID = gSQLiteSummaries.AddParameter(gProcessingID,ePTDouble,(int)sizeof(MaxScore),"maxscore",&MaxScore);
		ParamID = gSQLiteSummaries.AddParameter(gProcessingID,ePTInt32,(int)sizeof(MinSeedCoreLen),"minseedcorelen",&MinSeedCoreLen);
		ParamID = gSQLiteSummaries.AddParameter(gProcessingID,ePTInt32,(int)sizeof(MaxCoreSeparation),"maxcoreseparation",&MaxCoreSeparation);

		ParamID = gSQLiteSummaries.AddParameter(gProcessingID,ePTInt32,(int)sizeof(MinClustLen),"minclustlen",&MinClustLen);
		ParamID = gSQLiteSummaries.AddParameter(gProcessingID,ePTInt32,(int)sizeof(Trim5),"trim5",&Trim5);
		ParamID = gSQLiteSummaries.AddParameter(gProcessingID,ePTInt32,(int)sizeof(Trim3),"trim3",&Trim3);
		ParamID = gSQLiteSummaries.AddParameter(gProcessingID,ePTInt32,(int)sizeof(MinReadLen),"minreadlen",&MinReadLen);


		ParamID = gSQLiteSummaries.AddParameter(gProcessingID,ePTText,(int)strlen(szInputFile),"in",szInputFile);
		ParamID = gSQLiteSummaries.AddParameter(gProcessingID,ePTText,(int)strlen(szOutFile),"out",szOutFile);
		
		ParamID = gSQLiteSummaries.AddParameter(gProcessingID,ePTInt32,(int)sizeof(NumThreads),"threads",&NumThreads);
		ParamID = gSQLiteSummaries.AddParameter(gProcessingID,ePTInt32,(int)sizeof(NumberOfProcessors),"cpus",&NumberOfProcessors);

		ParamID = gSQLiteSummaries.AddParameter(gProcessingID,ePTText,(int)strlen(szSQLiteDatabase),"sumrslts",szSQLiteDatabase);
		ParamID = gSQLiteSummaries.AddParameter(gProcessingID,ePTText,(int)strlen(szExperimentName),"experimentname",szExperimentName);
		ParamID = gSQLiteSummaries.AddParameter(gProcessingID,ePTText,(int)strlen(szExperimentDescr),"experimentdescr",szExperimentDescr);
		}


#ifdef _WIN32
	SetPriorityClass(GetCurrentProcess(), BELOW_NORMAL_PRIORITY_CLASS);
#endif
	gStopWatch.Start();
	Rslt = ProcPacBioFilter((etPBPMode)PMode,MaxScore,MinSeedCoreLen,MaxCoreSeparation,MinClustLen,Trim5,Trim3,MinReadLen,szInputFile,szOutFile,NumThreads);
	Rslt = Rslt >=0 ? 0 : 1;
	if(gExperimentID > 0)
		{
		if(gProcessingID)
			gSQLiteSummaries.EndProcessing(gProcessingID,Rslt);
		gSQLiteSummaries.EndExperiment(gExperimentID);
		}
	gStopWatch.Stop();

	gDiagnostics.DiagOut(eDLInfo,gszProcName,"Exit code: %d Total processing time: %s",Rslt,gStopWatch.Read());
	exit(Rslt);
	}
else
	{
    printf("\n%s %s %s, Version %s\n", gszProcName,gpszSubProcess->pszName,gpszSubProcess->pszFullDescr,cpszProgVer);
	arg_print_errors(stdout,end,gszProcName);
	arg_print_syntax(stdout,argtable,"\nUse '-h' to view option and parameter usage\n");
	exit(1);
	}
return 0;
}


int
ProcPacBioFilter(etPBPMode PMode,	// processing mode
		double MaxScore,			// only accept reads with scores <= this score
		int MinSeedCoreLen,			// use seed cores of this length when identifying putative antisense subsequences indicative of SMRTBell read past onto antisense
		int MaxCoreSeparation,      // max core separation between cores in the same cluster
		int MinClustLen,			// any putative sense overlap with antisense subsequence cluster must be of at least this length
		int Trim5,					// 5' trim accepted reads by this many bp
		int Trim3,					// 3' trim accepted reads by this many bp
		int MinReadLen,				// read sequences must be at least this length after any end timming
		char *pszInputFile,			// name of input file containing reads to be filtered
		char *pszOutFile,			// name of file in which to write filter accepted and trimmed sequences
		int NumThreads)			// maximum number of worker threads to use
{
int Rslt;
CPBFilter *pPacBioer;

if((pPacBioer = new CPBFilter)==NULL)
	{
	gDiagnostics.DiagOut(eDLFatal,gszProcName,"Fatal: Unable to instantiate CPBFilter");
	return(eBSFerrObj);
	}
Rslt = pPacBioer->Process(PMode,MaxScore,MinSeedCoreLen,MaxCoreSeparation,MinClustLen,Trim5,Trim3,MinReadLen,pszInputFile,pszOutFile,NumThreads);
delete pPacBioer;
return(Rslt);
}

CPBFilter::CPBFilter() // relies on base classes constructors
{
m_bMutexesCreated = false;
m_hOutFilt = -1;
m_hOutFile = -1;
m_pOutBuff = NULL;
Init();
}

CPBFilter::~CPBFilter() // relies on base classes destructors
{
Reset();
}


void
CPBFilter::Init(void)
{
if(m_hOutFile != -1)
	{
#ifdef _WIN32
	_commit(m_hOutFile);
#else
	fsync(m_hOutFile);
#endif
	close(m_hOutFile);
	m_hOutFile = -1;
	}
if(m_pOutBuff)
	{
	delete m_pOutBuff;
	m_pOutBuff = NULL;
	}

if(m_hOutFilt != -1)
	{
#ifdef _WIN32
	_commit(m_hOutFilt);
#else
	fsync(m_hOutFilt);
#endif
	close(m_hOutFilt);
	m_hOutFilt = -1;
	}

m_PMode = ePBPMFilter;

m_MinSeedCoreLen = cDfltFiltSeedCoreLen;
m_MaxCoreSeparation = cDfltCoreSeparation;				
m_MinClustLen = cDfltMinClustLen;	
m_MinCoresCluster = cDfltMinCoresCluster;				
m_Trim5 = cDfltTrim5;							
m_Trim3 = cDfltTrim3;							
m_MinReadLen = cDfltMinReadLen;		

m_TotProcessed = 0;
m_TotAccepted = 0;
m_TotRejected = 0;
m_TotUnderLen = 0;
	
m_OutBuffIdx = 0;
m_AllocOutBuffSize=0;
			
m_szInputFile[0] = '\0';			
m_szOutFile[0] = '\0';			
m_szOutFilt[0] = '\0';			
m_szFiltLineBuff[0x07fff];			
m_FiltLineBuffIdx = 0;					
m_NumThreads = 0;
if(m_bMutexesCreated)
	DeleteMutexes();
m_bMutexesCreated = false; 
m_PacBioUtility.Reset();
}

void
CPBFilter::Reset(void)
{

Init();
}


int
CPBFilter::CreateMutexes(void)
{
if(m_bMutexesCreated)
	return(eBSFSuccess);

#ifdef _WIN32
InitializeSRWLock(&m_hRwLock);
#else
if(pthread_rwlock_init (&m_hRwLock,NULL)!=0)
	{
	gDiagnostics.DiagOut(eDLFatal,gszProcName,"Fatal: unable to create rwlock");
	return(eBSFerrInternal);
	}
#endif

#ifdef _WIN32
if((m_hMtxIterReads = CreateMutex(NULL,false,NULL))==NULL)
	{
#else
if(pthread_mutex_init (&m_hMtxIterReads,NULL)!=0)
	{
	pthread_rwlock_destroy(&m_hRwLock);
#endif
	gDiagnostics.DiagOut(eDLFatal,gszProcName,"Fatal: unable to create mutex");
	return(eBSFerrInternal);
	}

#ifdef _WIN32
if((m_hMtxMHReads = CreateMutex(NULL,false,NULL))==NULL)
	{
#else
if(pthread_mutex_init (&m_hMtxMHReads,NULL)!=0)
	{
#endif
	gDiagnostics.DiagOut(eDLFatal,gszProcName,"Fatal: unable to create mutex");
#ifdef _WIN32
	CloseHandle(m_hMtxIterReads);
#else
	pthread_rwlock_destroy(&m_hRwLock);
	pthread_mutex_destroy(&m_hMtxIterReads);
#endif
	return(eBSFerrInternal);
	}

m_bMutexesCreated = true;
return(eBSFSuccess);
}

void
CPBFilter::DeleteMutexes(void)
{
if(!m_bMutexesCreated)
	return;
#ifdef _WIN32
CloseHandle(m_hMtxIterReads);
CloseHandle(m_hMtxMHReads);
#else
pthread_mutex_destroy(&m_hMtxIterReads);
pthread_mutex_destroy(&m_hMtxMHReads);
pthread_rwlock_destroy(&m_hRwLock);
#endif
m_bMutexesCreated = false;
}

void
CPBFilter::AcquireSerialise(void)
{
#ifdef _WIN32
WaitForSingleObject(m_hMtxIterReads,INFINITE);
#else
pthread_mutex_lock(&m_hMtxIterReads);
#endif
}

void
CPBFilter::ReleaseSerialise(void)
{
#ifdef _WIN32
ReleaseMutex(m_hMtxIterReads);
#else
pthread_mutex_unlock(&m_hMtxIterReads);
#endif
}

void
CPBFilter::AcquireSerialiseMH(void)
{
#ifdef _WIN32
WaitForSingleObject(m_hMtxMHReads,INFINITE);
#else
pthread_mutex_lock(&m_hMtxMHReads);
#endif
}

void
CPBFilter::ReleaseSerialiseMH(void)
{
#ifdef _WIN32
ReleaseMutex(m_hMtxMHReads);
#else
pthread_mutex_unlock(&m_hMtxMHReads);
#endif
}

void
CPBFilter::AcquireLock(bool bExclusive)
{
#ifdef _WIN32
if(bExclusive)
	AcquireSRWLockExclusive(&m_hRwLock);
else
	AcquireSRWLockShared(&m_hRwLock);
#else
if(bExclusive)
	pthread_rwlock_wrlock(&m_hRwLock);
else
	pthread_rwlock_rdlock(&m_hRwLock);
#endif
}

void
CPBFilter::ReleaseLock(bool bExclusive)
{
#ifdef _WIN32
if(bExclusive)
	ReleaseSRWLockExclusive(&m_hRwLock);
else
	ReleaseSRWLockShared(&m_hRwLock);
#else
pthread_rwlock_unlock(&m_hRwLock);
#endif
}

int
CPBFilter::Process(etPBPMode PMode,	// processing mode
		double MaxScore,			// only accept reads with scores <= this score
		int MinSeedCoreLen,			// use seed cores of this length when identifying putative antisense subsequences indicative of SMRTBell read past onto antisense
		int MaxCoreSeparation,      // max core separation between cores in the same cluster
		int MinClustLen,			// any putative sense overlap with antisense subsequence cluster must be of at least this length
		int Trim5,					// 5' trim accepted reads by this many bp
		int Trim3,					// 3' trim accepted reads by this many bp
		int MinReadLen,				// read sequences must be at least this length after any end timming
		char *pszInputFile,			// name of input file containing reads to be filtered
		char *pszOutFile,			// name of file in which to write filter accepted and trimmed sequences
		int NumThreads)			// maximum number of worker threads to use
{
int Rslt = eBSFSuccess;

Reset();
CreateMutexes();

m_PMode = PMode;
m_MinSeedCoreLen = MinSeedCoreLen;
m_MaxCoreSeparation = MaxCoreSeparation;    
m_MinClustLen = MinClustLen;			
m_Trim5 = Trim5;					
m_Trim3 = Trim3;					
m_MinReadLen = MinReadLen;	
m_MaxScore = MaxScore;			

strncpy(m_szInputFile,pszInputFile,sizeof(m_szInputFile));
m_szInputFile[sizeof(m_szInputFile)-1] = '\0';	
strncpy(m_szOutFile,pszOutFile,sizeof(m_szOutFile));
m_szOutFile[sizeof(m_szOutFile)-1] = '\0';	
strncpy(m_szOutFilt,m_szOutFile,sizeof(m_szOutFile) - 11);
strcat(m_szOutFilt,".filt.csv");

m_NumThreads = NumThreads;	
m_PacBioUtility.Reset();
if((Rslt = m_PacBioUtility.StartAsyncLoadSeqs(m_szInputFile)) < eBSFSuccess)
	{
	Reset();
	return(Rslt);
	}

Rslt = ProcessFiltering(cDfltMaxPacBioSeqLen,NumThreads);

gDiagnostics.DiagOut(eDLFatal,gszProcName,"Completed: Loaded %d PacBio reads, accepted (A) %d, rejected (R) %d and further (U) %d were underlength",m_TotProcessed,m_TotAccepted,m_TotRejected,m_TotUnderLen);

Reset();
return(Rslt);
}

#ifdef _WIN32
unsigned __stdcall PBFilterThread(void * pThreadPars)
#else
void *PBFilterThread(void * pThreadPars)
#endif
{
int Rslt;
tsThreadPBFilter *pPars = (tsThreadPBFilter *)pThreadPars;			// makes it easier not having to deal with casts!
CPBFilter *pPBFilter = (CPBFilter *)pPars->pThis;

Rslt = pPBFilter->PBFilterReads(pPars);
pPars->Rslt = Rslt;
#ifdef _WIN32
_endthreadex(0);
return(eBSFSuccess);
#else
pthread_exit(NULL);
#endif
}

int
CPBFilter::ProcessFiltering(int MaxSeqLen,			// max length sequence expected
							int NumOvlpThreads)	// filtering using at most this many threads
{
tsThreadPBFilter *pThreadPutOvlps;
int ThreadIdx;
tsThreadPBFilter *pThreadPar;

pThreadPutOvlps = new tsThreadPBFilter [NumOvlpThreads];

pThreadPar = pThreadPutOvlps;
for(ThreadIdx = 0; ThreadIdx < NumOvlpThreads; ThreadIdx++,pThreadPar++)
	{
	memset(pThreadPar,0,sizeof(tsThreadPBFilter));
	pThreadPar->AllocdCoreHits = cAllocdNumCoreHits;
	pThreadPar->AllocdCoreHitsSize = cAllocdNumCoreHits * sizeof(tsFiltCoreHit);
#ifdef _WIN32
	pThreadPar->pCoreHits = (tsFiltCoreHit *)malloc(pThreadPar->AllocdCoreHitsSize);	
	if(pThreadPar->pCoreHits == NULL)
		{
		gDiagnostics.DiagOut(eDLFatal,gszProcName,"Process: Core hits memory allocation of %llu bytes - %s",pThreadPar->AllocdCoreHitsSize,strerror(errno));
		break;
		}
#else
	if((pThreadPar->pCoreHits = (tsFiltCoreHit *)mmap(NULL,pThreadPar->AllocdCoreHitsSize, PROT_READ |  PROT_WRITE,MAP_PRIVATE | MAP_ANONYMOUS, -1,0)) == MAP_FAILED)
		{
		gDiagnostics.DiagOut(eDLFatal,gszProcName,"Process: Core hits memory allocation of %llu bytes - %s",pThreadPar->AllocdCoreHitsSize,strerror(errno));
		break;
		}
#endif

		pThreadPar->AllocdAntisenseKmersSize = (UINT32)(pow(4,m_MinSeedCoreLen) * sizeof(tsAntisenseKMerOfs));
		if((pThreadPar->pAntisenseKmers = new tsAntisenseKMerOfs [pThreadPar->AllocdAntisenseKmersSize + 10])==NULL)
			{
			gDiagnostics.DiagOut(eDLFatal,gszProcName,"Process: Antisense KMer sequence memory allocation of %d bytes - %s",pThreadPar->AllocdAntisenseKmersSize,strerror(errno));
			break;
			}
	
	pThreadPar->AllocdTargSeqSize = max(cAllocdQuerySeqLen,MaxSeqLen);
	if((pThreadPar->pTargSeq = new etSeqBase [pThreadPar->AllocdTargSeqSize + 10])==NULL)
		{
		gDiagnostics.DiagOut(eDLFatal,gszProcName,"Process: Core hits target sequence memory allocation of %d bytes - %s",pThreadPar->AllocdTargSeqSize,strerror(errno));
		break;
		}

	if((pThreadPar->pmtqsort = new CMTqsort) == NULL)
		{
		gDiagnostics.DiagOut(eDLFatal,gszProcName,"Process: Core hits instantiation of CMTqsort failed");
		break;
		}
	pThreadPar->pmtqsort->SetMaxThreads(4);
	}

if(ThreadIdx != NumOvlpThreads)	// any errors whilst allocating memory for core hits?
	{
	do {
		if(pThreadPar->pmtqsort != NULL)
			delete pThreadPar->pmtqsort;

		if(pThreadPar->pAntisenseKmers != NULL)
			delete pThreadPar->pAntisenseKmers;

		if(pThreadPar->pCoreHits != NULL)
			{
#ifdef _WIN32
			free(pThreadPar->pCoreHits);				// was allocated with malloc/realloc, or mmap/mremap, not c++'s new....
#else
			if(pThreadPar->pCoreHits != MAP_FAILED)
				munmap(pThreadPar->pCoreHits,pThreadPar->AllocdCoreHitsSize);
#endif	
			}
		pThreadPar -= 1;
		ThreadIdx -= 1;
		}
	while(ThreadIdx >= 0);
	delete pThreadPutOvlps;
	Reset();
	return((INT64)eBSFerrMem);
	}


if((m_pOutBuff = new char [cAllocOutBuffSize]) == NULL)
	{
	gDiagnostics.DiagOut(eDLFatal,gszProcName,"Unable to allocate %d chars for buffering",cAllocOutBuffSize);
	Reset();
	return(eBSFerrMem);
	}
m_AllocOutBuffSize = cAllocOutBuffSize;

#ifdef _WIN32
m_hOutFile = open(m_szOutFile,( O_WRONLY | _O_BINARY | _O_SEQUENTIAL | _O_CREAT | _O_TRUNC),(_S_IREAD | _S_IWRITE));
#else
if((m_hOutFile = open(m_szOutFile,O_WRONLY | O_CREAT,S_IREAD | S_IWRITE))!=-1)
	if(ftruncate(m_hOutFile,0)!=0)
		{
		gDiagnostics.DiagOut(eDLFatal,gszProcName,"Unable to truncate %s - %s",m_szOutFile,strerror(errno));
		Reset();
		return(eBSFerrCreateFile);
		}
#endif
if(m_hOutFile < 0)
	{
	gDiagnostics.DiagOut(eDLFatal,gszProcName,"Process: unable to create/truncate output file '%s'",m_szOutFile);
	Reset();
	return(eBSFerrCreateFile);
	}

#ifdef _WIN32
m_hOutFilt = open(m_szOutFilt,( O_WRONLY | _O_BINARY | _O_SEQUENTIAL | _O_CREAT | _O_TRUNC),(_S_IREAD | _S_IWRITE));
#else
if((m_hOutFilt = open(m_szOutFilt,O_WRONLY | O_CREAT,S_IREAD | S_IWRITE))!=-1)
	if(ftruncate(m_hOutFilt,0)!=0)
		{
		gDiagnostics.DiagOut(eDLFatal,gszProcName,"Unable to truncate %s - %s",m_szOutFilt,strerror(errno));
		Reset();
		return(eBSFerrCreateFile);
		}
#endif
if(m_hOutFilt < 0)
	{
	gDiagnostics.DiagOut(eDLFatal,gszProcName,"Process: unable to create/truncate output file '%s'",m_szOutFilt);
	Reset();
	return(eBSFerrCreateFile);
	}

if(m_hOutFilt != -1)
	{
	m_FiltLineBuffIdx=sprintf(m_szFiltLineBuff,"\"Descr\",\"Sense\",\"Score\",\"ProbeID\",\"ProbeLen\",\"SenseOfs\",\"AntisenseOfs\",\"ClustLen\",\"NumClustHits\",\"SumHitBases\"");
	CUtility::SafeWrite(m_hOutFilt,m_szFiltLineBuff,m_FiltLineBuffIdx);
	m_FiltLineBuffIdx = 0;
	}

pThreadPar = pThreadPutOvlps;
for (ThreadIdx = 1; ThreadIdx <= NumOvlpThreads; ThreadIdx++, pThreadPar++)
	{
	pThreadPar->ThreadIdx = ThreadIdx;
	pThreadPar->pThis = this;
#ifdef _WIN32
	pThreadPar->threadHandle = (HANDLE)_beginthreadex(NULL, 0x0fffff, PBFilterThread, pThreadPar, 0, &pThreadPar->threadID);
#else
	pThreadPar->threadRslt = pthread_create(&pThreadPar->threadID, NULL, PBFilterThread, pThreadPar);
#endif
	}

// allow threads a few seconds to startup
#ifdef _WIN32
Sleep(2000);
#else
sleep(2);
#endif
pThreadPar = pThreadPutOvlps;
for (ThreadIdx = 0; ThreadIdx < NumOvlpThreads; ThreadIdx++, pThreadPar++)
	{
#ifdef _WIN32
	while (WAIT_TIMEOUT == WaitForSingleObject(pThreadPar->threadHandle, 60000))
		{
		AcquireSerialise();

		ReleaseSerialise();
		};
	CloseHandle(pThreadPar->threadHandle);
#else
	struct timespec ts;
	int JoinRlt;
	clock_gettime(CLOCK_REALTIME, &ts);
	ts.tv_sec += 60;
	while ((JoinRlt = pthread_timedjoin_np(pThreadPar->threadID, NULL, &ts)) != 0)
		{
		AcquireSerialise();

		ReleaseSerialise();
		ts.tv_sec += 60;
		}
#endif
	}

pThreadPar = pThreadPutOvlps;
for(ThreadIdx = 0; ThreadIdx < NumOvlpThreads; ThreadIdx++,pThreadPar++)
	{
	if(pThreadPar->pmtqsort != NULL)
		delete pThreadPar->pmtqsort;

	if(pThreadPar->pSW != NULL)
		delete pThreadPar->pSW;

	if(pThreadPar->pCoreHits != NULL)
		{
#ifdef _WIN32
		free(pThreadPar->pCoreHits);				// was allocated with malloc/realloc, or mmap/mremap, not c++'s new....
#else
		if(pThreadPar->pCoreHits != MAP_FAILED)
			munmap(pThreadPar->pCoreHits,pThreadPar->AllocdCoreHitsSize);
#endif	
		pThreadPar->pCoreHits = NULL;
		}
	if(pThreadPar->pTargSeq != NULL)
		delete pThreadPar->pTargSeq;
	}

delete pThreadPutOvlps;


if(m_hOutFile != -1)
	{
	if(m_OutBuffIdx > 0)
		CUtility::SafeWrite(m_hOutFile,m_pOutBuff,m_OutBuffIdx);

#ifdef _WIN32
	_commit(m_hOutFile);
#else
	fsync(m_hOutFile);
#endif
	close(m_hOutFile);
	m_hOutFile = -1;
	delete m_pOutBuff;
	m_pOutBuff = NULL;
	}

if(m_hOutFilt != -1)
	{
	if(m_FiltLineBuffIdx > 0)
		CUtility::SafeWrite(m_hOutFilt,m_szFiltLineBuff,m_FiltLineBuffIdx);

#ifdef _WIN32
	_commit(m_hOutFilt);
#else
	fsync(m_hOutFilt);
#endif
	close(m_hOutFilt);
	m_hOutFilt = -1;
	}

return(0);
}

// expectations -
// core hits in pThreadPar have been sorted ProbeID.TargID.TargOfs.ProbeOfs ascending
int				// returns 0 if no cluster, or if a cluster then the length of that cluster
CPBFilter::ClusterSpatial(tsThreadPBFilter *pThreadPar, 
				UINT32 ProbeLen,					// sequence from which antisense cores were identified was this length		
			   tsFiltCoreHitsCluster *pCluster,			// returned cluster
			   UINT32 MinClusterHits,				// clusters must contain at least this number of consistency checked hits
			   UINT32 MinClusterLen)				// clusters must be at least this length
{
UINT32 MaxClusterLen;
UINT32 NumClustHits;
UINT32 SumClustHitLens;

tsFiltCoreHit *pClustCoreHit;
tsFiltCoreHit *pPrevClustCoreHit;

UINT32 CurProbeNodeID;
tsFiltCoreHit *pCurCoreHit;

UINT32 ClustProbeOfs;
UINT32 IntraClustProbeOfs;
UINT32 ClustTargOfs;
UINT32 ClustLen;

double ClustScore;

UINT32 DeltaProbeOfs;
UINT32 DeltaTargOfs;

if(pCluster != NULL)
	memset(pCluster,0,sizeof(tsFiltCoreHitsCluster));

if(pThreadPar == NULL || pThreadPar->pCoreHits == NULL || MinClusterHits < 2 || pThreadPar->NumCoreHits < MinClusterHits || pCluster == NULL || MinClusterLen < 50 || ProbeLen < MinClusterLen)
	return(0);


pThreadPar->pmtqsort->qsort(pThreadPar->pCoreHits,pThreadPar->NumCoreHits,sizeof(tsFiltCoreHit),SortCoreHitsByProbeTargOfs); // sort core hits by ProbeOfs.TargOfs ascending

MaxClusterLen = (ProbeLen * 110) / 100;			// allowing 10% for PacBio insertions
pCurCoreHit = pThreadPar->pCoreHits;
CurProbeNodeID = pCurCoreHit->ProbeNodeID;
pCluster->ProbeID = CurProbeNodeID; 
pCluster->ProbeSeqLen = ProbeLen;
pCluster->Status = 'A';

while(pCurCoreHit->ProbeNodeID != 0)	    // hits are terminated by core with ProbeNodeID set to 0	
	{
	ClustProbeOfs = pCurCoreHit->ProbeOfs;	// note starting offsets as these are used intra cluster to determine consistency
	ClustTargOfs = pCurCoreHit->TargOfs;
	IntraClustProbeOfs = ClustProbeOfs+pCurCoreHit->HitLen;
	NumClustHits = 1;
	ClustLen = SumClustHitLens = pCurCoreHit->HitLen;
	pPrevClustCoreHit = pCurCoreHit;
	pClustCoreHit = pCurCoreHit + 1;
	while(pClustCoreHit->ProbeNodeID != 0)
		{
		if(pClustCoreHit->TargOfs >= ClustTargOfs + MaxClusterLen)	// target hits must be within the probe length + 10% to allow for the PacBio insertions
			break;
		if(pClustCoreHit->ProbeOfs <= IntraClustProbeOfs) // looking for next ascending probe core
			{
			pClustCoreHit += 1;
			continue;
			}
		if(pClustCoreHit->ProbeOfs - pPrevClustCoreHit->ProbeOfs > (UINT32)m_MaxCoreSeparation)
			break;

		DeltaProbeOfs = pClustCoreHit->ProbeOfs - ClustProbeOfs;	// will always be >= 1
		if( pClustCoreHit->TargOfs < ClustTargOfs)					// core could easily be before the current target offset as sorted by probeofs.targofs
			{
			pClustCoreHit += 1;
			continue;		
			}

		else
			DeltaTargOfs = pClustCoreHit->TargOfs - ClustTargOfs;       // could be < 0

		if(DeltaProbeOfs > max(25,(DeltaTargOfs * 110)/100) || DeltaTargOfs > max(25,(DeltaProbeOfs * 110)/100)) // allowing at least 25bp float
			{
			pClustCoreHit += 1;
			continue;		
			}
		// accepting this core hit as being consistent between probe and target
		NumClustHits += 1;
		pPrevClustCoreHit = pClustCoreHit;
		SumClustHitLens += pClustCoreHit->HitLen;
		ClustLen = pClustCoreHit->HitLen + pClustCoreHit->ProbeOfs - ClustProbeOfs; 
		IntraClustProbeOfs = pClustCoreHit->ProbeOfs + pClustCoreHit->HitLen;
		pClustCoreHit += 1;
		}
	pCurCoreHit += 1;
	if(ClustLen < MinClusterLen || ClustLen > MaxClusterLen || NumClustHits < MinClusterHits)
		continue;

	// if a cluster then is this cluster higher scoring than any previous?
	// clusters are scored as a function of the SumClustLens and the ratio of the number of cores in cluster to the cluster length
    // e.g (SumClustLens * NumClustHits) / ClustLen
	ClustScore = ((double)SumClustHitLens * NumClustHits) / ClustLen;
	if(ClustScore > pCluster->ClustScore)
		{
		pCluster->ClustProbeOfs = ClustProbeOfs;		
		pCluster->ClustTargOfs = ClustTargOfs;
		pCluster->ClustLen = ClustLen;
		pCluster->NumClustHits = NumClustHits;
		pCluster->SumClustHitLens = SumClustHitLens;
		pCluster->ClustScore = ClustScore;
		if(ClustScore > m_MaxScore)
			pCluster->Status = 'R';
		else
			pCluster->Status = 'A';
		}
	}

return(pCluster->ClustLen);
}


int
CPBFilter::PBFilterReads(tsThreadPBFilter *pThreadPar)
{
int SeqID;
char szQuerySeqIdent[100];
UINT8 *pQuerySeq;
int QuerySeqLen;
int	PrevSeqID;
int PrevQuerySeqLen;
tsSSWCell *pPeakMatchesCell;
int NumTopNPeakMatches;
tsSSWCell *pPeakMatchesCells;
tsSSWCell *pPutPeakMatchesCell;
int AcceptedHairpins;
int AcceptedEndHairpins;

if(pThreadPar->pSW == NULL)
	{
	AcquireSerialise();
	pThreadPar->pSW = new CSSW;
	pThreadPar->pSW->SetScores(2,-7,-5,-2,3,6,3);
	pThreadPar->pSW->PreAllocMaxTargLen(100000);		// will be realloc'd as may be needed ...
	ReleaseSerialise();
	}

tsFiltCoreHitsCluster Cluster;
AcceptedHairpins = 0;
AcceptedEndHairpins = 0;
PrevSeqID = 0;
PrevQuerySeqLen = 0;
while((pQuerySeq = m_PacBioUtility.DequeueQuerySeq(20,sizeof(szQuerySeqIdent),&SeqID,szQuerySeqIdent,&QuerySeqLen))!=NULL) // iterating over all sequences and processing as targets
	{
	pThreadPar->NumCoreHits = 0;
	memset(&Cluster,0,sizeof(Cluster));

	if(!(SeqID % 5000))
		gDiagnostics.DiagOut(eDLInfo,gszProcName,"Process: Thread: %d Processing probe %d (len: %d)",pThreadPar->ThreadIdx,SeqID,QuerySeqLen);

	if(QuerySeqLen < m_MinReadLen + m_Trim5 + m_Trim3)
		{
		Cluster.ProbeID = SeqID;
		Cluster.ProbeSeqLen = QuerySeqLen;
		Cluster.Status = 'U';
		}
	else
		{
		pThreadPar->pSW->SetMaxInitiatePathOfs(0);
		pThreadPar->pSW->SetMinNumExactMatches(cSmartBellAdaptorSeqLen/2);
		pThreadPar->pSW->SetTopNPeakMatches(20);
		pThreadPar->pSW->SetProbe(cSmartBellAdaptorSeqLen,(etSeqBase *)cSmartBellAdaptorSeq);
		pThreadPar->pSW->SetTarg(QuerySeqLen,pQuerySeq);	// iterated sequence is the target
		pPeakMatchesCell = pThreadPar->pSW->Align();
		NumTopNPeakMatches = pThreadPar->pSW->GetTopNPeakMatches(&pPeakMatchesCells);


		if(NumTopNPeakMatches)
			{
			int PeakIdx;
			etSeqBase *p5Seq;
			etSeqBase *p3Seq;
			etSeqBase RevCpl3Seq[1000];
			pPeakMatchesCell = pPeakMatchesCells;
			for(PeakIdx = 0; PeakIdx < NumTopNPeakMatches;  PeakIdx++,pPeakMatchesCell++)
				{
				if(pPeakMatchesCell->StartTOfs < 500 || (QuerySeqLen - pPeakMatchesCell->EndTOfs) < 501)
					{
					// just trim
					AcceptedEndHairpins += 1;
					continue;
					}
				// get ptr to sequence 5' to identified hairpin peak
				p5Seq = &pQuerySeq[pPeakMatchesCell->StartTOfs - 499];
				// get sequence 3' to identified hairpin peak and RevCpl
				p3Seq = &pQuerySeq[pPeakMatchesCell->EndTOfs];
				memcpy(RevCpl3Seq,p3Seq,500);
				CSeqTrans::ReverseComplement(500,RevCpl3Seq);
				// look for alignment
				pThreadPar->pSW->SetMaxInitiatePathOfs(100);
				pThreadPar->pSW->SetMinNumExactMatches(200);
				pThreadPar->pSW->SetTopNPeakMatches(0);
				pThreadPar->pSW->SetProbe(500,p5Seq);
				pThreadPar->pSW->SetTarg(500,p3Seq);	
				pPutPeakMatchesCell = pThreadPar->pSW->Align();
				if(pPutPeakMatchesCell->NumExacts >= 200)
					AcceptedHairpins += 1;
				}
			}

		Cluster.ProbeID = SeqID;
		Cluster.ProbeSeqLen = QuerySeqLen;
		Cluster.Status = 'A';

	//		IdentifySmartBells(CurNodeID,pThreadPar);
//		if(NumTopNPeakMatches > 0)
//			{
//			IdentifyCoreHits(SeqID,QuerySeqLen,pQuerySeq,pThreadPar);
//			if(pThreadPar->NumCoreHits >= (UINT32)m_MinCoresCluster)
//				ClusterSpatial(pThreadPar,QuerySeqLen,&Cluster,m_MinCoresCluster,m_MinClustLen);
//			}
		}

	if(m_hOutFilt != -1)
		{
		AcquireSerialise();
		if(m_FiltLineBuffIdx > sizeof(m_szFiltLineBuff) - 1000)
			{
			CUtility::SafeWrite(m_hOutFilt,m_szFiltLineBuff,m_FiltLineBuffIdx);
			m_FiltLineBuffIdx = 0;
			}

		m_FiltLineBuffIdx += sprintf(&m_szFiltLineBuff[m_FiltLineBuffIdx], "\n\"%s\",\"%c\",%1.3f,%d,%d,%d,%d,%d,%d,%d",
									szQuerySeqIdent,
									Cluster.Status,Cluster.ClustScore,Cluster.ProbeID,Cluster.ProbeSeqLen,
									Cluster.ClustProbeOfs,Cluster.ClustTargOfs,Cluster.ClustLen,Cluster.NumClustHits,Cluster.SumClustHitLens);
		m_TotProcessed += 1;
		switch(Cluster.Status) {
			case 'a': case 'A':
				m_TotAccepted += 1;
				break;
			case 'r': case 'R':
				m_TotRejected += 1;
				break;
			case 'u': case 'U':
				m_TotUnderLen += 1;
				break;
			}

		if(m_hOutFile != -1 && (Cluster.Status == 'A' || Cluster.Status == 'a'))
			{
			int SeqIdx;
			int LineLen;
			char Base;
			char *pBuff;
			if(m_OutBuffIdx > (m_AllocOutBuffSize - (QuerySeqLen * 2)))
				{
				CUtility::SafeWrite(m_hOutFile,m_pOutBuff,m_OutBuffIdx);
				m_OutBuffIdx = 0;
				}
			m_OutBuffIdx += sprintf(&m_pOutBuff[m_OutBuffIdx],">%s\n",szQuerySeqIdent);
			LineLen = 0;
			pBuff = &m_pOutBuff[m_OutBuffIdx]; 
			for(SeqIdx = m_Trim5; SeqIdx < QuerySeqLen - m_Trim3; SeqIdx++)
				{
				switch(pQuerySeq[SeqIdx]) {
					case eBaseA:
						Base = 'A';
						break;
					case eBaseC:
						Base = 'C';
						break;
					case eBaseG:
						Base = 'G';
						break;
					case eBaseT:
						Base = 'T';
						break;
					default:
						Base = 'N';
						break;
						}
				*pBuff++ = Base;
				m_OutBuffIdx += 1;
				LineLen += 1;
				if(LineLen > 80)
					{
					*pBuff++ = '\n';
					m_OutBuffIdx += 1;
					LineLen = 0;
					}
				}	
			*pBuff++ = '\n';
			m_OutBuffIdx += 1;
			}
		ReleaseSerialise();
		}

	delete pQuerySeq;
	PrevSeqID = SeqID;
	PrevQuerySeqLen = QuerySeqLen;
	}

if(m_hOutFilt != -1)
	{
	AcquireSerialise();
	if(m_FiltLineBuffIdx)
		{
		CUtility::SafeWrite(m_hOutFilt,m_szFiltLineBuff,m_FiltLineBuffIdx);
		m_FiltLineBuffIdx = 0;
		}
	ReleaseSerialise();
	}
if(PrevSeqID)
	gDiagnostics.DiagOut(eDLInfo,gszProcName,"Completed: Thread: %d Processed probe %d (len: %d)",pThreadPar->ThreadIdx,PrevSeqID,PrevQuerySeqLen);
return(0);
}


int					// returns index 1..N of just added core hit or -1 if errors
CPBFilter::AddCoreHit(UINT32 ProbeNodeID,		// core hit was from this probe read 
			   UINT32 ProbeOfs,                 // hit started at this probe offset
			   UINT32 TargOfs,                  // probe core matched starting at this antisense offset
			   UINT32 HitLen,					// hit was of this length
               tsThreadPBFilter *pPars)			// thread specific
{
tsFiltCoreHit *pCoreHit;

if((pPars->NumCoreHits + 5) > pPars->AllocdCoreHits)	// need to realloc memory to hold additional cores?
	{
		// realloc memory with a 25% increase over previous allocation 
	int coresreq;
	size_t memreq;
	void *pAllocd;
	coresreq = (int)(((INT64)pPars->AllocdCoreHits * 125) / (INT64)100);
	memreq = coresreq * sizeof(tsFiltCoreHit);

#ifdef _WIN32
		pAllocd = realloc(pPars->pCoreHits,memreq);
#else
		pAllocd = mremap(pPars->pCoreHits,pPars->AllocdCoreHitsSize,memreq,MREMAP_MAYMOVE);
		if(pAllocd == MAP_FAILED)
			pAllocd = NULL;
#endif
		if(pAllocd == NULL)
			{
			gDiagnostics.DiagOut(eDLFatal,gszProcName,"SavePartialSeqs: Memory re-allocation to %d bytes - %s",memreq,strerror(errno));
			Reset();
			return(eBSFerrMem);
			}

		pPars->pCoreHits = (tsFiltCoreHit *)pAllocd;
		pPars->AllocdCoreHitsSize = memreq;
		pPars->AllocdCoreHits = coresreq; 
		}
		
pCoreHit = &pPars->pCoreHits[pPars->NumCoreHits++];

pCoreHit->ProbeNodeID = ProbeNodeID;
pCoreHit->ProbeOfs = ProbeOfs;
pCoreHit->HitLen = HitLen;
pCoreHit->TargOfs = TargOfs;
memset(&pCoreHit[1],0,sizeof(tsFiltCoreHit));	// ensuring that used cores are always terminated with a marker end of cores initialised to 0
return(pPars->NumCoreHits);
}

int
CPBFilter::IdentifyCoreHits(UINT32 SeqID,					// sequence identifier
							int SeqLen,						// sequence is this long bp
							etSeqBase *pProbeSeq,				// sequence to process for core hits from sense on to the antisense
							tsThreadPBFilter *pPars)		// thread specific
{
UINT32 HitsThisCore;
UINT32 TotHitsAllCores;
UINT32 ChkOvrLapCoreProbeOfs;
UINT32 LastCoreProbeOfs;
tsFiltCoreHit *pChkOvrLapCore;
int ChkOvrLapCoreStartIdx;
int ChkOvrlapIdx;

etSeqBase *pHomo;
int HomoIdx;
int HomoBaseCnts[4];
int MaxAcceptHomoCnt;
UINT32 QIdx;
UINT32 TIdx;
etSeqBase *pQIdx;
etSeqBase *pTIdx;

if(SeqID < 1 || SeqLen < 10 || pProbeSeq == NULL || pPars == NULL)
	return(eBSFerrParams);
if(pPars->pTargSeq == NULL || pPars->AllocdTargSeqSize < (UINT32)SeqLen + 10)
	{
	if(pPars->pTargSeq != NULL)
		delete pPars->pTargSeq;
	pPars->AllocdTargSeqSize = SeqLen;
	pPars->pTargSeq = new etSeqBase [SeqLen + 10];
	}

memset(pPars->pAntisenseKmers,0,pPars->AllocdAntisenseKmersSize);
memcpy(pPars->pTargSeq,pProbeSeq,SeqLen);
CSeqTrans::ReverseComplement(SeqLen,pPars->pTargSeq);
pTIdx = pPars->pTargSeq;
tsAntisenseKMerOfs *pASKmer;
int AntisenseKMerMsk = 3;
int AntisenseKMerIdx = 0;
for(TIdx=0; TIdx < (UINT32)SeqLen - m_MinSeedCoreLen; TIdx++, pTIdx++)
	{
	AntisenseKMerIdx <<= 2;
	AntisenseKMerIdx |= *pTIdx & 0x03;
	if(TIdx < (UINT32)m_MinSeedCoreLen - 1)
		{
		AntisenseKMerMsk <<= 2;
		AntisenseKMerMsk |= 0x03; 
		continue;
		}
	AntisenseKMerIdx &= AntisenseKMerMsk;  // 0x0fffff
	pASKmer = &pPars->pAntisenseKmers[AntisenseKMerIdx];
	if(pASKmer->MinOfs == 0 && pASKmer->MaxOfs == 0)		// both 0 if this is the first
		pASKmer->MinOfs = TIdx+2-m_MinSeedCoreLen;
	pASKmer->MaxOfs = TIdx+2-m_MinSeedCoreLen;			
	}

MaxAcceptHomoCnt = (m_MinSeedCoreLen * cQualCoreHomopolymer) / 100; // if any core contains more than cQualCoreHomopolymer% of the same base then treat as being a near homopolymer core (likely PacBio artefact) and slough
pQIdx = pProbeSeq;
HitsThisCore = 0;
TotHitsAllCores = 0;
AntisenseKMerIdx = 0;
ChkOvrLapCoreProbeOfs = 0;
ChkOvrLapCoreStartIdx = 0;
LastCoreProbeOfs = 0;
for(QIdx=0; QIdx < (UINT32)SeqLen - m_MinSeedCoreLen; QIdx++, pQIdx++)
	{
	AntisenseKMerIdx <<= 2;
	AntisenseKMerIdx |= *pQIdx & 0x03;
	// with PacBio reads most homopolymer runs are actually artefact inserts so don't bother processing for homopolymer cores
	if(MaxAcceptHomoCnt > 0)
		{
		HomoBaseCnts[0] = HomoBaseCnts[1] = HomoBaseCnts[2] = HomoBaseCnts[3] = 0;
		for(pHomo = pQIdx, HomoIdx = 0; HomoIdx < m_MinSeedCoreLen; HomoIdx+=1, pHomo += 1)
			HomoBaseCnts[*pHomo & 0x03] += 1;
		if(HomoBaseCnts[0] > MaxAcceptHomoCnt || HomoBaseCnts[1] > MaxAcceptHomoCnt || HomoBaseCnts[2] > MaxAcceptHomoCnt || HomoBaseCnts[3] > MaxAcceptHomoCnt)
			continue;
		}
	if(QIdx < (UINT32)m_MinSeedCoreLen - 1)
		continue;
	AntisenseKMerIdx &= AntisenseKMerMsk;
	pASKmer = &pPars->pAntisenseKmers[AntisenseKMerIdx];
	if(pASKmer->MinOfs == 0 && pASKmer->MaxOfs == 0)		// both 0 if no matching antisense Kmer exists
		continue;

	 HitsThisCore = 0;
	// at least one antisense Kmer exists
	int RelQIdx;

	RelQIdx = 1 + QIdx - m_MinSeedCoreLen;
	pTIdx = &pPars->pTargSeq[pASKmer->MinOfs-1];
	for(TIdx=pASKmer->MinOfs-1; TIdx <= pASKmer->MaxOfs-1; TIdx++, pTIdx++)
		{
		for(ChkOvrlapIdx = 0; ChkOvrlapIdx < m_MinSeedCoreLen; ChkOvrlapIdx++)
			if(pProbeSeq[RelQIdx + ChkOvrlapIdx] != pTIdx[ChkOvrlapIdx])
				break;
		if(ChkOvrlapIdx < m_MinSeedCoreLen)
			continue;

		if(cChkOverlapGapLen > 0)	// if > 0 then attempt to collapse overlapping core hits down into a single core hit with extended length
			{
			if(HitsThisCore == 0 && (RelQIdx - LastCoreProbeOfs) > cChkOverlapGapLen)	// if gap between cores was more than cChkOverlapGapLen then assume no overlapping cores need extending
				{
				ChkOvrLapCoreStartIdx = pPars->NumCoreHits;
				ChkOvrLapCoreProbeOfs = RelQIdx;
				}
			LastCoreProbeOfs = RelQIdx;
			// check if this hit is extending a previous hit onto same target loci
			// start from most recent probe overlapping cores with at least one hit
			if(pPars->NumCoreHits && RelQIdx > (int)ChkOvrLapCoreProbeOfs)
				{
				pChkOvrLapCore = &pPars->pCoreHits[ChkOvrLapCoreStartIdx];
		
				for(ChkOvrlapIdx = ChkOvrLapCoreStartIdx; ChkOvrlapIdx < (int)pPars->NumCoreHits; ChkOvrlapIdx++, pChkOvrLapCore++)
					{
					if((pChkOvrLapCore->ProbeOfs + pChkOvrLapCore->HitLen) > (UINT32)RelQIdx &&									// must be overlapping probe core sequence
						(pChkOvrLapCore->TargOfs + RelQIdx - (pChkOvrLapCore->ProbeOfs)) == TIdx)
							{
							pChkOvrLapCore->HitLen = m_MinSeedCoreLen + RelQIdx - pChkOvrLapCore->ProbeOfs;
							break;
							}
					}
				if(ChkOvrlapIdx != pPars->NumCoreHits)
					continue;
				}
			}

		AddCoreHit(SeqID,RelQIdx,TIdx,m_MinSeedCoreLen,pPars);
		HitsThisCore += 1;
		}
	TotHitsAllCores += HitsThisCore;
	}
return(TotHitsAllCores);
}

// SortCoreHitsByProbeTargOfs
// Sort core hits by ProbeOfs.TargOfs ascending, note no need to sort on ProbeNodeID as always same
int
CPBFilter::SortCoreHitsByProbeTargOfs(const void *arg1, const void *arg2)
{
tsFiltCoreHit *pEl1 = (tsFiltCoreHit *)arg1;
tsFiltCoreHit *pEl2 = (tsFiltCoreHit *)arg2;

if(pEl1->ProbeOfs < pEl2->ProbeOfs)	
	return(-1);
if(pEl1->ProbeOfs > pEl2->ProbeOfs)
	return(1);
if(pEl1->TargOfs < pEl2->TargOfs)	
	return(-1);
if(pEl1->TargOfs > pEl2->TargOfs)
	return(1);

return(0);
}
