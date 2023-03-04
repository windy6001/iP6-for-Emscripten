/*
** Sound.c
**
** This file countains PSG/SCC emulation stuff for fMSX via the /dev/dsp
** device present in Linux and FreeBSD (and probably in other PC Unices
** using USS(/Lite) (formerly known as VoxWare) sound driver). Also very
** experimental support for Sun's /dev/audio is present.
**
** (C) Ville Hallik (ville@physic.ut.ee) 1996
**
** You can do almost anything you want with this file, provided that notice
** about original author still appears there and in case you make any
** modifications, PLEASE label it as modified version.
**
** Known problems:
** 1. The noise and envelope frequencies may be wrong - I dont have
**    real MSX handy to compare them.
** 2. The seventh bit of port 0xAA is not supported (it's almost impossible
**    to do with the method I use here), fortunately it's not used in most
**    cases.
** 3. Modifications of PSG registers are checked (theoretically ;) once
**    per every SOUND_BUFSIZE/SOUND_RATE seconds. It's about 11ms with the
**    values of 256bytes and 22050Hz, which should be satisfactory for most
**    cases.
** 4. Support for Sun's /dev/audio is crap, but it works somehow ;)
** 5. I don't know how realistic is the emulation of SCC chip.
**
** last modified on Sat Apr 22 1996
*/

#define SOUND // 後で消す
#ifdef SOUND

#define FREQ 2052654284
#include "P6.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "SDL.h"

SDL_AudioSpec want, have;
SDL_AudioDeviceID dev;

extern byte PSG[16];



#ifdef SUN_AUDIO

#ifdef PAGESIZE
#undef PAGESIZE
#endif PAGESIZE

#include <sys/audioio.h>
#include <sys/conf.h>
#include <stropts.h>
#include <signal.h>



static unsigned char dsp_ulaw[256] = {
    31,   31,   31,   32,   32,   32,   32,   33, 
    33,   33,   33,   34,   34,   34,   34,   35, 
    35,   35,   35,   36,   36,   36,   36,   37, 
    37,   37,   37,   38,   38,   38,   38,   39, 
    39,   39,   39,   40,   40,   40,   40,   41, 
    41,   41,   41,   42,   42,   42,   42,   43, 
    43,   43,   43,   44,   44,   44,   44,   45, 
    45,   45,   45,   46,   46,   46,   46,   47, 
    47,   47,   47,   48,   48,   49,   49,   50, 
    50,   51,   51,   52,   52,   53,   53,   54, 
    54,   55,   55,   56,   56,   57,   57,   58, 
    58,   59,   59,   60,   60,   61,   61,   62, 
    62,   63,   63,   64,   65,   66,   67,   68, 
    69,   70,   71,   72,   73,   74,   75,   76, 
    77,   78,   79,   81,   83,   85,   87,   89, 
    91,   93,   95,   99,  103,  107,  111,  119, 
   255,  247,  239,  235,  231,  227,  223,  221, 
   219,  217,  215,  213,  211,  209,  207,  206, 
   205,  204,  203,  202,  201,  200,  199,  198, 
   197,  196,  195,  194,  193,  192,  191,  191, 
   190,  190,  189,  189,  188,  188,  187,  187, 
   186,  186,  185,  185,  184,  184,  183,  183, 
   182,  182,  181,  181,  180,  180,  179,  179, 
   178,  178,  177,  177,  176,  176,  175,  175, 
   175,  175,  174,  174,  174,  174,  173,  173, 
   173,  173,  172,  172,  172,  172,  171,  171, 
   171,  171,  170,  170,  170,  170,  169,  169, 
   169,  169,  168,  168,  168,  168,  167,  167, 
   167,  167,  166,  166,  166,  166,  165,  165, 
   165,  165,  164,  164,  164,  164,  163,  163, 
   163,  163,  162,  162,  162,  162,  161,  161, 
   161,  161,  160,  160,  160,  160,  159,  159, 
};
                                                                         
#define AUDIO_CONV(A) (dsp_ulaw[0xFF&(128+(A))])

/*
** Size of audio buffer
*/
#define SOUND_BUFSIZE 256

/* Do not change this! It's not implemented. */
#define SOUND_RATE 8000

