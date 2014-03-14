/* player.c */

/* modified by David Nichols for OS/2 PM MOD player */

/* $Author: espie $
 * $Id: player.c,v 2.10 1991/12/03 23:03:39 espie Exp espie $
 * $Revision: 2.10 $
 * $Log: player.c,v $
 * Revision 2.10  1991/12/03  23:03:39  espie
 * Added transpose feature.
 *
 * Revision 2.9  1991/12/03  21:24:53  espie
 * Reverted to previous behaviour because of intromusic6.b.
 *
 * Revision 2.8  1991/12/03  20:43:46  espie
 * Added possibility to get back to MONO for the sgi.
 *
 * Revision 2.7  1991/12/03  18:07:38  espie
 * Added stereo capabilities to the indigo version.
 *
 * Revision 2.6  1991/12/03  17:12:33  espie
 * Minor changes ??
 *
 * Revision 2.5  1991/11/19  16:07:19  espie
 * Added comments, moved minor stuff around.
 *
 * Revision 2.4  1991/11/18  14:10:30  espie
 * Moved resample part and empty sample test to audio.
 *
 * Revision 2.3  1991/11/18  01:23:30  espie
 * Added two level of fault tolerancy.
 *
 * Revision 2.2  1991/11/18  01:12:31  espie
 * Added some control on the number of replays,
 * and better error recovery.
 *
 * Revision 2.1  1991/11/17  23:07:58  espie
 * Coming from str32.
 *
 */

#define INCL_DOS
#define INCL_GPI
#define INCL_WIN

#include <stdio.h>
#include <os2.h>
#include <string.h>
#include "defs.h"
#include "os2defs.h"
#include "tracker.h"

char songtitle[128];

/* init_channel(ch, dummy):
 * setup channel, with initially
 * a dummy sample ready to play,
 * and no note.
 */
void init_channel (struct channel *ch, struct sample_info *dummy)
{
  ch->samp = dummy;
  ch->mode = DO_NOTHING;
  ch->pointer = 0;
  ch->step = 0;
  ch->volume = 0;
  ch->pitch = 0;
  ch->note = NO_NOTE;

  /* we don't setup arpeggio values. */
  ch->viboffset = 0;
  ch->vibdepth = 0;

  ch->slide = 0;

  ch->pitchgoal = 0;
  ch->pitchrate = 0;

  ch->volumerate = 0;

  ch->vibrate = 0;
  ch->adjust = do_nothing;
}

int VSYNC;			/* base number of sample to output */
void (*eval[NUMBER_EFFECTS]) ();

 /* the effect table */
int oversample;			/* oversample value */
int frequency;			/* output frequency */
int channel;			/* channel loop counter */

char allmodes[] = {PLAYER_PAUSE, PLAYER_RECORD, PLAYER_PLAY, 
	PLAYER_STOP, PLAYER_REWIND, PLAYER_FASTFORWARD, '\0' };


struct channel chan[NUMBER_TRACKS];

 /* every channel */
int countdown;			/* keep playing the tune or not */

struct song_info *info;
struct sample_info **voices;

int count = 0;

int susp = 0;

struct automaton a;

void init_player (int o, int f)
{
  oversample = o;
  frequency = f;
  init_tables (oversample, frequency);
  init_effects (eval);
}

void setup_effect (struct channel *ch, struct automaton *a, struct event *e)
{
  int samp, cmd;

  /* retrieves all the parameters */
  samp = e[a->note_num].sample_number;
  a->note = e[a->note_num].note;
  if (a->note != NO_NOTE)
    a->pitch = pitch_table[a->note];
  else
    a->pitch = e[a->note_num].pitch;
  cmd = e[a->note_num].effect;
  a->para = e[a->note_num].parameters;

  if (a->pitch >= MAX_PITCH)
    {
       trackerror(5, "Pitch out of bounds", NONFATAL_ERROR);
      a->pitch = 0;
      error = FAULT;
    }

  /* load new instrument */
  if (samp)
    {
      /* note that we can change sample in the middle
       * of a note. This is a *feature*, not a bug (see
       * intromusic6.b)
       */
      ch->samp = voices[samp];
      ch->volume = voices[samp]->volume;
    }
  /* check for a new note: cmd 3 (portamento)
   * is the special case where we do not restart
   * the note.
   */
  if (a->pitch && cmd != 3)
    reset_note (ch, a->note, a->pitch);
  ch->adjust = do_nothing;
  /* do effects */
  (eval[cmd]) (a, ch);
}

