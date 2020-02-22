/* Unix.h */
/* Common definitons and declarations for Unix.c and Refresh.c */

#define Bool int
typedef unsigned long XID;



/* size of ximage */
#define M1WIDTH   256
#define M1HEIGHT  192
#define M5WIDTH   320
#define M5HEIGHT  200

#define WIDTH     M5WIDTH*2
#define HEIGHT    M5HEIGHT*2
#define SCRWIDTH  WIDTH+20
#define SCRHEIGHT HEIGHT+20

/** Some typedef's **/
typedef union {
    XID ct_xid;
    byte ct_byte[4]; 
} ColTyp; /* sizeof ColTyp should be 4 */

/* functions and variables in Unix.c used by Refresh.c */
extern int Mapped;
extern byte *XBuf;
extern int scale;
extern int bitpix;
extern ColTyp BPal[16],BPal53[32],
  BPal11[4],BPal12[8],BPal13[8],BPal14[4],BPal15[8];

/* functions and variables in Refresh.c used by Unix.c */
extern void choosefuncs(int lsbfirst, int bitpix);
extern void setwidth(int wide);
extern Bool lsbfirst;

extern XID black;
extern void PutImage(void);
