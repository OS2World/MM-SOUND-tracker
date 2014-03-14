/* main.c (formerly str32.c) */

/* modified (rewritten) by David Nichols for PM MOD player */

/* original authors:
 * Authors  : Liam Corner - zenith@dcs.warwick.ac.uk
 *            Marc Espie - espie@dmi.ens.fr
 *            Steve Haehnichen - shaehnic@ucsd.edu
*/

#include <stdio.h>

#define INCL_DOS
#define INCL_WIN
#define INCL_GPI

#include <os2.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "sblast_user.h"
#include "defs.h"
#include "os2defs.h"
#include "tracker.h"
#include "easyfont.h"

#define LCID_MYFONT 1

HWND hwndControl;
HWND hwndSong;
HWND hwndTime;
HWND hwndName;
HWND hwndQueue;
HWND hwndChan1, hwndChan2, hwndChan3, hwndChan4;

HPS hpsSong;
HPS hpsTime;
HPS hpsName;
HPS hpsQueue;
HPS hpsChan1, hpsChan2, hpsChan3, hpsChan4;

RECTL rclTime, rclSong, rclName, rclQueue,
	rclChan1, rclChan2, rclChan3, rclChan4;

HAB hab;
HMQ hmq;

HSWITCH hSwitch;                /* Switch entry handle        */

int quiet = 0;                  /* Global flag for no text output */
int suspended = 0;              /* flag for pause mode */
int abortsong = 0;                  /* abort flag */
int blink;
int pauseack = 0;
int terminate = 0;
int iconic = 0;

struct pref pref;               /* Global user preferences */

int error_flag = 0;

int priority = 0;

/* global variable to catch various types of errors
 * and achieve the desired flow of control
 */
int error;
/* small hack for transposing songs on the fly */
static int transpose;

char *msong[MAXSONGS];

int numsongs = 0,
   songnum = 0,
   frequency, 
   oversample;

int fwidth, fheight;

USHORT DMAbuffersize = 63;

char playermode[32] = " ";

char title[] = "Tracker/PM";
char paused[] = " (paused)";

char progname[128] = "Tracker";

char programpath[256];
char currentdisk[3] = "c:";
char currentdir[256];

char usage[] = \
  "usage: %s [-][options] filename [filename [...]]\n"
  "-h: Help; display usage information\n"
  "-i: Iconic; start as an icon\n"
  "-m: Mono; select single audio channel output\n"
  "-s: Stereo; select dual audio channel output\n"
  "-n: New; select new MOD type for mod playing\n"
  "-o: Old; select old MOD type for mod playing\n"
  "-b: Both; select both MOD types to try (default is -both)\n"
  "-T: Terminate; terminate after playing all MODs\n"
  "-L: Low priority; sets tracker to normal low priority (default)\n"
  "-M: Middle priority; sets tracker to \"foregroundserver\" priority\n" 
  "-H: Highest priority; sets tracker to \"timecritical\" priority\n"
  "-dnum: DMA buffer size; set DMA buffer size in K.\n"
  "-fnum: Frequency; sets playback frequency to <num> Hz.\n"
  "-tnum: Transpose all notes up <num> half-steps\n"
  "-rnum: Repeat; repeats <num> number of repeats (0 is forever) (default 1)\n"
  "-Bnum: Blend; sets percent of channel mixing to <num>. (0=spatial, 100=mono)\n"
  "-Onum: Oversample; set oversampling to <num> times.\n"
  "-Snum: Speed; set song speed to <speed>.  Some songs want 60 (default 50)\n"
  "%s: OS/2 2.0 32bit MOD player\n";

FILE *debug;
int logerrors = 1;

void quit(int rv)
{
   restoreparams();
   close_audio();
   WinDestroyWindow(hwndControl);
   WinDestroyMsgQueue(hmq);
   WinTerminate(hab);
   exit(rv);
}

void trackerror(int err, char *txt, int errtype)
{
   if (logerrors) fprintf(debug, "ERROR %d: %s\n", err, txt);
   if (errtype == FATAL_ERROR) quit(err);
}

