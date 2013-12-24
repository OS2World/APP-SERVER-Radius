/* An OS/2 REXX script to Start and run the RADIUS server. (Restarts after crash) */

call RxFuncAdd 'SysLoadFuncs', 'RexxUtil', 'SysLoadFuncs'
call SysLoadFuncs

 /* PARSE ARG options */
 options="-v -n -p"

TRUE=1
FALSE=0


Quit=FALSE



Do Until Quit
  Say "Calling Radius Daemon"
  "@RADIUSd " options

  Say "07"x "Radius Ended"
  Say "07"x "Will restart in 15 seconds unless you press CTRL+C NOW!!!"
  CALL SysSleep(15)

END


RETURN