void UpdateControl(char *mess)
{
   static POINTL pl;

   pl.x = 15;
   pl.y = 10;
   
   DosEnterCritSec();

   /*WinFillRect(hpsTime, &rclTime, CLR_BLACK);*/
   GpiMove(hpsTime, &pl);
   GpiSetColor(hpsTime, 0x00101010);
   GpiCharString(hpsTime, 8, "88 88 88");
   GpiMove(hpsTime, &pl);
   GpiSetColor(hpsTime, 0x0000ff00);
   if (mess) GpiCharString(hpsTime, strlen(mess), mess);

   DosExitCritSec();
}

void PrintSong(char *fn)
{
   static POINTL pl;
   static char buf[10];
   static i;
 
   pl.x = 7;
   pl.y = 7;
   DosEnterCritSec();

   GpiSetColor(hpsSong, RGB_BLUE);
   if (fn != NULL) GpiCharStringAt(hpsSong, &pl, strlen(fn), fn);
   GpiSetColor(hpsName, RGB_BLUE);
   if (fn != NULL) GpiCharStringAt(hpsName, &pl, strlen(songtitle), songtitle);
   pl.y = 10; pl.x = 15;
   sprintf(buf, "%03d:%03d", songnum, numsongs);
   GpiSetColor(hpsQueue, 0x00101010);
   GpiCharStringAt(hpsQueue, &pl, 7, "8888888");
   GpiSetColor(hpsQueue, 0x0000ff00);
   GpiCharStringAt(hpsQueue, &pl, strlen(buf), buf);
   GpiSetColor(hpsTime, 0x0000ff00);
   GpiCharStringAt(hpsTime, &pl, 15, "** ** **   $&**");
   GpiQueryCurrentPosition(hpsTime, &pl);
   GpiSetColor(hpsTime, 0x00101010);
   GpiCharString(hpsTime, strlen(allmodes), allmodes);
   GpiMove(hpsTime, &pl);
   i = playermode[0] - PLAYER_PAUSE;
   strcpy(buf, "\'\'\'\'\'\'\'\'");
   buf[i] = playermode[0];
   buf[i+1] = '\0';
   GpiSetColor(hpsTime, 0x00d00000);
   GpiCharStringAt(hpsTime, &pl, strlen(buf), buf);

   DosExitCritSec();
}

void DoGraphics(ULONG arg)
{
   PTHREADPARAMS ptp;
   ULONG inc;
   int tenths = 0;
   int hour, min, sec = 0;
   int oldsec = 0;
   char buf[30];
   int sc = 0;

   blink = 0;
   ptp = (PTHREADPARAMS)arg;
   PrintSong(ptp->fn);
   while (!ptp->fTerminate)
   {
      DosWaitEventSem(ptp->hevTimer, SEM_INDEFINITE_WAIT);
      DosResetEventSem(ptp->hevTimer, &inc);
      if (suspended || blink) sc += inc;
      else tenths += inc;
      if (blink) tenths += inc;
      oldsec = sec;
      sec = tenths/10;
      min = sec / 60;
      sec = sec - (min * 60);
      hour = min / 60;
      min = min - (hour * 60);
      sprintf(buf, "%02d!%02d\"%02d#", hour, min, sec);
      if (blink && ((sc/2)%3)) strcpy(buf, "**!**\"**#"); 
      if (blink || (oldsec != sec)) UpdateControl(buf);
      else if (suspended && !susp)
      {
	 susp = 1;
         UpdateControl(buf);
      }
   }
   DosPostEventSem(ptp->hevDone);
}