struct song *do_read_song(char *s, int type)
{
  struct song *song;
  FILE *fp;

  fp = fopen (s, "rb");
  if (fp == NULL)
    {
      trackerror(1, "Unable to open tune file", NONFATAL_ERROR);
      return NULL;
    }
  song = read_song (fp, type, transpose);
  fclose (fp);
  
  return song;
}

char *strtolower(wordv)
char *wordv;
{
   int loop = 0;

   while(wordv[loop])
   {
      if (isupper(wordv[loop])) wordv[loop] = tolower(wordv[loop]);
      loop++;
   }
   return wordv;
}

void checkext(char *fn)
{
   int l;

   strtolower(fn);
   l = strlen(fn);
   if ((l < 5)||(strcmp(&fn[l-4],".mod"))) 
      if (fn[l] != '.') strcat(fn, ".mod");
}

void getpname(char *argv[])
{
   int cp;
   char t[2] = { '\0', '\0' };

   programpath[0] = '\0';
   cp = strlen(argv[0]);
   if (cp)
   {
      while (cp && (argv[0][cp] != '\\') && (argv[0][cp] != ':')) cp--;
      if (cp)
      {
         t[0] = argv[0][cp];
	 argv[0][cp] = '\0';
         cp++;
      }
      strcpy(progname, &argv[0][cp]);
      strtolower(progname);
      cp = strlen(progname);
      while (cp && (progname[cp] != '.')) cp--;
      if (progname[cp] == '.') progname[cp] = '\0';
      strcpy(programpath, argv[0]);
      strcat(programpath, t);
   }
}


void fixfn(char *fn)
{
   int cp;
   char buf[strlen(fn)];

   cp = strlen(fn);
   if (cp)
   {
      while (cp && (fn[cp] != '\\') && (fn[cp] != ':')) cp--;
      strcpy(buf, &fn[cp+1]);
      strcpy(fn, buf);
   }
}

void gotodir(char *path)
{
   int cp;
   char buf[strlen(path)+1];
   char *p;

   strcpy(buf, path);
   p = buf;
   if (p[1] == ':')
   {
      if (islower(p[0])) p[0] = toupper(p[0]);
      if (isupper(p[0])) 
      {
/*	 DosSetDefaultDisk(p[0]-'A'); */
	 currentdisk[0] = p[0];
      }
      p += 2;
   }
   cp = strlen(p);
   if (cp)
   {
      while (cp && (p[cp] != '\\') && (p[cp] != ':')) cp--;
      p[cp+1] = '\0';
      strcpy(currentdir, p);
/*      DosSetCurrentDir(p); */
   }   
}

int OpenDlg(HWND hwndOwner)
{
   FILEDLG fild;         /* File dialog structure. */

   memset(&fild, 0, sizeof(FILEDLG));
   fild.cbSize     = sizeof(FILEDLG);
   fild.fl         = FDS_OPEN_DIALOG|FDS_CENTER;
   fild.pszIDrive  = currentdisk;
   strcpy(fild.szFullFile, currentdir);
   strcat(fild.szFullFile, "*.mod");
   fild.pszTitle = "Queue Song";
   fild.pszOKButton = "Queue";
   WinFileDlg(HWND_DESKTOP, hwndOwner, &fild);
   if (fild.lReturn == DID_OK)
   {
      DosAllocMem((PVOID) &msong[numsongs], strlen(fild.szFullFile)+1,
		  PAG_READ|PAG_WRITE|PAG_COMMIT);
/*    msong[numsongs] = (char *)malloc(strlen(fild.szFullFile)+1); */
      strcpy(msong[numsongs], fild.szFullFile);
      gotodir(msong[numsongs]);
      numsongs++;
      PrintSong(NULL);
      return 1;
   }
   return 0;
}


MRESULT AboutDlgProc(HWND hwnd, USHORT msg, MPARAM mp1, MPARAM mp2)
{
   switch(msg)
   {
      case WM_INITDLG:
	 break;

      case WM_COMMAND:
 	 switch(SHORT1FROMMP(mp1))
  	 {
	    case MBID_OK:
               break;
	 }
	 break;
   }
   return WinDefDlgProc(hwnd, msg, mp1, mp2);
}

