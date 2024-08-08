/********************************************************************/
/* File:         logger.h                                           */
/* Created:      1 June 1999 by Steven Haworth                      */
/* Last Updated: 20 June 1999                                       */
/* Version History:                                                 */
/*  1 June 1999 - Version 0.0a by Steven Haworth                    */
/*                                                                  */
/* (C) Steven Haworth 1999                                          */
/********************************************************************/
#ifndef loggerH
#define loggerH
//---------------------------------------------------------------------------
#include <syncobjs.hpp>

/* Suggested Log levels:
    1   Fatal Crash Causes
    2   Misc Errors
    3   Program State
    4   Running Status
    5   Notes
*/

/********************************************************************/
/* class TLogger                                                    */
/*                                                                  */
/* inherits from: none                                              */
/* inherited by:  none                                              */
/* aggregates:    TStringList, TCriticalSection, TEvent             */
/*                                                                  */
/* description:                                                     */
/*   This is a thread-safe logging class.  The main method is Log   */
/*   which is based on the printf format for the most flexibility.  */
/*   When Log is first called, the class creates a new thread,      */
/*   which is alive until the object is deleted.                    */
/*                                                                  */
/* notes:                                                           */
/*   There is no error checking in this class which could be added  */
/*   if problems occur for anybody using the class. See the source  */
/*   in logger.cpp for more information on what is happenning,      */
/*   although the defauls should be fine for most people.           */
/********************************************************************/
class TLogger
{
private:
    /* Internal variables. */
    TStringList *Strings;           /* List of logs to be written to file. */
    TCriticalSection *Critical;
    HANDLE hMutex;
    HANDLE hThread;
    TEvent *ActiveEvent;
    TEvent *StopEvent;
    /* Property variables. */
    bool FAddReturns;
    bool FAppend;
    AnsiString FFile;
    int FLevel;
    AnsiString FTimeFormat;
    bool FTimeStamp;
    /* Property functions. */
    bool __fastcall GetActive(void);
public:
    /* Constructors. */
    __fastcall TLogger(AnsiString File,bool Append);
    /* Destructors. */
    __fastcall ~TLogger(void);
    /* Methods. */
    void Log(int Level,const char *fmt,...);
    /* Properties. */
    __property bool Active = { read = GetActive };                                  /* If the log file is currently being written to. */
    __property bool AddReturns = { read = FAddReturns, write = FAddReturns };       /* If returns are to be added to any logs passed to Log. */
    __property bool Append = { read = FAppend };                                    /* Whether the file was appended to or not. */
    __property AnsiString File = { read = FFile };                                  /* The log file being used. */
    __property int Level = { read = FLevel, write = FLevel };                       /* Log level. */
    __property AnsiString TimeFormat = { read = FTimeFormat, write = FTimeFormat }; /* Time format to use for time stamps (see TDateTime help for info). */
    __property bool TimeStamp = { read = FTimeStamp, write = FTimeStamp };          /* Whether a time stamp should be prepended to logs. */
};
//---------------------------------------------------------------------------
#endif