#else /* SUN_AUDIO */

#ifdef LINUX
//#include <linux/soundcard.h>
#else
//#include <soundcard.h>
#endif

#define AUDIO_CONV(A) (128+(A))

/*
** Output rate (Hz)
*/
#ifndef SOUND_RATE
#define SOUND_RATE 22050
#endif // SOUND_RATE

/*
** Buffer size. SOUND_BUFSIZE should be equal to (or at least less than)
** 2 ^ SOUND_BUFSIZE_BITS
*/
#define SOUND_BUFSIZE 256
#define SOUND_BUFSIZE_BITS 8

/*
** Number of audio buffers to use. Must be at least 2, but bigger value
** results in better behaviour on loaded systems (but output gets more
** delayed)
*/
#define SOUND_NUM_OF_BUFS 8

#endif // SUN_AUDIO

pid_t soundchildpid=0;
int soundpipe[2];
int sounddev=-1;
int sound_rate=SOUND_RATE;
int UseSound=1;
int UseSCC=1;

unsigned char envforms[16][32] = {
	{ 15, 14, 13, 12, 11, 10,  9,  8,  7,  6,  5,  4,  3,  2,  1,  0,
	   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0 },
	{ 15, 14, 13, 12, 11, 10,  9,  8,  7,  6,  5,  4,  3,  2,  1,  0,
	   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0 },
	{ 15, 14, 13, 12, 11, 10,  9,  8,  7,  6,  5,  4,  3,  2,  1,  0,
	   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0 },
	{ 15, 14, 13, 12, 11, 10,  9,  8,  7,  6,  5,  4,  3,  2,  1,  0,
	   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0 },
	{  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
	   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0 },
	{  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
	   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0 },
	{  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
	   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0 },
	{  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
	   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0 },
	{ 15, 14, 13, 12, 11, 10,  9,  8,  7,  6,  5,  4,  3,  2,  1,  0,
	  15, 14, 13, 12, 11, 10,  9,  8,  7,  6,  5,  4,  3,  2,  1,  0 },
	{ 15, 14, 13, 12, 11, 10,  9,  8,  7,  6,  5,  4,  3,  2,  1,  0,
	   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0 },
	{ 15, 14, 13, 12, 11, 10,  9,  8,  7,  6,  5,  4,  3,  2,  1,  0,
	   0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15 },
	{ 15, 14, 13, 12, 11, 10,  9,  8,  7,  6,  5,  4,  3,  2,  1,  0,
	  15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15 },
	{  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
	   0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15 },
	{  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
	  15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15 },
	{  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
	  15, 14, 13, 12, 11, 10,  9,  8,  7,  6,  5,  4,  3,  2,  1,  0 },
	{  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
	   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0 }
};

int MakeWave( char *soundbuf, int len);


void MyAudioCallback(void* userdata, Uint8* stream, int len) 
{
  char tmp[SOUND_BUFSIZE];
  MakeWave(tmp ,len);
  memcpy( stream , tmp , len);
  printf("len=%d bufsize=%d \n",len , SOUND_BUFSIZE);
}


void SoundMainLoop(void);

void TrashSound(void)
{
  int J;
//  if(soundchildpid) { kill(soundchildpid,SIGKILL);wait(&J); }
//  if(sounddev!=-1) close(sounddev);
  SDL_CloseAudioDevice(1);
}

int OpenSoundDev(void)
{
  int I,J,K;
  
  SDL_memset(&want, 0, sizeof(want)); /* または SDL_zero(want); */
  want.freq = SOUND_RATE;
  want.format = AUDIO_U8;
  want.channels = 1;
  want.samples = SOUND_BUFSIZE;
  want.callback = MyAudioCallback;  /* この関数はどこか別の場所に書く. 詳細はSDL_AudioSpecを参照すること */

  printf("Audio open...");
  dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, SDL_AUDIO_ALLOW_ANY_CHANGE);
  if (dev == 0) {
      printf("FAILED: %s\n", SDL_GetError());
      return 0;
  }  /* else {
      if (have.format != want.format) { // 変わった？ 
          printf("FAILED (Not support audio format) %X\n ",have.format);
          return 0;
      }
  } */
  SDL_PauseAudioDevice(dev, 0); /* 再生を開始する */