MRESULT ControlDlgProc(HWND hwnd, USHORT msg, MPARAM mp1, MPARAM mp2)
{
   static TID tid;
   static THREADPARAMS tp;
   static struct song *song = NULL;
   static char buf[256];
   static HRGN hrgnSong;
   static HRGN hrgnTime;
   static HRGN hrgnName;
   static HRGN hrgnQueue;
   static HRGN hrgnChan1;
   static HRGN hrgnChan2;
   static HRGN hrgnChan3;
   static HRGN hrgnChan4;
   static HRGN hrgnTemp;
   static int x;
   static FONTMETRICS fm;
   static REAL_SWCNTRL SwitchData;        /* Switch control data block  */

   switch (msg)
   {
      case WM_INITDLG:
         DosCreateEventSem("\\sem32\\done", &tp.hevDone, 0, (BOOL32) FALSE);
	 DosCreateEventSem("\\sem32\\pause", &tp.hevPause, 0, (BOOL32) FALSE);
	 tp.fPlaying = FALSE;
	 hwndSong = WinWindowFromID(hwnd, ID_SONGRECT);
	 hwndTime = WinWindowFromID(hwnd, ID_TIMERECT);
	 hwndName = WinWindowFromID(hwnd, ID_NAMERECT);
	 hwndQueue = WinWindowFromID(hwnd, ID_QUEUERECT);
	 hwndChan1 = WinWindowFromID(hwnd, ID_CHAN1RECT);
	 hwndChan2 = WinWindowFromID(hwnd, ID_CHAN2RECT);
	 hwndChan3 = WinWindowFromID(hwnd, ID_CHAN3RECT);
	 hwndChan4 = WinWindowFromID(hwnd, ID_CHAN4RECT);
	 hpsSong = WinGetPS(hwndSong);
	 hpsTime = WinGetPS(hwndTime);
	 hpsName = WinGetPS(hwndName);
	 hpsQueue = WinGetPS(hwndQueue);
	 hpsChan1 = WinGetPS(hwndChan1);
	 hpsChan2 = WinGetPS(hwndChan2);
	 hpsChan3 = WinGetPS(hwndChan3);
	 hpsChan4 = WinGetPS(hwndChan4);
	 WinQueryWindowRect(hwndSong, &rclSong);
	 WinQueryWindowRect(hwndTime, &rclTime);
	 WinQueryWindowRect(hwndName, &rclName);
	 WinQueryWindowRect(hwndQueue, &rclQueue);
	 WinQueryWindowRect(hwndChan1, &rclChan1);
	 WinQueryWindowRect(hwndChan2, &rclChan2);
	 WinQueryWindowRect(hwndChan3, &rclChan3);
	 WinQueryWindowRect(hwndChan4, &rclChan4);
	 hrgnSong = GpiCreateRegion(hpsSong, 1, &rclSong);
	 hrgnTime = GpiCreateRegion(hpsTime, 1, &rclTime);
	 hrgnName = GpiCreateRegion(hpsName, 1, &rclName);
	 hrgnQueue = GpiCreateRegion(hpsQueue, 1, &rclQueue);
	 hrgnChan1 = GpiCreateRegion(hpsChan1, 1, &rclChan1);
	 hrgnChan2 = GpiCreateRegion(hpsChan2, 1, &rclChan2);
	 hrgnChan3 = GpiCreateRegion(hpsChan3, 1, &rclChan3);
	 hrgnChan4 = GpiCreateRegion(hpsChan4, 1, &rclChan4);
	 GpiSetClipRegion(hpsSong, hrgnSong, hrgnTemp);
	 GpiSetClipRegion(hpsTime, hrgnTime, hrgnTemp);
	 GpiSetClipRegion(hpsName, hrgnName, hrgnTemp);
	 GpiSetClipRegion(hpsQueue, hrgnQueue, hrgnTemp);
	 GpiSetClipRegion(hpsChan1, hrgnChan1, hrgnTemp);
	 GpiSetClipRegion(hpsChan2, hrgnChan2, hrgnTemp);
	 GpiSetClipRegion(hpsChan3, hrgnChan3, hrgnTemp);
	 GpiSetClipRegion(hpsChan4, hrgnChan4, hrgnTemp);
         GpiCreateLogColorTable(hpsSong, LCOL_PURECOLOR, LCOLF_RGB,
                0L, 0L, NULL);
         GpiCreateLogColorTable(hpsTime, LCOL_PURECOLOR, LCOLF_RGB,
                0L, 0L, NULL);
         GpiCreateLogColorTable(hpsName, LCOL_PURECOLOR, LCOLF_RGB,
                0L, 0L, NULL);
         GpiCreateLogColorTable(hpsQueue, LCOL_PURECOLOR, LCOLF_RGB,
                0L, 0L, NULL);
         GpiCreateLogColorTable(hpsChan1, LCOL_PURECOLOR, LCOLF_RGB,
                0L, 0L, NULL);
         GpiCreateLogColorTable(hpsChan2, LCOL_PURECOLOR, LCOLF_RGB,
                0L, 0L, NULL);
         GpiCreateLogColorTable(hpsChan3, LCOL_PURECOLOR, LCOLF_RGB,
                0L, 0L, NULL);
         GpiCreateLogColorTable(hpsChan4, LCOL_PURECOLOR, LCOLF_RGB,
                0L, 0L, NULL);
	 EzfQueryFonts(hpsSong);
	 EzfQueryFonts(hpsName);
         EzfCreateLogFont(hpsSong, LCID_MYFONT, FONTFACE_TIMES, 
                                 FONTSIZE_10, 0) ;         
         EzfCreateLogFont(hpsName, LCID_MYFONT, FONTFACE_TIMES, 
                                 FONTSIZE_10, 0) ;         
	 GpiSetCharSet(hpsSong, LCID_MYFONT);
	 GpiSetCharSet(hpsName, LCID_MYFONT);
	 strcpy(buf, programpath);
	 strcat(buf, "lcd.fon");
	 if (!GpiLoadFonts(hab, buf)) trackerror(1, "can't load font", FATAL_ERROR);
	 EzfQueryFonts(hpsTime);
	 EzfQueryFonts(hpsQueue);
         EzfCreateLogFont(hpsTime, LCID_MYFONT, FONTFACE_SYSTEM,
                                 FONTSIZE_10, 0) ;         
         EzfCreateLogFont(hpsQueue, LCID_MYFONT, FONTFACE_SYSTEM,
                                 FONTSIZE_10, 0) ;         
	 GpiSetCharSet(hpsTime, LCID_MYFONT);
	 GpiSetCharSet(hpsQueue, LCID_MYFONT);
	 GpiQueryFontMetrics(hpsSong, sizeof(FONTMETRICS), &fm);
	 fwidth = (USHORT)(1.2*fm.lAveCharWidth);
         fheight= (USHORT)(fm.lExternalLeading+fm.lMaxBaselineExt);
	 playermode[0] = PLAYER_STOP;

	 if (numsongs) WinPostMsg(hwnd,WM_COMMAND,MPFROM2SHORT(IDM_PLAY,0),0);
	 break;

      case WM_SHOW:
      case WM_ENABLE:
      case WM_ACTIVATE:
         DosEnterCritSec();

/*	 WinQueryWindowRect(hwndSong, &rclSong);
	 WinFillRect(hpsSong, &rclSong, CLR_BLACK);
	 WinQueryWindowRect(hwndName, &rclName);
	 WinFillRect(hpsName, &rclName, CLR_BLACK);
	 WinQueryWindowRect(hwndTime, &rclTime);
	 WinFillRect(hpsTime, &rclTime, CLR_BLACK); 
*/
	 if (tp.fPlaying) PrintSong(tp.fn);
	 else PrintSong(NULL);
	 
	 DosExitCritSec();
	 break;
       
      case WM_CLOSE:
       	 if (tp.fPlaying) WinSendMsg(hwnd, WM_COMMAND, MPFROM2SHORT(IDM_STOP,0), 0);
	 return WinDefWindowProc(hwnd, msg, mp1, mp2);

      case WM_DESTROY:
         WinReleasePS(hpsSong);
	 WinReleasePS(hpsTime);
	 WinReleasePS(hpsName);
	 WinReleasePS(hpsQueue);
	 WinReleasePS(hpsChan1);
	 WinReleasePS(hpsChan2);
	 WinReleasePS(hpsChan3);
	 WinReleasePS(hpsChan4);
	 return 0;

      case WM_COMMAND:
	 switch (SHORT1FROMMP (mp1))
	 {
	    case IDM_OPEN:
	       if (OpenDlg(hwnd)&&!suspended) WinSendMsg(hwnd,WM_COMMAND,MPFROM2SHORT(IDM_PLAY,0),0);
	       return 0;

	    case IDM_PAUSE:
	       if (suspended) WinSendMsg(hwnd,WM_COMMAND,MPFROM2SHORT(IDM_PLAY,0),0);
	       else if (tp.fPlaying)
	       {
		  blink = 1;
		  DosResetEventSem(tp.hevPause, &x);
		  pauseack = 1;
		  WinQuerySwitchEntry(hSwitch, (PSWCNTRL) &SwitchData);
		  strcpy(buf, title);
		  strcat(buf, " - ");
		  strcat(buf, tp.fn);
		  strcat(buf, paused);
		  strcpy(SwitchData.szSwtitle, buf);
		  WinChangeSwitchEntry(hSwitch, (PSWCNTRL) &SwitchData);
		  WinSetWindowText(hwndControl, buf);
	       }
	       return 0;

	    case IDM_FF:
	    case IDM_FR:

	    case IDM_PREV:
	    case IDM_NEXT:
	       if (SHORT1FROMMP(mp1)==IDM_PREV)
	       {
		  songnum -= 2;
		  if (songnum < 0) songnum = 0;
		  if (tp.fPlaying)
		  {
		     playermode[0] = PLAYER_REWIND;
		     PrintSong(NULL);
 		  }
	       }
	       else if (tp.fPlaying)
	       {
	          playermode[0] = PLAYER_FASTFORWARD;
		  PrintSong(NULL);
	       }
	       if (tp.fPlaying) WinSendMsg(hwnd, WM_COMMAND, MPFROM2SHORT(IDM_STOP,0), 0);
	       WinSendMsg(hwnd, WM_COMMAND, MPFROM2SHORT(IDM_PLAY,0), 0);
	       return 0;

            case IDM_RESTART:
	       if (tp.fPlaying)
	       {
	          WinSendMsg(hwnd, WM_COMMAND, MPFROM2SHORT(IDM_STOP,0), 0);
		  songnum--;
	          WinSendMsg(hwnd, WM_COMMAND, MPFROM2SHORT(IDM_PLAY,0), 0);
	       }
	       return 0;

	    case IDM_STOP:
	       if (tp.fPlaying)
	       {
		  blink = TRUE;
		  abortsong = TRUE;
		  tp.fTerminate = TRUE;
		  if (suspended) 
		  {
		     DosPostEventSem(tp.hevPause);
/*		     DosResumeThread(tid); */
		     WinQuerySwitchEntry(hSwitch, (PSWCNTRL) &SwitchData); 
		     strcpy(SwitchData.szSwtitle, title);
		     WinChangeSwitchEntry(hSwitch, (PSWCNTRL) &SwitchData);
		     WinSetWindowText(hwndControl,title);
		     suspended = 0;
		  }
		  DosWaitEventSem(tp.hevDone, SEM_INDEFINITE_WAIT);
	       }
	       return 0;

	    case IDM_PLAY:
	       if (tp.fPlaying)
		  if (suspended)
		  {
		     DosPostEventSem(tp.hevPause);
/*		     DosResumeThread(tid); */
		     WinQuerySwitchEntry(hSwitch, (PSWCNTRL) &SwitchData); 
		     strcpy(buf,title);
		     strcat(buf, " - ");
		     strcat(buf, tp.fn);
		     strcpy(SwitchData.szSwtitle, buf);
		     WinChangeSwitchEntry(hSwitch, (PSWCNTRL) &SwitchData);
		     WinSetWindowText(hwnd,buf);
		     suspended = 0;
		     playermode[0] = PLAYER_PLAY;
		     PrintSong(NULL);
		     return 0;
		  }
	          else return 0;
	       if (songnum < numsongs)
	       {
		  switch (pref.type)
		  {
		     case BOTH:
		        song = do_read_song(msong[songnum], NEW);
		        if (!song) song = do_read_song (msong[songnum], OLD);
		        break;
		     case OLD:
		        song = do_read_song (msong[songnum], pref.type);
		        break;
		     case NEW:
		        song = do_read_song (msong[songnum], NEW_NO_CHECK);
		        break;
		  }
		  if (song != NULL)
		  {
		     DosResetEventSem(tp.hevDone, &x);
		     tp.fTerminate = FALSE;
		     tp.song = song;
		     tp.pref = &pref;
		     tp.fPlaying = TRUE;
		     tp.hwndControl = hwnd;
		     DosAllocMem((PVOID) &tp.fn, strlen(msong[songnum])+1,
				 PAG_READ|PAG_WRITE|PAG_COMMIT);
		     strcpy(tp.fn, msong[songnum]);
		     fixfn(tp.fn);
		     playermode[0] = PLAYER_PLAY;
		     PrintSong(NULL);
		     DosCreateThread(&tid, PlaySong, (ULONG)&tp, 0, 0x4000);
		     if (priority) DosSetPriority(PRTYS_THREAD,priority,0,tid);
		     WinQuerySwitchEntry(hSwitch, (PSWCNTRL) &SwitchData); 
		     strcpy(buf,title);
		     strcat(buf, " - ");
		     strcat(buf, tp.fn);
		     strcpy(SwitchData.szSwtitle, buf);
		     WinChangeSwitchEntry(hSwitch, (PSWCNTRL) &SwitchData);
		     WinSetWindowText(hwnd,buf);
		  }
		  songnum++;
	       }
	       else
	       {
		  playermode[0] = PLAYER_STOP;
		  PrintSong(NULL);
   		  if (terminate) WinSendMsg(hwnd, WM_CLOSE, 0, 0);
	       }
	       return 0;

	    case IDM_EXIT:
	       WinSendMsg (hwnd, WM_CLOSE, 0L, 0L);
	       return 0;

            case IDM_ABOUT:
	       DosEnterCritSec();

               WinLoadDlg(HWND_DESKTOP, hwnd, AboutDlgProc, NULL, IDM_ABOUT, NULL);

	       DosExitCritSec();
               return 0;
	 }
	 break;
   }
   return WinDefDlgProc(hwnd, msg, mp1, mp2);
}

