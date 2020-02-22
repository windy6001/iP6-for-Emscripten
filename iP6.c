/** iP6: PC-6000/6600 series emualtor ************************/
/**                                                         **/
/**                           iP6.c                         **/
/**                                                         **/
/** by ISHIOKA Hiroshi 1998,1999                            **/
/** This code is based on fMSX written by Marat Fayzullin   **/
/*************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include "P6.h"
#include "Help.h"
#include "Xconf.h"

char *Options[]=
{ 
  "verbose","help",
  "shm","noshm","trap","sound","nosound","patch","scale",NULL
};

extern char *Title;
extern int   UseSHM;
extern int   UseSound;
extern int   scale;
extern int   IntLac;

int main(int argc,char *argv[])
{
  int N,J;
  TrapBadOps=0;
  Verbose=1;P6Version=0;PatchLevel=1;

#ifdef MITSHM
  UseSHM=1;
#endif
#ifdef SOUND
  UseSound=0;
#endif

 // Xt_init(&argc,&argv);

  for(N=1;N<argc;N++)
    {    
      for(J=0;Options[J];J++)
        if(!strcmp(argv[N]+1,Options[J])) break;
      switch(J)
      {
        case 0:  N++; /* verbose */
                 if(N<argc) Verbose=atoi(argv[N]);
                 else printf("%s: No verbose level supplied\n",argv[0]);
                 TrapBadOps=Verbose&0x10;
                 break;
	case 1:  /* help */
                 printf("%s by ISHIOKA Hiroshi    (C) 1998,1999\n",Title);
                 for(J=0;HelpText[J];J++) puts(HelpText[J]);
                 return(0);
#ifdef MITSHM
        case 2:  UseSHM=1;break; /* shm */
        case 3:  UseSHM=0;break; /* noshm */
#endif
#ifdef DEBUG
        case 4:  N++; /* trap */
                 if(N>=argc)
                   printf("%s: No trap address supplied\n",argv[0]);
                 else
                   if(!strcmp(argv[N],"now")) Trace=1;
                   else sscanf(argv[N],"%hX",&Trap);
                 break;
#endif
#ifdef SOUND
        case 5:  UseSound=1;break; /* sound */
        case 6:  UseSound=0;break; /* nosound */
#endif
        case 7:  N++; /* patch */
                 if(N<argc) PatchLevel=atoi(argv[N]);
                 else printf("%s: No patch level supplied\n",argv[0]);
		 if(PatchLevel<1) PatchLevel=0;
		 if(PatchLevel>1) PatchLevel=1;
                 break;
        case 8:  N++; /* scale */
	         if(N<argc) scale=atoi(argv[N]);
		 if(scale<1) PatchLevel=1;
		 if(scale>2) PatchLevel=2;
                 break;
        default: printf("%s: Wrong option '%s'\n",argv[0],argv[N]);
      }
    }

  if(!InitMachine()) return(1);
  StartP6();TrashP6();
  TrashMachine();
  return(0);
}
