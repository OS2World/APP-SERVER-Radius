/* SETPow.cmd: Set environment for PowerWeb builds */


/* Initialize global variables */
TRUE=1
FALSE=0

/* Load REXXUTIL */
CALL rxfuncadd sysloadfuncs, rexxutil, sysloadfuncs
CALL sysloadfuncs

powerWebRoot=Value('POWERWEB',,'OS2ENVIRONMENT')
if LENGTH(powerWebRoot) > 3 then do
   powerWebRoot = STRIP(powerWebRoot, "T", "\") || "\"
end
IF LENGTH(powerWebRoot) < 2 THEN DO
  powerWebRoot="F:\powerWeb\"
end


/* --------------------- BEGIN -------------------------------------- */

  /* Add Current version of PowerWeb to development path */
  verFile=powerWebRoot || "Version"
  verDir=lineIn(verFile)
  Error=STREAM(verFile, C,'CLOSE')
  verDir=STRIP(verDir, "B")
  newPath=powerWebRoot || verDir || "\include"
  "@set include=" || newPath || ";%include%"
   Say "Added " NewPath

  newPath=powerWebRoot || verDir || "\lib"
  "@set lib=" || newPath || ";%lib%"
   Say "Added " NewPath

  Say "Done"

EXIT 0



/* ------------------------------------------------------------------- */
CreateOutputFile: PROCEDURE EXPOSE OutputFile
  OutputFile=ARG(1)
  result=SysFileDelete(OutputFile)
  Error=STREAM(OutputFile,C,'OPEN WRITE')
  IF Error\="READY:" THEN Bomb("Error " || Error || "creating file ",
          || OutputFile)

  RETURN


/* ------------------------------------------------------------------- */
WriteToFile: PROCEDURE EXPOSE OutputFile
  outputData=ARG(1)
  linesLeft=LINEOUT(OutputFile, outputData)
  IF linesLeft > 0 THEN DO
    Bomb("Error writing to " || OutputFile)
  END
  RETURN


/* ------------------------------------------------------------------- */
CloseOutputFile: PROCEDURE EXPOSE OutputFile
  Error=STREAM(OutputFile,C,'CLOSE')
  IF Error\="READY:" THEN Bomb("Error " || Error || "closing file ",
          || OutputFile)

  RETURN



/* ------------------------------------------------------------------- */
ExistDir: PROCEDURE EXPOSE TRUE FALSE
directory=ARG(1)

  rc = SysFileTree(directory,stemRes,'D',,);
  IF stemRes.0=1 THEN RETURN TRUE
  ELSE RETURN FALSE



/* ------------------------------------------------------------------- */
ExistFile: PROCEDURE EXPOSE TRUE FALSE
filename=ARG(1)

  /* Check that the input file exists */
  IF (STREAM(filename,C,'QUERY EXISTS')="" ) THEN RETURN FALSE
  ELSE RETURN TRUE





/* ------------------------------------------------------------------- */
BOMB: PROCEDURE EXPOSE LogFile TRUE FALSE
  reason=ARG(1)
  Say ""

  Say  "Terminating.." reason
  EXIT 1




/* ------------------------------------------------------------------- */
LowerCase: PROCEDURE

  string=ARG(1)
  lowString=TRANSLATE(string, "abcdefghijklmnopqrstuvwxyz",,
                              "ABCDEFGHIJKLMNOPQRSTUVWXYZ")

  RETURN (lowString)


