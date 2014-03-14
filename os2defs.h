
#define PLAYER_PAUSE 	(char)22
#define PLAYER_RECORD 	(char)23
#define PLAYER_PLAY	(char)24
#define PLAYER_STOP	(char)25
#define PLAYER_REWIND	(char)26
#define PLAYER_FASTFORWARD 	(char)27

typedef struct _REAL_SWCNTRL          /* swctl from C Set/2 */
{
   HWND     hwnd;
   HWND     hwndIcon;
   HPROGRAM hprog;
   PID      idProcess;
   ULONG    idSession;
   ULONG    uchVisibility;
   ULONG    fbJump;
   CHAR     szSwtitle[MAXNAMEL+4];
   ULONG    bProgType;
} REAL_SWCNTRL;

typedef struct
{
   int  fPlaying, fTerminate;
   char *fn;
   struct song *song;
   struct pref *pref;
   HWND hwndControl;
   HEV hevDone;
   HEV hevPause;
   HEV hevTimer;
} THREADPARAMS;

typedef THREADPARAMS *PTHREADPARAMS;

/* real_freq = open_audio(USHORT):
 * try to open audio with a sampling rate of ask_freq.
 * We get the real frequency back. If we ask for 0, we
 * get the ``preferred'' frequency.
 */
int open_audio(int frequency, USHORT DMAbuffersize);

void PrintSong(char *fn);

void PlaySong(ULONG arg);

extern HWND hwndControl; /* handle to control window */
extern HWND hwndSong;
extern HWND hwndTime;
extern HWND hwndName;
extern HWND hwndQueue;
extern HWND hwndChan1, hwndChan2, hwndChan3, hwndChan4;

extern HSWITCH hSwitch;	/* handle to switch info */
extern HAB hab;

extern HPS hpsSong;
extern HPS hpsTime;
extern HPS hpsName;
extern HPS hpsQueue;
extern HPS hpsChan1, hpsChan2, hpsChan3, hpsChan4;

extern RECTL rclTime, rclSong, rclName, rclQueue, 
	rclChan1, rclChan2, rclChan3, rclChan4;

extern int fwidth;
extern int fheight;
extern int suspended;
extern int abortsong;
extern int blink;
extern int pauseack;
extern int numsongs, songnum;
extern USHORT DMAbuffersize;

extern char title[];

extern char playermode[];