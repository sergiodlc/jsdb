#include "rslib.h"
#pragma hdrstop

#ifndef NO_MAPI_MAIL

//#include "rs/os.h"
#ifdef __WIN16__
#include <mapi16.h>
#else
#include <mapi.h>
#endif

// hack because of WIN32_LEAN_AND_MEAN=1 defined in tracemonkey
#ifdef XP_WIN
#undef _INC_COMMDLG
#include <Commdlg.h>
#endif

//#define SHOWDEBUG(x) if (GlobalFindAtom("RS_Debug")) {x;}
#define SHOWDEBUG(x) {}

/* we really only MAPI code for sending. Otherwise, CMC works great for reading.
   So, use the GroupWise mail type to read with CMC and send with MAPI.
   Each mail sending gets its own MAPI session!
*/

#ifndef NO_MAPI_MAIL
bool MAPISendDocuments(const char * recip, const char * file,
 const char * subject, const char * textnote,HWND win,
 const char* name, const char * password, bool quiet)
{
SHOWDEBUG(dspMessageBox(textnote,"MAPI send"));
HINSTANCE    hMAPILibrary;
const char * LibName = "MAPI32.DLL";
hMAPILibrary = LoadLibrary(LibName);

if ((UINT) hMAPILibrary < 32) return false;
SHOWDEBUG(dspMessageBox("Library loaded","MAPI"));

typedef HRESULT (FAR PASCAL *MAPIINIT)(LPVOID lpMapiInit);
typedef void (FAR PASCAL *MAPITERM)();
typedef ULONG (FAR PASCAL *MAPILOGON)
   (ULONG ulUIParam,    LPSTR lpszProfileName,
    LPSTR lpszPassword, FLAGS flFlags,
    ULONG ulReserved,   LPLHANDLE lplhSession);

typedef ULONG (FAR PASCAL * MAPISENDMAIL)
   (LHANDLE lhSession,        ULONG ulUIParam,
    lpMapiMessage lpMessage,  FLAGS flFlags,
    ULONG ulReserved);

typedef ULONG (FAR PASCAL * MAPIRESOLVENAME)
   (LHANDLE lhSession,  ULONG ulUIParam,
    LPSTR lpszName,     FLAGS flFlags,
    ULONG ulReserved,   lpMapiRecipDesc FAR *lppRecip);

typedef ULONG (FAR PASCAL * MAPILOGOFF)
   (LHANDLE lhSession,    ULONG ulUIParam,
    FLAGS flFlags,        ULONG ulReserved);

MAPIRESOLVENAME MAPIRESOLVE;
MAPISENDMAIL MAPISEND;
MAPILOGON LOGON;
MAPILOGOFF LOGOFF;
#ifdef __WIN16__
LOGON = (MAPILOGON)GetProcAddress(hMAPILibrary,"MAPILOGON");
if (!LOGON) return false;
LOGOFF = (MAPILOGOFF)GetProcAddress(hMAPILibrary,"MAPILOGOFF");
if (!LOGOFF) return false;
MAPISEND = (MAPISENDMAIL)GetProcAddress(hMAPILibrary,"MAPISENDMAIL");
if (!MAPISEND) return false;
MAPIRESOLVE = (MAPIRESOLVENAME)GetProcAddress(hMAPILibrary,"MAPIRESOLVENAME");
if (!MAPIRESOLVE) return false;
SHOWDEBUG(dspMessageBox("Function addresses loaded","MAPI"));
#else
MAPIINIT INIT = (MAPIINIT)GetProcAddress(hMAPILibrary,"MAPIInitialize");
MAPITERM TERM = (MAPITERM)GetProcAddress(hMAPILibrary,"MAPIUninitialize");
LOGON = (MAPILOGON)GetProcAddress(hMAPILibrary,"MAPILogon");
if (!LOGON) return false;
LOGOFF = (MAPILOGOFF)GetProcAddress(hMAPILibrary,"MAPILogoff");
if (!LOGOFF) return false;
MAPISEND = (MAPISENDMAIL)GetProcAddress(hMAPILibrary,"MAPISendMail");
if (!MAPISEND) return false;
MAPIRESOLVE = (MAPIRESOLVENAME)GetProcAddress(hMAPILibrary,"MAPIResolveName");
if (!MAPIRESOLVE) return false;
struct {
    ULONG           ulVersion;
    ULONG           ulFlags;
} Init;
Init.ulVersion = 0;
Init.ulFlags   = 0;
if (INIT) INIT(&Init);
#endif


unsigned long mapists;
unsigned long Session;

mapists = LOGON((uint32)win,(char*)name,(char*)password,MAPI_NEW_SESSION,0,&Session);
if (mapists) mapists = LOGON((uint32)win,0,0,MAPI_LOGON_UI,0,&Session);

if (mapists) return false;
#define MAPI_UNICODE 0x80000000

TAPointer<MapiFileDesc> FileDesc(0);
TStringList FL(file?file:"",",");
TStringList Titles;
size_t FileCount = FL.Count();
if (FileCount)
{
 FileDesc = new MapiFileDesc[FileCount];
 TStr FileTitle(MAXPATH);
 FOREACH(char * c,FL)
  GetFileTitle(c, FileTitle, MAXPATH);
  FileDesc[i].ulReserved   = 0;
  FileDesc[i].flFlags      = 0;
  FileDesc[i].nPosition    = UINT_MAX; //0xffffffff
  FileDesc[i].lpszPathName = (char*) c;
  FileDesc[i].lpszFileName = (char*) Titles[Titles.Add(FileTitle)];
  FileDesc[i].lpFileType  = 0;
 DONEFOREACH
}

WStr r(recip);
bool AnyRecipient = (recip && *recip);
MapiRecipDesc * RecipDesc = 0;
if (AnyRecipient)
 {
    mapists = MAPIRESOLVE(
          Session,
          (uint32)win,
          (char*) (wchar_t*)r, //recip,
          MAPI_DIALOG,
          0UL|MAPI_UNICODE,
          &RecipDesc);

   if (mapists == 0)
   {
   SHOWDEBUG(dspMessageBox("Mail will be sent to",RecipDesc->lpszAddress));
   RecipDesc->ulRecipClass = MAPI_TO;
   }
 }

WStr s(subject);
WStr t(textnote);

MapiMessage Message;
 Message.ulReserved        = 0;
 Message.lpszSubject       = (char*) (wchar_t*)s; //subject;
 Message.lpszNoteText      = (char*) (wchar_t*)t;//textnote;
// Message.lpszSubject       = (char*) subject;
// Message.lpszNoteText      = (char*) textnote;
 Message.lpszMessageType   = 0; // "CMC:IPM"
 Message.lpszDateReceived  = 0;
 Message.lpszConversationID= 0;
 Message.flFlags           = MAPI_UNICODE; //0;
 Message.lpOriginator      = 0;
 Message.nRecipCount       = (AnyRecipient) ? 1 : 0;
 Message.lpRecips          = RecipDesc;
 Message.nFileCount        = FileCount;
 Message.lpFiles           = FileCount ? &(FileDesc[0]) : (MapiFileDesc*)0;

uint32 flags = 0;
if ((!recip || !RecipDesc) && (!quiet)) flags = MAPI_DIALOG;

       mapists = MAPISEND(Session,
                          (uint32)win,
                          &Message,
                          flags|MAPI_UNICODE,
                          0);

//LOGOFF(Session,(uint32)win,0,0);

if (TERM) TERM();

SHOWDEBUG(dspMessageBox("Function returned","MAPI"));

FreeLibrary(hMAPILibrary);

return (mapists == 0);
} //MAPISend
#endif
#endif
