/** iP6: PC-6000/6600 series emualtor ************************/
/**                                                         **/
/**                          Help.h                         **/
/**                                                         **/
/** by ISHIOKA Hiroshi 1998, 1999                           **/
/** This code is based on fMSX written by Marat Fayzullin   **/
/*************************************************************/

char *HelpText[] =
{
  "Usage: iP6 [-option1 [-option2...]]",
  "[-option]   =",
  "  -uperiod <period>   - Number of interrupts per screen update [2]",
  "  -help               - Print this help page",
#ifdef MITSHM
  "  -shm/-noshm         - Use/don't use MIT SHM extensions for X [-shm]",
#endif
#ifdef DEBUG
  "  -trap <address>     - Trap execution when PC reaches address [FFFFh]",
#endif
  "  -60/62/64/66/68     - Select P6 model [-62]",
  "  -rom1 <filename>    - Load external ROM file to &H4000 [off]",
  "  -rom2 <filename>    - Load external ROM file to &H6000 [off]",
  "  -tape <filename>    - Set tape image file [off]",
  "  -printer <filename> - Redirect printer output to file [stdout]",
  "  -scr4col            - Colored N60 screen 4 mode [off]",
#ifdef UNIX
  "  -saver/-nosaver     - Save/don't save CPU when inactive [-saver]",
#endif
#ifdef SOUND
  "  -sound/-nosound     - Play/don't play sound [-nosound]",
#endif
  "  -patch <n>          - set patch level(0-1) [1]",

  "  -drawWait <n>       - screen draw wait [192]",
  "  -clock <n>          - Z80 clock [4MHz]",
  "  -wait/-nowait       - Use/don't use wait [-nowait]",
  "  -scale <n>          - Set window size(1-2) [2]",
  "  -intlac/-nointlac   - Use/don't interlace [-nointlac]",
  NULL
};