void getcd(char *cdfile)
{
   FILE *cdf;
   char buf[256];

   if ((cdf = fopen(cdfile, "rt")) == NULL) return;
   while (!feof(cdf))
   {
      fscanf(cdf, "%s", buf);
      msong[numsongs] = (char *)malloc(strlen(buf)+5);
      strcpy(msong[numsongs], buf);
      checkext(msong[numsongs]);
      numsongs++;
   }
   fclose(cdf);
}

int checkcd(char *fn)
{
   int x;
   char buf[4];

   if ((x = strlen(fn)) < 4) return 0;
   strcpy(buf, &fn[x-3]);
   strtolower(buf);
   return !strcmp(buf, ".cd");
}

void parsecommandline(int argc, char *argv[])
{
   int i, j, count;

   if (argc) for (i=1; i<argc; i++)
      {
	 if (argv[i][0] == '@') getcd(&argv[i][1]);
	 else if (checkcd(argv[i])) getcd(argv[i]);
	 else if (argv[i][0] != '-') 
	 {
	    msong[numsongs] = (char *)malloc(strlen(argv[i])+5);
	    strcpy(msong[numsongs], argv[i]);
	    checkext(msong[numsongs]);
	    numsongs++;
	 }
	 else
	 {
	    count = strlen(argv[i]);
	    for(j=1;j<count;j++)
	    {
	       switch (argv[i][j])
	       {
/*		  case 'h': help(); break; */
		  case 'i': iconic = TRUE; break;
		  case 'p': pref.tolerate = 0; break; /* tolerate faults */
		  case 'n': pref.type = NEW; break; /* new mod type */
	     	  case 'o': pref.type = OLD; break; /* old mod type */
		  case 'b': pref.type = BOTH; break; /* try both types */
		  case 'm': pref.stereo = 0; break; /* mono */
		  case 's': pref.stereo = 1; break; /* stereo */
		  case 'd': DMAbuffersize = atoi(&argv[i][j+1]); break;
		  case 'T': terminate = 1; break;
		  case 'f': frequency = atoi(&argv[i][j+1]); break; /* frequency */
		  case 't': transpose = atoi(&argv[i][j+1]); break;
		  case 'r': pref.repeats = atoi(&argv[i][j+1]); break;
		  case 'O': oversample = atoi(&argv[i][j+1]); break;
		  case 'S': pref.speed = atoi(&argv[i][j+1]); break;
		  case 'B': set_mix(atoi(&argv[i][j+1])); break;
		  case 'L': priority = 0; break;
		  case 'M': priority = PRTYC_FOREGROUNDSERVER; break;
		  case 'H': priority = PRTYC_TIMECRITICAL; break;
		  default: error_flag++; break;
	       }
	    }
	 }
      }
}