#if 0
#ifdef SUN_AUDIO

  if(Verbose) printf("  Opening /dev/audio...");
  if((sounddev=open("/dev/audio",O_WRONLY | O_NONBLOCK))==-1)
  { if(Verbose) puts("FAILED");return(0); }

  /*
  ** Sun's specific initialization should be here...
  ** We assume, that it's set to 8000kHz u-law mono right now.
  */

#else

  /*** At first, we need to open /dev/dsp: ***/
  if(Verbose) printf("  Opening /dev/dsp...");
  if((sounddev=open("/dev/dsp",O_WRONLY | O_NONBLOCK))==-1)
  { if(Verbose) puts("FAILED");return(0); }

  if(Verbose) printf("OK\n  Setting mode: 8bit...");

  J=AFMT_U8;I=(ioctl(sounddev,SNDCTL_DSP_SETFMT,&J)<0);
  if(!I)
  {
    if(Verbose) printf("mono...");
    J=0;I|=(ioctl(sounddev,SNDCTL_DSP_STEREO,&J)<0);
  }
  if(I) { if(Verbose) puts("FAILED");return(0); }
  /*** Set sampling rate ***/
  if(!I)
  {
    if(Verbose) printf("OK\n  Setting sampling rate: %dHz...",sound_rate);
    I|=(ioctl(sounddev,SNDCTL_DSP_SPEED,&sound_rate)<0);
    if(Verbose) printf("(got %dHz)...",sound_rate);
  }
  if(I) { if(Verbose) puts("FAILED");return(0); }
  /*** Here we set the number of buffers to use **/
  if(!I)
  {
    if(Verbose) printf("OK\n  Adjusting buffers: %d buffers %d bytes each...",
     SOUND_NUM_OF_BUFS, 1<<SOUND_BUFSIZE_BITS);
    J=K=SOUND_BUFSIZE_BITS|(SOUND_NUM_OF_BUFS<<16);
    I|=(ioctl(sounddev,SNDCTL_DSP_SETFRAGMENT,&J)<0);
    if( J != K ) {
      if(Verbose) printf("(got %d buffers %d bytes each)...",
       J>>16,1<<(J&0xFFFF));
      I=-1;
    }
  }
  if(I) { if(Verbose) puts("FAILED");return(0); }

#endif // SUN_AUDIO
#endif

  if(Verbose) puts("OK");
  return(1);
}

int InitSound(void )
{

  if(Verbose) puts("Starting sound server:");

  UseSound=0;sounddev=-1;soundchildpid=0;

  if( !OpenSoundDev() ) return(0);

/*
  if(Verbose) printf("  Opening pipe...");
  if(pipe(soundpipe)==-1) { if(Verbose) puts("FAILED");return(0); }
  if(Verbose) { printf("OK\n  Forking..."); fflush( stdout ); }

  switch(soundchildpid=fork())
  {
    case -1: if(Verbose) puts("FAILED");
             return(0);
    case 0:  close(soundpipe[1]);
             if( !(Verbose&32) ) Verbose=0;
             SoundMainLoop();
             break;
    default: if(Verbose) puts("OK");
             close(soundpipe[0]);
             fcntl( soundpipe[1], F_SETFL, O_NONBLOCK );
             close(sounddev);
  }
*/
  UseSound=1;return(1);
}



void SoundOut(byte R,byte V)
{
  if( R != 0xff) {
    PSG[R] = V;
  }
#if 0
  static unsigned char Buf[1024];
  static int Buffered = 0;

  if(UseSound) {
    if((R==0xFF)||(Buffered>=sizeof(Buf))) {
      write(soundpipe[1],Buf,Buffered);
      Buffered = 0;
    }
    if(R!=0xFF) {
      Buf[Buffered++]=R; Buf[Buffered++]=V;
    }
  }
#endif
}

void FlushSound(void)      { SoundOut(0xFF,0); }
void StopSound(void)       { if(UseSound) kill(soundchildpid,SIGUSR1); }
void ResumeSound(void)     { if(UseSound) kill(soundchildpid,SIGUSR2); }
void PSGOut(byte R,byte V) { if(R<16||R==0xFF) SoundOut(R,V); }
void SCCOut(byte R,byte V) { if(UseSCC&&R<0x90) SoundOut(R+0x10,V); }

