/* soundblaster_audio.c */

/* modified by David Nichols for this PM MOD player */

/* MODIFIED BY Michael Fulbright (MSF) to work with os/2 device driver */

/* $Author: steve $
 * $Id: soundblaster_audio.c,v 1.1 1992/06/24 06:24:17 steve Exp steve $
 * $Revision: 1.1 $
 * $Log: soundblaster_audio.c,v $
 * Revision 1.1  1992/06/24  06:24:17  steve
 * Initial revision
 */

#define INCL_DOS

#include <os2.h>
#include <stdio.h>
#include <stdlib.h>
#include "sblast_user.h"
#include "defs.h"

struct sb_mixer_levels sbLevels;
struct sb_mixer_params sbParams;

unsigned char *buffer;		/* buffer for ready-to-play samples */

static int buf_index;

HFILE hAudio;      /* audio handle */

int fixparams = 0;
int filterout;
int filterin;
int filterhi;

/* 256th of primary/secondary source for that side. */
static int primary, secondary;

void restoreparams()
{
   ULONG parlen, datlen;

   if (fixparams)
   {
      parlen = 0;
      datlen = sizeof(struct sb_mixer_params);
      DosDevIOCtl(hAudio, DSP_CAT, MIXER_IOCTL_READ_PARAMS,
             NULL, 0, &parlen, &sbParams, datlen, &datlen);
      sbParams.hifreq_filter = filterhi;
      sbParams.filter_output = filterout;
      sbParams.filter_input = filterin;
      parlen = 0;
      datlen = sizeof(struct sb_mixer_params);
      DosDevIOCtl(hAudio, DSP_CAT, MIXER_IOCTL_SET_PARAMS,
              NULL, 0, &parlen, &sbParams, datlen, &datlen);
   }
}

void set_mix (int percent)
{
  percent *= 256;
  percent /= 100;
  primary = percent;
  secondary = 512 - percent;
}

int open_audio(int frequency, unsigned short DMAbuffersize)
{
  USHORT status, freq;
  USHORT   flag;
  ULONG  datlen, parlen, action, temp;
 
  /* MSF - open SBDSP for output */
  status = DosOpen( "SBDSP$", &hAudio, &action, 0, FILE_NORMAL, FILE_OPEN,
   OPEN_ACCESS_READWRITE | OPEN_SHARE_DENYREADWRITE |
   OPEN_FLAGS_WRITE_THROUGH | OPEN_FLAGS_NOINHERIT |
   OPEN_FLAGS_NO_CACHE | OPEN_FLAGS_FAIL_ON_ERROR, NULL );

  if (status != 0) trackerror(1, "Error opening audio device SBDSP$", FATAL_ERROR);

  /* see if we are on a SBREG or SBPRO */
  status = DosOpen( "SBMIX$", &temp, &action, 0, FILE_NORMAL, FILE_OPEN,
   OPEN_ACCESS_READWRITE | OPEN_SHARE_DENYNONE |
   OPEN_FLAGS_WRITE_THROUGH | OPEN_FLAGS_NOINHERIT | 
   OPEN_FLAGS_NO_CACHE, NULL );

  if (status !=0) pref.stereo=FALSE;
  else
   {
      fixparams = TRUE;
      parlen = 0;
      datlen = sizeof(struct sb_mixer_params);
      DosDevIOCtl(hAudio, DSP_CAT, MIXER_IOCTL_READ_PARAMS,
             NULL, 0, &parlen, &sbParams, datlen, &datlen);
      filterhi = sbParams.hifreq_filter;
      filterout = sbParams.filter_output;
      filterin = sbParams.filter_input;
      sbParams.hifreq_filter = TRUE;
      sbParams.filter_output = FALSE;
      sbParams.filter_input = TRUE;
      parlen = 0;
      datlen = sizeof(struct sb_mixer_params);
      DosDevIOCtl(hAudio, DSP_CAT, MIXER_IOCTL_SET_PARAMS,
               NULL, 0, &parlen, &sbParams, datlen, &datlen);
      datlen=1;
      parlen=0;
      flag=pref.stereo;
      status=DosDevIOCtl(hAudio, DSP_CAT, DSP_IOCTL_STEREO,
			 NULL, 0, &parlen, &flag, 1, &datlen);
      if (status != 0)	trackerror (1, "Error setting stereo/mono", FATAL_ERROR);
      datlen = 1;
      flag = DMAbuffersize * 1024;
      DMAbuffersize = flag;
      status=DosDevIOCtl(hAudio, DSP_CAT, DSP_IOCTL_BUFSIZE,
                    NULL, 0, &parlen, &DMAbuffersize, datlen, &datlen);
      if (status != 0)trackerror(1, "Error setting DMA buffer size", FATAL_ERROR);
   }

  if (pref.stereo) frequency *= 2;  /* XXX Stereo takes twice the speed */

  if (frequency == 0) frequency = -1;  /* read current frequency from driver */

  /* set speed */
  datlen=2;
  parlen=0;
  freq = (USHORT) frequency;
  status=DosDevIOCtl(hAudio, DSP_CAT, DSP_IOCTL_SPEED,
		     NULL, 0, &parlen, &freq, 2, &datlen);
  frequency=freq;
  if (status!=0) trackerror(1, "Error setting frequency", FATAL_ERROR);

  buffer = malloc (sizeof (SAMPLE) * frequency);	/* Stereo makes x2 */
  buf_index = 0;

  if (pref.stereo) return (frequency / 2);
  else return (frequency);
}

void output_samples (int left, int right)
{
  if (pref.stereo)
    {
      buffer[buf_index++] = (((left * primary + right * secondary) / 256) + (1 << 15)) >> 8;
      buffer[buf_index++] = (((right * primary + left * secondary) / 256) + (1 << 15)) >> 8;
    }
  else buffer[buf_index++] = (left + right + (1 << 15)) >> 8;
}

void flush_buffer (void)
{
  ULONG numread, status;

  status = DosWrite(hAudio, buffer, buf_index, &numread);
   if (status != 0)
   {
      char buf[80];

      sprintf(buf, "Error writing to audio device: %d, tried to write: %d, wrote: %d", status, buf_index, numread);
      trackerror (1, buf, FATAL_ERROR);
   }
  if (numread != buf_index)
   {
      char buf[80];

      sprintf(buf, "DosWrite mismatch, buf_index: %d, numread: %d", buf_index, numread);
      trackerror(1, buf, NONFATAL_ERROR);
   }      
  buf_index = 0;
}

void flush_DMA_buffers()
{
  ULONG status, datlen, parlen;

  /* now tell device driver to flush out internal buffers */
  parlen=0;
  datlen=0;
  status=DosDevIOCtl(hAudio, DSP_CAT, DSP_IOCTL_FLUSH,
                    NULL, 0, &parlen, NULL, 0, &datlen);
   if (status != 0)
   {
      char buf[80];

      sprintf(buf, "Error flushing DMA buffers: %d", status);
      trackerror(1, buf, NONFATAL_ERROR);
   }
}

void close_audio (void)
{
   DosClose(hAudio);
}