void main(int argc, char *argv[])
{
   static char szClientClass[] = "form.child";
   QMSG qmsg;
   REAL_SWCNTRL SwitchData;
   HPOINTER hptr;
   HACCEL haccel;
  
   if (logerrors)
   {
      debug = fopen("debug", "w");
      fprintf(debug, "error log file opened\n");
   }
   getpname(argv);
   pref.stereo = DEFAULT_CHANNELS - 1;
   pref.type = BOTH;
   pref.repeats = 1;
   pref.speed = 50;
   pref.tolerate = 2;
   pref.verbose = 0;
   pref.stereo = 1;
   frequency = 20000;
   oversample = 1;
   transpose = 0;

   set_mix (DEFAULT_MIX);        /* 0 = full stereo, 100 = mono */
   parsecommandline(argc, argv);

   if (numsongs) gotodir(msong[numsongs-1]);
   error_flag = 0;
   frequency = open_audio(frequency, DMAbuffersize);
   init_player(oversample, frequency);

   hab = WinInitialize (0) ;
   hmq = WinCreateMsgQueue (hab, 0) ;

   WinRegisterClass (hab, szClientClass, ControlDlgProc,
			     CS_SIZEREDRAW, 0);

   hwndControl = WinLoadDlg(HWND_DESKTOP, NULL, ControlDlgProc, NULL, IDM_CONTROL, NULL);

   SwitchData.hwnd = hwndControl;
   SwitchData.hwndIcon = NULL;
   SwitchData.hprog = NULL;
   SwitchData.idProcess = 0;
   SwitchData.idSession = 0;
   SwitchData.uchVisibility = SWL_VISIBLE;
   SwitchData.fbJump = SWL_JUMPABLE;
   SwitchData.bProgType = PROG_PM;

   strcpy(SwitchData.szSwtitle,title);

   hSwitch = WinCreateSwitchEntry(hab, (PSWCNTRL)&SwitchData);

   hptr = WinLoadPointer(HWND_DESKTOP, NULL, ID_TRACKER);
   WinPostMsg(hwndControl, WM_SETICON, hptr, NULL);
   haccel = WinLoadAccelTable(hab, NULL, ID_TRACKER);
   WinSetAccelTable(hab, haccel, hwndControl);

   PrintSong(NULL);

   while (WinGetMsg (hab, &qmsg, NULL, 0, 0))
       WinDispatchMsg (hab, &qmsg);

   quit(0);
}
