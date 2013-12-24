/* Get password function - - OS/2 specific */

#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>
#include <time.h>

#define INCL_KBD
#define INCL_NOPM          /* No Presentation manager files */
#define INCL_BASE
#include <os2.h>

#include <sys\stat.h>
#include <crypt.h>

#define LF 10
#define CR 13
#define STDOUT 1

#define MAX_STRING_LEN 256

char *tn;



/* ---------------------------------------------------- */
void readln(char *st,int maxnum, int echo)
{
char ch;
int pos;
KBDKEYINFO kbci;

  pos=0;
  do {
    KbdCharIn(&kbci, IO_WAIT, 0);
    ch=kbci.chChar;
    if ((ch < 128) && (ch >= 8)) {
      switch (ch) {
        case 8 : if (pos>0) {
                  pos--;
                  printf("%c %c",ch,ch);
                  break;
                }
        case 13 : st [pos]=0; break;
        case 3  :
        case 27 : st[0]=0; exit(1);
        default : if (pos < maxnum) {
                    st[pos++]=ch;
                    if (echo)
                      putchar(ch);
                    else
                      putchar('*');
                  }
      }
    }
  } while (ch != 13);
  printf("\n");
}


char * getpass(const char * prompt) {
  static char pw[40];
  printf("%s", prompt);
  readln(pw, sizeof(pw)-1, 0);
  return (pw);
}


