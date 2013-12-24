/* OS/2 2.0 32bit timer functions */

#define INCL_DOSMISC

#include <os2.h>

void StartElapsedTimer(ULONG *timer);
ULONG DetermineElapsedTime(ULONG timer);

