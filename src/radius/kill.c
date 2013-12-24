/* kill.c */

#define INCL_DOSEXCEPTIONS
#define INCL_KBD
#define INCL_NOPM          /* No Presentation manager files */
#define INCL_BASE
#include <os2.h>
#include <errno.h>
#include <signal.h>
#include <process.h>

int kill (int pid, int sig)
{
  ULONG rc;
  ULONG n;

  if (pid == getpid ())
    {
      if (sig == 0)
        return 0;
      return raise (sig);
    }
  else if (sig == SIGINT || sig == SIGBREAK)
    {
      if (sig == SIGINT)
        n = XCPT_SIGNAL_INTR;
      else
        n = XCPT_SIGNAL_BREAK;
      rc = DosSendSignalException (pid, n);
      if (rc != 0)
        {
          errno = rc; /*  _sys_set_errno (rc);  OS/2 equiv? ***************** */
          return -1;
        }
      return 0;
    }
  errno = EINVAL;
  return -1;
}
