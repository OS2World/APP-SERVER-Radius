#define INCL_DOSMISC

#include "timer.h"




ULONG ReadTimer(void)
{
ULONG value;

  DosQuerySysInfo(QSV_MS_COUNT, QSV_MS_COUNT, &value, sizeof(value));
  return value;
}


void StartElapsedTimer(ULONG *timer)
{
  *timer = ReadTimer();
}




ULONG DetermineElapsedTime(ULONG timer)
{
  return (ReadTimer()-timer);
}