void PlaySong(ULONG arg)
{
   struct song *song;
   struct pref *pref;
   THREADPARAMS tp;
   PTHREADPARAMS ptp;
   RECTL rcl;
   TID tid;
   HTIMER hTimer;
   static REAL_SWCNTRL SwitchData;

   ptp = (PTHREADPARAMS)arg;
   song = ptp->song;
   pref = ptp->pref;
   init_automaton (&a, song);
   strcpy(songtitle, song->title);
   VSYNC = frequency * 100 / pref->speed;
   if (pref->repeats) countdown = pref->repeats; /* if repeat == 0, repeat indefinitely */
   else countdown = 1;

   info = song->info;
   voices = song->samples;

   DosCreateEventSem("\\sem32\\graphicsdone", &tp.hevDone, 0, (BOOL32) FALSE);
   DosCreateEventSem("\\sem32\\graphicstimer", &tp.hevTimer, 0, (BOOL32) FALSE);
   tp.fTerminate = FALSE;
/*   tp.hpsClient = ptp->hpsClient; */
   tp.fn = ptp->fn;
   DosCreateThread(&tid, DoGraphics, (ULONG)&tp, 0, 0x4000);
   DosSetPriority(PRTYS_THREAD,PRTYC_REGULAR,-31,tid);
   DosStartTimer(100, (HSEM)tp.hevTimer, &hTimer);
  
   for (channel = 0; channel < NUMBER_TRACKS; channel++)
     init_channel (chan + channel, voices[0]);

   count = 0;
   while (countdown && !ptp->fTerminate)
   {
      for (channel = 0; channel < NUMBER_TRACKS; channel++)
	if (a.counter == 0) setup_effect (chan + channel, &a, a.pattern->e[channel]);
	else (chan[channel].adjust) (chan + channel); /* do effects */

      /* advance player for the next tick */
      next_tick (&a);
      count++;
      /* actually output samples */
      resample (chan, oversample, VSYNC / a.finespeed);

      switch (error)
      {
	 case NONE:
	    break;

	 case ENDED:
	    if (pref->repeats) countdown--;
	    count = 0;
	    break;

	 case SAMPLE_FAULT:
 	    if (!pref->tolerate) countdown = 0;
	    break;

 	 case FAULT:
	    if (pref->tolerate < 2) countdown = 0;
	    break;

	 case NEXT_SONG:
	 case UNRECOVERABLE:
	    countdown = 0;
	    break;

	 default:
	    break;
      }
      error = NONE;
      if (pauseack)
      {
            flush_DMA_buffers();
	    playermode[0] = PLAYER_PAUSE;
	    PrintSong(NULL);
            pauseack = 0;
            blink = 0;
            suspended = 1;
            DosWaitEventSem(ptp->hevPause, SEM_INDEFINITE_WAIT);
	    susp = 0;
      }
   }
   release_song(song);
   flush_DMA_buffers();
   ptp->fPlaying = FALSE;
   tp.fTerminate = TRUE;
   DosStopTimer(hTimer);
   DosPostEventSem(tp.hevTimer);
   DosWaitEventSem(tp.hevDone, SEM_INDEFINITE_WAIT);
   DosCloseEventSem(tp.hevTimer);
   DosCloseEventSem(tp.hevDone);
   DosEnterCritSec();

   WinQueryWindowRect(hwndSong, &rcl);
   WinFillRect(hpsSong, &rcl, CLR_BLACK);
   WinQueryWindowRect(hwndName, &rcl);
   WinFillRect(hpsName, &rcl, CLR_BLACK);

   DosExitCritSec();
   if (DosPostEventSem(ptp->hevDone)) trackerror(4, "Semaphore error", FATAL_ERROR);
   WinQuerySwitchEntry(hSwitch, (PSWCNTRL) &SwitchData);
   strcpy(SwitchData.szSwtitle, title);
   WinChangeSwitchEntry(hSwitch, (PSWCNTRL) &SwitchData);
   WinSetWindowText(hwndControl, title);
   if (!abortsong) WinPostMsg(hwndControl,WM_COMMAND,MPFROM2SHORT(IDM_PLAY,0),0);
   else
   {
      playermode[0] = PLAYER_STOP;
      PrintSong(NULL);
   }
   abortsong = 0;
   close_audio();
   frequency = open_audio(frequency, DMAbuffersize);
}