void SoundSignal( int sig )
{
#if 0
  static ok_to_continue=0;
  
  switch( sig ) {
  case SIGUSR1:
    /* Suspend execution, until SIGUSR2 catched */
#ifndef SUN_AUDIO
    ioctl( sounddev, SNDCTL_DSP_RESET );
#endif /* not SUN_AUDIO */
    close( sounddev );
    ok_to_continue=0;
    while( !ok_to_continue ) pause();
    if( Verbose ) puts( "Soundserver: Re-opening sound device:" );
    OpenSoundDev();
    if( Verbose ) fflush( stdout );
    signal( SIGUSR1, SoundSignal );
    break;
  case SIGUSR2:
    ok_to_continue=1;
    signal( SIGUSR2, SoundSignal );
    break;
  }
#endif
}

//void SoundMainLoop( void )
//{
int MakeWave( char *soundbuf, int len)
{
  int incr0=0, incr1=0, incr2=0, increnv=0, incrnoise=0;
  int statenoise=0, noisegen=1;
  int counter0, counter1, counter2, countenv, countnoise;
  int r=0, v=0, i, j=0, rc, x;
  int vol0, vol1, vol2, volnoise, envelope = 15;
  int c0, c1, l0, l1, l2;
  unsigned char buf[2];
  //unsigned char soundbuf[SOUND_BUFSIZE];
//  unsigned char PSG[16];
  pid_t parent;
  /* Variables for SCC emulation: */
  int SCCactive=0, SCCoutv=0;
  int SCCinc1=0, SCCinc2=0, SCCinc3=0, SCCinc4=0, SCCinc5=0;
  int SCCcnt1=0, SCCcnt2=0, SCCcnt3=0, SCCcnt4=0, SCCcnt5=0;
  int SCCvol1, SCCvol2, SCCvol3, SCCvol4, SCCvol5;
  unsigned char SCC[256];

  printf("making wave...\n");
#if 0	
  parent = getppid();
  signal( SIGUSR1, SoundSignal );
  signal( SIGUSR2, SoundSignal );
	
  for( i=0; i<16; ++i ) PSG[i] = 0;   // PSG 初期化すべき？
  PSG[7] = 077;
  PSG[8] = 8;
  PSG[9] = 8;
  PSG[10] = 8;
  for( i=0; i<256; ++i ) SCC[i] = 0;

  fcntl( soundpipe[0], F_SETFL, O_NONBLOCK );
	
  for( ;; ) {
    /* check for parent's existence */
    if( parent != getppid() ) {
      fprintf( stderr, "Parent died\n" );
      exit( 1 );
    }
    /* Read from pipe: */
    for( ;; ) {
      rc = read( soundpipe[0], buf, v < 0 ? 1 : 2 );
      if( rc == 0 ) break;
      if( rc < 0 ) {
        if( errno == EWOULDBLOCK ) {
          break;
        } else {
          perror( "read" );
          exit( 1 );
        }
      }
      if( v == -1 ) {
        v = buf[0];
      } else {
        r = buf[0];
        if( rc == 1 ) {
          v = -1;
          break;
        }
        v = buf[1];
      }
			
      if( r<16 ) {
      
        PSG[r] = v;
#endif
    for(r=0; r<16; r++) {
        switch( r ) {
        case 0:
        case 1:
          x = (PSG[0]+((unsigned)(PSG[1]&0xF)<<8));
          incr0 = x ? FREQ / sound_rate * 4 / x : 0;
          break;
        case 2:
        case 3:
          x = (PSG[2]+((unsigned)(PSG[3]&0xF)<<8));
          incr1 = x ? FREQ / sound_rate * 4 / x : 0;
          break;
        case 4:
        case 5:
          x = (PSG[4]+((unsigned)(PSG[5]&0xF)<<8));
          incr2 = x ? FREQ / sound_rate * 4 / x : 0;
          break;
        case 6:
          x = PSG[6]&0x1F;
          incrnoise = FREQ / sound_rate * 4 / ( x ? x : 1 );
          break;
        case 8:
          PSG[8] &= 0x1F;
          break;
        case 9:
          PSG[9] &= 0x1F;
          break;
        case 10:
          PSG[10] &= 0x1F;
          break;
        case 11:
        case 12:
        case 13:
          PSG[13] &= 0xF;
          x = (PSG[11]+((unsigned)PSG[12]<<8));
          increnv = x ? FREQ / sound_rate * 4 / x * SOUND_BUFSIZE : 0;
          countenv=0;
          break;
        }
    }
#if 0
      } else if( UseSCC ) {

        if( !SCCactive ) {
          if( Verbose ) {
            puts( "Soundserver: SCC emulation activated" );
            fflush( stdout );
          }
          SCCactive=1;
        }
        SCC[r-0x10]=v;

        switch(r-0x10) {
        case 0x80:
        case 0x81:
          x = (SCC[0x80]+((unsigned)(SCC[0x81]&0xF)<<8));
          SCCinc1 = x ? FREQ / sound_rate * 4 / x : 0;
          break;
        case 0x82:
        case 0x83:
          x = (SCC[0x82]+((unsigned)(SCC[0x83]&0xF)<<8));
          SCCinc2 = x ? FREQ / sound_rate * 4 / x : 0;
          break;
        case 0x84:
        case 0x85:
          x = (SCC[0x84]+((unsigned)(SCC[0x85]&0xF)<<8));
          SCCinc3 = x ? FREQ / sound_rate * 4 / x : 0;
          break;
        case 0x86:
        case 0x87:
          x = (SCC[0x86]+((unsigned)(SCC[0x87]&0xF)<<8));
          SCCinc4 = x ? FREQ / sound_rate * 4 / x : 0;
          break;
        case 0x88:
        case 0x89:
          x = (SCC[0x88]+((unsigned)(SCC[0x89]&0xF)<<8));
          SCCinc5 = x ? FREQ / sound_rate * 4 / x : 0;
          break;
        }

      }
      
    }
#endif

    envelope = envforms[PSG[13]][(countenv>>16)&0x1F];
    if( ( countenv += increnv ) & 0xFFE00000 ) {
      switch( PSG[13] ) {
      case 8:
      case 10:
      case 12:
      case 14:
        countenv -= 0x200000;
        break;
      default:
        countenv = 0x100000;
        increnv = 0;
      }
    }

    vol0 = ( PSG[8] < 16 ) ? PSG[8] : envelope;
    vol1 = ( PSG[9] < 16 ) ? PSG[9] : envelope;
    vol2 = ( PSG[10] < 16 ) ? PSG[10] : envelope;

    volnoise = (
     ( ( PSG[7] & 010 ) ? 0 : vol0 ) +
     ( ( PSG[7] & 020 ) ? 0 : vol1 ) +
     ( ( PSG[7] & 040 ) ? 0 : vol2 ) ) / 2;

    vol0 = ( PSG[7] & 001 ) ? 0 : vol0;
    vol1 = ( PSG[7] & 002 ) ? 0 : vol1;
    vol2 = ( PSG[7] & 004 ) ? 0 : vol2;
#if 0		
    if( SCCactive ) {
      SCCvol1 = SCC[0x8F] & 0x01 ? SCC[0x8A] & 0xF : 0;
      SCCvol2 = SCC[0x8F] & 0x02 ? SCC[0x8B] & 0xF : 0;
      SCCvol3 = SCC[0x8F] & 0x04 ? SCC[0x8C] & 0xF : 0;
      SCCvol4 = SCC[0x8F] & 0x08 ? SCC[0x8D] & 0xF : 0;
      SCCvol5 = SCC[0x8F] & 0x10 ? SCC[0x8E] & 0xF : 0;
    }
#endif

    for( i=0; i<SOUND_BUFSIZE; ++i ) {
#if 0
      if( SCCactive ) {

        SCCcnt1 = ( SCCcnt1 + SCCinc1 ) & 0xFFFF;
        c0 = (signed char)SCC[SCCcnt1>>11];
        c1 = (signed char)SCC[((SCCcnt1>>11)+1)&0x1F];
        SCCoutv = SCCvol1*(c0+(c1-c0)*(SCCcnt1&0x7FF)/0x800);

        SCCcnt2 = ( SCCcnt2 + SCCinc2 ) & 0xFFFF;
        c0 = (signed char)SCC[(SCCcnt2>>11)+0x20];
        c1 = (signed char)SCC[(((SCCcnt2>>11)+1)&0x1F)+0x20];
        SCCoutv += SCCvol2*(c0+(c1-c0)*(SCCcnt2&0x7FF)/0x800);

        SCCcnt3 = ( SCCcnt3 + SCCinc3 ) & 0xFFFF;
        c0 = (signed char)SCC[(SCCcnt3>>11)+0x40];
        c1 = (signed char)SCC[(((SCCcnt3>>11)+1)&0x1F)+0x40];
        SCCoutv += SCCvol3*(c0+(c1-c0)*(SCCcnt3&0x7FF)/0x800);

        SCCcnt4 = ( SCCcnt4 + SCCinc4 ) & 0xFFFF;
        c0 = (signed char)SCC[(SCCcnt4>>11)+0x60];
        c1 = (signed char)SCC[(((SCCcnt4>>11)+1)&0x1F)+0x60];
        SCCoutv += SCCvol4*(c0+(c1-c0)*(SCCcnt4&0x7FF)/0x800);

        SCCcnt5 = ( SCCcnt5 + SCCinc5 ) & 0xFFFF;
        c0 = (signed char)SCC[(SCCcnt5>>11)+0x60];
        c1 = (signed char)SCC[(((SCCcnt5>>11)+1)&0x1F)+0x60];
        SCCoutv += SCCvol5*(c0+(c1-c0)*(SCCcnt5&0x7FF)/0x800);

        SCCoutv /= 128;

      }
#endif

      /*
      ** These strange tricks are needed for getting rid
      ** of nasty interferences between sampling frequency
      ** and "rectangular sound" (which is also the output
      ** of real AY-3-8910) we produce.
      */

      c0 = counter0;
      c1 = counter0 + incr0;
      l0 = ((c0&0x8000)?-16:16);
      if( (c0^c1)&0x8000 ) {
        l0=l0*(0x8000-(c0&0x7FFF)-(c1&0x7FFF))/incr0;
      }
      counter0 = c1 & 0xFFFF;
			
      c0 = counter1;
      c1 = counter1 + incr1;
      l1 = ((c0&0x8000)?-16:16);
      if( (c0^c1)&0x8000 ) {
        l1=l1*(0x8000-(c0&0x7FFF)-(c1&0x7FFF))/incr1;
      }
      counter1 = c1 & 0xFFFF;
			
      c0 = counter2;
      c1 = counter2 + incr2;
      l2 = ((c0&0x8000)?-16:16);
      if( (c0^c1)&0x8000 ) {
        l2=l2*(0x8000-(c0&0x7FFF)-(c1&0x7FFF))/incr2;
      }
      counter2 = c1 & 0xFFFF;
			
      countnoise &= 0xFFFF;
      if( ( countnoise += incrnoise ) & 0xFFFF0000 ) {
        /*
        ** The following code is a random bit generator :)
        */
        statenoise =
         ( ( noisegen <<= 1 ) & 0x80000000
         ? noisegen ^= 0x00040001 : noisegen ) & 1;
      }

      soundbuf[i] = AUDIO_CONV ( SCCoutv +
       ( l0 * vol0 + l1 * vol1 + l2 * vol2 ) / 16 +
       ( statenoise ? volnoise : -volnoise ) );

    }

#if 0
    if( sounddev == -1 ) {
      sleep(1);
      continue;
    }
#endif

#ifdef SUN_AUDIO

    /*
    ** Flush output first, don't care about return status.
    ** After this write next buffer of audio data. This method
    ** produces a horrible click on each buffer :( Any ideas,
    ** how to fix this?
    */

    ioctl( sounddev, AUDIO_DRAIN );
    write( sounddev, soundbuf, SOUND_BUFSIZE );

#else

    /*
    ** We'll block here until next DMA buffer becomes free.
    ** It happens once per (1<<SOUND_BUFSIZE_BITS)/sound_rate seconds.
    */
		
    write( sounddev, soundbuf, SOUND_BUFSIZE );

#endif // SUN_AUDIO

  }
//}

#endif // SOUND
