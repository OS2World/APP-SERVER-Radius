/* UserHist.cmd: Create a simple histogram of number of users logged in.
   As written here, it just graphs the total users among all servers.
*/


/* Initialize global variables */
TRUE=1
FALSE=0

/* Load REXXUTIL */
CALL rxfuncadd sysloadfuncs, rexxutil, sysloadfuncs
CALL sysloadfuncs


/* -------------CONFIGURATION SECTION --------------------- */
/* Directory where 'runstats.exe' is executed from */
statsDir="C:\runstats\"

statsFile="con"
/* Flag whether to rotate runstats log each time this program runs */
rotateStatsLogs=FALSE







/* --------------------- BEGIN -------------------------------------- */

  CALL SystemStatistics

  Say "Done"

EXIT 0




/* =================== Provide Histogram of how many on and data rates.========= */
SystemStatistics:

  call LineOut StatsFile, ""
  call LineOut StatsFile, "----------------------------------"
  call LineOut StatsFile, "SYSTEM Statistics"
  call LineOut StatsFile, ""

  statsLogfile=statsDir || "runstats.lo"

  if rotateStatsLogs then do
     "@del "statsLogfile"8 2>nul"
     do i=0 TO 7
       "@ren "statsLogfile||7-i" runstats.lo"8-i" 2>nul"
     end
   
     "@ren "statsLogFile"g runstats.lo1"
   
     statsLogFile=statsLogFile"1"
  end
  else do
     statsLogFile=statsLogFile"g"
  end
     

  /* Clear count bins */
  do bin=0 to 23
     sum.bin=0
     entries.bin=0
     min.bin=99999
     max.bin=0
  end
  DO WHILE LINES(statsLogFile) > 0
     line=LINEIN(statsLogFile)
     parse VAR line '[' yy '/' mm '/' dd ':' hh ':' mm ':' ss ']' line
     if (line <> "") then do
        line=STRIP(line,"B")
        hh=hh+0 /* Strip off leading zero */
        total=0
        DO WHILE (line <> "")
          parse VAR line serverCount line
          if serverCount <> "" then do
             total=total+serverCount
          end
        END
        entries.hh=entries.hh+1
        sum.hh=sum.hh+total
        if total < min.hh then do
           min.hh=total
        end
        if total > max.hh then do
           max.hh=total
        end
     end
  END
  Error=STREAM(statsLogFile,C,'CLOSE')

  /* Determine range (customers per cell) */
  range=0
  do bin=0 to 23
     if max.bin > range then do
        range=max.bin
     end
  end
  NumCells=50
  if range < NumCells then do
     cusPerCell=1
  end
  else do
    cusPerCell=range/NumCells
  end

  call LineOut StatsFile, "Hourly Summary: Customer logins sampled once per 5 minutes"
  call LineOut StatsFile, "-------------------------------"
  call LineOut StatsFile, ""
  call LineOut StatsFile, "Each unit (*) represents "FORMAT(cusPerCell,,1)" customers, (|) represent Min and Max"
  call LineOut StatsFile, ""
  call LineOut StatsFile, "hr: Min. Avg. Max. customers:"
  call LineOut StatsFile, "--  ---------------"
  do bin=0 TO 23
     if entries.bin=0 then do
        average=0
        min.bin=0
     end
     else do
        average=sum.bin/entries.bin
     end
     line=FORMAT(bin,2)":"FORMAT(min.bin,5,0)||FORMAT(average,5,0)||FORMAT(max.bin,5,0)": "
     if max.bin > 0 then do
        minPos=FORMAT(min.bin / cusPerCell,,0)
        avePos=FORMAT(average / cusPerCell,,0)
        maxPos=FORMAT(max.bin / cusPerCell,,0)
        if minPos>0 then do
          line=line||COPIES("*",minPos-1)
          line=line||"|"
        end
        if avePos > minPos then do
          line=line||COPIES("*",(avePos-minPos))
        end
        if maxPos > avePos then do
           line=line||COPIES(" ",maxPos-avePos-1)||"|"
        end
     end
     call LineOut StatsFile, line
  end

  call LineOut StatsFile, "-------------------------------"
  call LineOut StatsFile, ""
  /* Close output file */
  call LineOut StatsFile

  return


