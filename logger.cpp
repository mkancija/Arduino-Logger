/********************************************************************/
/* File:         logger.cpp                                         */
/* Created:      1 June 1999 by Steven Haworth                      */
/* Last Updated: 20 June 1999                                       */
/* Version History:                                                 */
/*  1 June 1999 - Version 0.0a by Steven Haworth                    */
/*                                                                  */
/* (C) Steven Haworth 1999                                          */
/********************************************************************/
#include <stdarg.h>
#include <stdio.h>
#include <vcl.h>
#pragma hdrstop

#include "logger.h"

#define RELEASE_PTR(ptr) if(ptr) { delete ptr; ptr = NULL; }

/* Names used for mutexes and events used in the class. */
#define LOGGER_MUTEX_NAME   "TLoggerMutex"
#define LOGGER_EVENT_ACTIVE "TLoggerActive"
#define LOGGER_EVENT_STOP   "TLoggerStop"

/* This structure is passed to the logging thread and contains all the
   required info for the thread to get to work. */
typedef struct {
    char      Filename[MAX_PATH];
    TStrings *Msgs;
} LoggerStruct;

/********************************************************************/
/* LoggerDoWrite                                                    */
/*                                                                  */
/* arguments:                                                       */
/*   File - the file to append the line in Msg to.                  */
/*   Msg - the log message to append to File.                       */
/********************************************************************/
void LoggerDoWrite(const char *File,const char *Msg)
{
    TFileStream *Log;

    try {
        /* Check if the file exists. If so, open the file for appending,
           otherwise, create the file.
           Also, allow other programs to read the file, but not write to it. */
        if(FileExists(File))
            Log = new TFileStream(File,fmOpenReadWrite | fmShareDenyWrite);
        else
            Log = new TFileStream(File,fmCreate | fmShareDenyWrite);
    }
    /* Don't care about any errors at the moment, so just ignore. */
    catch(EFOpenError& Error) {
        return;
    }
    catch(EFCreateError& Error) {
        return;
    }
    /* Go to end of the file, and add Msg. */
    Log->Seek(0,soFromEnd);
    Log->Write(Msg,strlen(Msg));
    RELEASE_PTR(Log);
}
/********************************************************************/
/* LoggerThread                                                     */
/*                                                                  */
/* arguments:                                                       */
/*   Arg - contains a pointer to LoggerStruct. This function        */
/*         releases the structure but not the TStrings * on exit.   */
/********************************************************************/
DWORD WINAPI LoggerThread(LPVOID Arg)
{
    /* Cast the argument to LoggerStruct. */
    LoggerStruct *ls = static_cast<LoggerStruct *>(Arg);
    HANDLE hMutex,hActive,hStop;

    /* This Mutex is used to control access to the Msgs TStrings
       object as it is not thread safe by default. */
    hMutex = OpenMutex(MUTEX_ALL_ACCESS,FALSE,LOGGER_MUTEX_NAME);
    if(hMutex == INVALID_HANDLE_VALUE) {
        RELEASE_PTR(ls);
        return -1;
    }
    /* This Event is used to tell other threads whether this thread is
       currently writing to the log file.  It is set on active. */
    hActive = OpenEvent(EVENT_ALL_ACCESS,FALSE,LOGGER_EVENT_ACTIVE);
    if(hActive == INVALID_HANDLE_VALUE) {
        CloseHandle(hMutex);
        RELEASE_PTR(ls);
        return -1;
    }
    /* This Event is used by the TLogger class to tell the thread to
       finish writing the Log entries and exit the thread. */
    hStop = OpenEvent(EVENT_ALL_ACCESS,FALSE,LOGGER_EVENT_STOP);
    if(hStop == INVALID_HANDLE_VALUE) {
        CloseHandle(hActive);
        CloseHandle(hMutex);
        RELEASE_PTR(ls);
        return -1;
    }
    do {
        AnsiString Msg;

        /* Wait 1 sec for access to the Msgs object. */
        switch(WaitForSingleObject(hMutex,1000)) {
        /* If abandoned, then the TLogger thread has been destroyed, so
           just exit straight away. */
        case WAIT_ABANDONED:
            RELEASE_PTR(ls);
            CloseHandle(hMutex);
            CloseHandle(hActive);
            CloseHandle(hStop);
            return -1;
        /* Got access to object, so see if there are logs to write. */
        case WAIT_OBJECT_0:
            if(ls->Msgs->Count > 0) {
                Msg = ls->Msgs->Strings[0];
                ls->Msgs->Delete(0);    /* Remove log from list. */
                ReleaseMutex(hMutex);   /* No longer need control of Msgs, so release it. */
                SetEvent(hActive);      /* Set active event whilst writing. */
                LoggerDoWrite(ls->Filename,Msg.c_str());
                ResetEvent(hActive);
            }
            else {
                ReleaseMutex(hMutex);   /* No messages, so just release Msgs. */
            }
            /* Give up current time slice to reduce system load by this thread. */
            Sleep(0);
            /* Fall through here. */
        /* Timed out on access to object, so see if we have been told to exit. */
        case WAIT_TIMEOUT:
            /* Just quickly check the event state. */
            if(WaitForSingleObject(hStop,0) == WAIT_OBJECT_0) {
                /* Get control of Msgs, and write the rest of the logs. */
                if(WaitForSingleObject(hMutex,0) == WAIT_OBJECT_0) {
                    while(ls->Msgs->Count > 0) {
                        LoggerDoWrite(ls->Filename,ls->Msgs->Strings[0].c_str());
                        ls->Msgs->Delete(0);
                    }
                    ReleaseMutex(hMutex);
                }
                /* Leave thread in proper manner. */
                RELEASE_PTR(ls);
                CloseHandle(hMutex);
                CloseHandle(hActive);
                CloseHandle(hStop);
                return 0;
            }
            break;
        }
    } while(1);
}
/********************************************************************/
/* TLogger::TLogger                                                 */
/*                                                                  */
/* arguments:                                                       */
/*   File - the name of the log file to use for this object.  This  */
/*          is not changeable during the life of the object, so     */
/*          more objects should be created for writing to other     */
/*          files.                                                  */
/*   Append - whether the object should add logs to the file        */
/*            without deleting it first.                            */
/********************************************************************/
__fastcall TLogger::TLogger(AnsiString File,bool Append)
{
    Strings = new TStringList;
    Critical = new TCriticalSection;
    hMutex = CreateMutex(NULL,FALSE,LOGGER_MUTEX_NAME);
    hThread = NULL;
    ActiveEvent = new TEvent(NULL,TRUE,FALSE,LOGGER_EVENT_ACTIVE);
    StopEvent = new TEvent(NULL,TRUE,FALSE,LOGGER_EVENT_STOP);
    FAddReturns = false;
    FAppend = Append;
    FFile = File;
    FLevel = 5;
    FTimeFormat = "dd/mm/yy hh:nn:ss ";
    FTimeStamp = true;
}
/********************************************************************/
/* TLogger::~TLogger                                                */
/********************************************************************/
__fastcall TLogger::~TLogger(void)
{
    /* If thread has been created, then set the stop event and wait for
       the thread to finish.  After 10 sec, we just quit anyway as something
       strange has happened. */
    if(hThread != NULL) {
        StopEvent->SetEvent();
        WaitForSingleObject(hThread,10000);
        CloseHandle(hThread);
    }
    RELEASE_PTR(Critical);
    RELEASE_PTR(Strings);
    CloseHandle(hMutex);
    RELEASE_PTR(ActiveEvent);
    RELEASE_PTR(StopEvent);
}
/********************************************************************/
/* TLogger::GetActive                                               */
/*                                                                  */
/* Property getter function.                                        */
/********************************************************************/
bool __fastcall TLogger::GetActive(void)
{
    /* Just checks the state of the active event. */
    return (ActiveEvent->WaitFor(0) == wrSignaled);
}
/********************************************************************/
/* TLogger::Log                                                     */
/*                                                                  */
/* arguments:                                                       */
/*   Level - level for this log.  Object only writes logs whose     */
/*           level is less than or equal to the Level property.     */
/*   fmt - format of log.  Same as printf functions (see online     */
/*         help for info).                                          */
/*   ... - arguments matching fmt string.                           */
/********************************************************************/
void TLogger::Log(int Level,const char *fmt,...)
{
    AnsiString m;

    /* This is a critical section of code that only one thread should
       go through at a time, so it is protected by a TCriticalSection
       object.  (This could be done by controlling the Msgs Mutex only,
       but this would interfere with the thread for more time than
       would be necessary.) */
    Critical->Enter();
    /* If the levels don't match up, then just ignore. */
    if(Level <= FLevel) {
        va_list ap;
        char szBuf[1024];

        /* If we are not appending to the file, then delete it. */
        if(!FAppend)
            DeleteFile(FFile);
        /* Use the v_ functions to get the printf functionality of the function. */
        va_start(ap,fmt);
        vsprintf(szBuf,fmt,ap);
        va_end(ap);
        /* Prepend a timestamp if required. */
        if(FTimeStamp)
            m = Now().FormatString(FTimeFormat);
        m += AnsiString(szBuf);
        /* Append linefeed-newline if required. */
        if(FAddReturns)
            m += AnsiString("\r\n");
        /* Now get control of the Msgs mutex for adding logs to the list. */
        if(WaitForSingleObject(hMutex,INFINITE) == WAIT_OBJECT_0) {
            Strings->Add(m);
            ReleaseMutex(hMutex);
            /* If thread hasn't been started yet, then start it! */
            if(hThread == NULL) {
                /* Create a LoggerStruct and fill in the details. */
                LoggerStruct *ls = new LoggerStruct;
                DWORD ThreadID;
                strcpy(ls->Filename,FFile.c_str());
                ls->Msgs = Strings;
                /* We don't do anything if the CreateThread fails, the next
                   call will try to start it again, with no logs lost. */
                hThread = CreateThread(NULL,0,LoggerThread,ls,0,&ThreadID);
            }
        }
    }
    /* Release critical section object. */
    Critical->Leave();
}
//---------------------------------------------------------------------------

