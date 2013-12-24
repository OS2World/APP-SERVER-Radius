/* Rexx file callable from MRTG to extract RUNSTATS output file data */

  logfile="c:\runstats\runstats.log"

  total=0

  fileSize=CHARS(logfile)
  if fileSize > 0 THEN DO
   error=STREAM(logfile, 'C', 'seek ='fileSize-100)
   DO WHILE LINES(logfile) > 0
     line=linein(logfile)

     parse VAR line '[' yy '/' mm '/' dd ':' hh ':' min ':' ss ']' line
     if (line <> "") then do
        line=STRIP(line,"B")
        total=0
        DO WHILE (line <> "")
          parse VAR line serverCount line
          if serverCount <> "" then do
             total=total+serverCount
          end
        END
     end

   END
   /* Here 'total' contains the number of users on the last line */
   error=STREAM(logfile, 'C', 'CLOSE')
  end

say total
say total
/* Make up an uptime rather than calculating this out... */
say time(s)+(date(d)*3600*24)
Say "Dialup server"
