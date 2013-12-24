/* RADUser.cmd: RADIUS 'users' file maintenance
  This is a bare bones example thrown together from several
  sources to demonstrate use of PROTREN.EXE and USERS file editing.
  Be sure to test functions you will be using before calling this
  page from a CGI script!

  USAGE:  raduser    with no arguments starts in interactive mode

   RADUSER loginId,loginpw,sessionLimit,sessionTimeout,fixedIP COMMAND

NOTE: sessionTimeout is SECONDS on the commandline

     executes 'command' on the passed user information.
     Examples:
        RADUSER bclinton,,,, /delete
           would delete user named 'bclinton' from USERS file
        RADUSER mjackson,crxggy,2,,, /add
           Create user record for mjackson with session limit of 2
        RADUSER jleno,xyzzy,1,18000,10.10.3.21 /Update
           Overwrite any existing record for 'jleno' with
              password=xyzzy
              sessionTimeout of 5 hours
              fixed IP of 10.10.3.21
        RADUSER jleno,xyzzy,1,18000,10.10.3.21 /edit
              Drop into interactive user edit mode.


*/


/* Initialize global variables */
TRUE=1
FALSE=0
tab="09"x

/* Load REXXUTIL */
CALL rxfuncadd sysloadfuncs, rexxutil, sysloadfuncs
CALL sysloadfuncs

etcDir=Value('ETC',,'OS2ENVIRONMENT')
if LENGTH(etcDir) > 3 then do
   etcDir = STRIP(etcDir, "T", "\") || "\"
end

radDbDir=etcDir || "raddb\"
radUserFile = "Users"
radUserPath = radDbDir || radUserFile



/* --------------------- BEGIN -------------------------------------- */
Say "RADIUS user management"
Say ""


call DeclareDatabase
call LoadUserDatabase


PARSE ARG loginId','user.DBLoginPW','user.DBSessionLimit','user.DBSessionTimeout','user.DBFixedIP "/" thisCommand
user.DBLoginID=loginId

  thisCommand=STRIP(TRANSLATE(thisCommand))

  do stripField=1 TO numDBFields
    user.stripField=STRIP(user.StripField,"B")
    DO WHILE POS('~', user.stripField) > 0
       user.stripField = DELSTR(user.stripField, POS('~', user.stripField), 1)
    END
  end

  commandlineRecord=MakeDBRecord()


  interactive=FALSE
  commandOk=TRUE
  select
    when thisCommand="" then do
       title="Interactive maintenance"
       interactive=TRUE
    end
    when thisCommand="CREATE" then do
       title="Create user"
       CALL ShowSettings
       call UpdateRadiusDB("ADD")
    end
    when thisCommand="EDIT" then do
       /* Interactive edit, passed on command line */
       title="Modify user"
       interactive=TRUE
       CALL EditUser(commandlineRecord)
    end
    when thisCommand="DELETE" then do
       title="Delete user"
       call UpdateRadiusDB("DELETE")
    end
    when thisCommand="UPDATE" then do
       title="Change user"
       CALL ShowSettings
       call UpdateRadiusDB("UPDATE")
    end
    otherwise do
       say "??? Unknown Command ??? " || thisCommand
       commandOk=FALSE
       interactive=TRUE
    end
  end

  if \interactive THEN DO
    /* Common exit for commandline invoked */
    EXIT 0
  END
  

DO UNTIL (haveLoginId)

   IF loginId="" THEN DO
      Say
     Call Charout, "Enter user login name : "
     loginId=GetString()
   END

   loginId=STRIP(loginId,"B")
   if loginID="" then do
      exit 1
   end

   if FindUserRecord(loginId) then do
      Say
      Say  "User "loginId" already exists"
      Say
      say tab"<E>dit user record"
      say tab"<D>elete user from system"
      say tab"change <L>ogin name"
      Call Charout,  tab"oops! <R>e-enter name.... "
      DO UNTIL ( POS(key, "EDLR") > 0)
        parse upper value SysGetKey('NOECHO') with key
        /* Check for ESCAPE */
        IF key='1B'X THEN EXIT 1
      END
      Say key

     SELECT
       WHEN key='E'  THEN DO    /* Edit user */
          dBRecord=MakeDBRecord()
          CALL EditUser(dbRecord)
          EXIT 0
       END
       WHEN key='D'  THEN DO    /* Delete user */
          dBRecord=MakeDBRecord()
          rc1=DeleteUser(dbRecord,TRUE)
          EXIT 0
       END
       WHEN key='L'  THEN DO    /* Change login name */
          oldDBRecord=MakeDBRecord()
          CALL RenameUser(oldDBRecord)
          EXIT 0
       END
       WHEN key='R'  THEN DO    /* Re-enter */
         haveLoginId = FALSE
         loginID=""
       END
     END


   end
   ELSE do
      haveLoginID=GetYesNo("Create user" loginId)
      if \haveLoginID then do
        loginID=""
      end
   end
end

/* Create new user */
createDBRecord=DefaultDbRecord(loginId)

createDBRecord=CreateUser(createDBRecord)


EXIT 0


/* =================================================================== */
RenameUser:
  oldRecord=ARG(1)
  oldLoginID=user.DBloginID

  do until(okToRename)
      do until ( (newLoginID \= "") & (newLoginID \= oldLoginID))
         Say
         Call Charout, "Enter NEW login name : "
         newLoginId=GetString()
         newLoginId=STRIP(newLoginId,"B")
      end

      okToRename=TRUE
      if FindUserRecord(newLoginId) then do
        okToRename=FALSE
        Say
        Say "   **** Sorry, user" newLoginId " already exists as "
        call ShowUserNames
      end

      if okToRename then do
         Say
         okToRename=GetYesNo("Rename " || oldLoginID|| " to " newLoginID)
      end

  end

  call parseUser(oldRecord)

  call AddAudit("Rename user " || oldLoginID|| " to " newLoginID":")
  Say
  Say

  prompt=FALSE
  rc1=DeleteUser(oldRecord, prompt)

  say

  parse VAR oldRecord oldName '~' remainder
  newRecord=newLoginId || '~' || remainder

  newRecord=CreateUser(newRecord)

  return


/* =================================================================== */
/* Create the requested userId */
EditUser:
  oldRecord=ARG(1)
  call ParseUser(oldRecord)
  do i=1 TO numDBFields
     olduser.i=user.i
  end

  editRecord=oldRecord
  do UNTIL (GetYesNo("Ok to make changes"))
    editRecord=UserRecordEdit(editRecord)
  end
  if oldRecord=editRecord then do
     Say "Nothing was changed"
  end
  else do

     call UpdateRadiusDB("UPDATE")

     call AddAudit("Changed user " || user.DBloginID)

  end
  RETURN


/* =================================================================== */
/* Create the requested userId */
CreateUser:
  dbRecord=ARG(1)

okToCreate=FALSE
DO WHILE(\okToCreate)

  dBRecord=UserRecordEdit(dbRecord)

  IF user.DBloginID=user.DBloginPW THEN DO
    Say
    Say "    *****Warning: User login name must be different than the password"
  END

  IF LENGTH(user.DBloginID) < 4 THEN DO
    Say
    Say "    *****Warning: User login name should be at least 4 characters"
  END
  IF LENGTH(user.DBloginID) > 13 THEN DO
    Say
    Say "    *****Warning: User login name should be no more than 13 characters (abs. max 16)"
  END

  /*  Not needed for our RADIUS
  IF LowerCase(user.DBloginID) \= user.DBloginID THEN DO
    Say "Warning: User login name should be all lower case characters"
  END
  */

  IF LENGTH(user.DBloginPW) < 4 THEN DO
    Say
    Say "    *****Warning: Password should be at least 4 characters"
  END
  IF LENGTH(user.DBloginPW) > 13 THEN DO
    say "   Password length is " LENGTH(user.DBloginPW)
    Say "    *****Warning: Password should be no more than 13 characters (abs. max 16)"
  END
  IF LowerCase(user.DBloginPW) \= user.DBloginPW THEN DO
    Say
    Say "    *****Warning: Password should be all lower case characters"
  END

  IF POS(TRANSLATE(user.DBloginPW), TRANSLATE(user.DBrealName)) > 0 THEN DO
     Say
     say "   *******  Warning: May not be a very good password!"
     say "            Suggest not using part of their name as a password!!"
  END

  okToCreate=GetYesNo('Ok to create user "'||  user.DBloginID || '/' ||,
                    user.DBloginPW ||'", "' || user.DBrealName || '"')
  if \okToCreate THEN DO
    IF \GetYesNo("Re-enter") THEN DO
      EXIT 1
    END
  END

END

Say

call UpdateRadiusDB("ADD")

call AddAudit("Add user " || user.DBloginID)

return dbRecord


/* =================================================================== */
DeleteDir:
  delDir=ARG(1)
  description=ARG(2)
  if ExistDir(delDir) THEN DO
    Say "Deleting " description" directory "
    "@rd " delDir " > nul 2>&1"
    IF rc \= 0 THEN DO
      "@deltree " delDir
    END

  end
  return 0

/* =================================================================== */
RenameDir:
  renDir=ARG(1)
  newDirName=ARG(2)
  description=ARG(3)

  renamed=FALSE
  if ExistDir(renDir) THEN DO
    tries=0
    do until(renamed | (tries > 3))
       Call CharOut,  "Renaming " description" directory ..."
       "@ren " renDir newDirName" > nul"
       renamed = (rc=0)
       if \renamed then do
          tries=tries+1
          Say
          say "Problem renaming directory (someone else may be accessing it)"
          call CharOut, "Try #" tries ", Pausing 5 seconds..."
          call SysSleep(5)
          Say
       end
       else do
          say "Success!"
          Say
       end
    end

    if tries > 3 then do
      say "Unable to rename directory " || renDir ". Rename it later manually."
      "@pause"
      Say
    end

  end
  return renamed


/* =================================================================== */
AddAudit:
  auditRecord=ARG(1)
  logString=Date("N") || " " || TIME("C") || " " || auditRecord
  auditFile=radDbDir || "Changes.Log"
  linesLeft=LINEOUT(auditFile, logString)
  Error=STREAM(auditFile,C,'CLOSE')
  return


/* ------------------------------------------------------------------- */
DeleteUser:
  delDbRecord=ARG(1)
  needPrompt=ARG(2)

  call ParseUser(delDbRecord)

  deleteHimOrHer=TRUE
  if needPrompt then do
     say
     say "*** ====================="
     say "*** Delete user " user.DBloginID
     say
     deleteHimOrHer=GetYesNo("Delete "user.DBloginID", are you Sure")
  end
  if deleteHimOrHer then do
     call UpdateRadiusDB("DELETE")

     call AddAudit("Deleted user " || user.DBloginID)
  end
  return 0




/* ------------------------------------------------------------------- */
/* WARNING: Any hand-entered RADIUS attributes not recognized by this
   procedure will be discarded.
   Performs the action on the user in the global user.record
 */
UpdateRadiusDB:
   /*
   'DELETE'
   'UPDATE'
   'ADD'
   */
   action=ARG(1)

   Error=STREAM(radUserPath,C,'OPEN READ')
   IF Error\="READY:" THEN DO
      Say "Error reading file " || radUserPath
      EXIT 1
   END

   lineCount=0
   lastDataLine=0
   userFound=FALSE

   DO WHILE (LINES(radUserPath) > 0)
     line=LINEIN(radUserPath)

     if POS(user.DBloginID, line) = 1 then do
        /* Delete the user and any qualification RADIUS line(s) */
        testChar=SUBSTR(line, LENGTH(user.DBloginID)+1, 1)
        if (testChar=" ") | (testChar=tab) then do
           do until (testChar \= tab) | (LENGTH(line)=0)
              line=LINEIN(radUserPath)
              testChar=SUBSTR(line, 1, 1);
              if (testChar=" ") | (testChar=tab) then do
                 /* Eat current line and get next: another user? */
                 line=LINEIN(radUserPath)
                 testChar=SUBSTR(line, 1, 1);
              end
           end
        end

     end
     lineCount=lineCount+1
     fileLine.lineCount=line
     if LENGTH(line)>0 then do
        /* Allows to strip trailing blank lines */
        lastDataLine=lineCount
     end


   END

   Error=STREAM(radUserPath,C,'CLOSE')

   /* Add new record to file and rewrite */
   tmpFile=radDbDir || "Users.new"

   call CreateOutputFile(tmpFile)
   do i=1 to lastDataLine
     call WriteToFile(fileLine.i)
   end
   if action\="DELETE"  then do
      radRec=user.DBloginID || tab || 'Password = "' || user.DBloginPW
      radRec=radRec || '", Sessions = '|| user.DBsessionLimit
      call WriteToFile(radRec)
      if user.DBsessionTimeout \= "" then do
         call WriteToFile(tab || "Session-Timeout = " || user.DBsessionTimeout*60)
      end
      if user.DBFixedIP \= "" then do
         call WriteToFile(tab || "Framed-Address = " || user.DBFixedIP)
      end
   end

   CALL CloseOutputFile

   bakFile="users.BAK"
   bakFileSpec=radDBDir || bakFile


   "@protren \sem32\radiusd\users "tmpFile || " " || radUserPath || " " || bakFileSpec
   /*
     ProtRen does same as following 3 lines except for Semaphore intelock:
   result=SysFileDelete(bakFileSpec)
   "@ren " || radUserPath || " " || bakFile
   "@ren " || tmpFile || " " radUserFile
   **** */


    IF (rc = 0) THEN DO
     say "RADIUS database updated; User has been "action"'d"
    END
    ELSE DO
      IF \GetYesNo("Problem updating RADIUS database.  Continue") THEN DO
        EXIT 1
      END
    END


  RETURN




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
GetYesNo: PROCEDURE Expose interactive

  prompt=ARG(1)


  IF \Interactive THEN DO
    Say "RADUSER: Aborting in non-interactive mode.  Would have blocked "
    Say "while asking the question ("prompt"? )"
    EXIT 1
  END


  CALL CharOut , prompt '? (Y/N): '

DO UNTIL ( (key='Y') | (key='N') )
  parse upper value SysGetKey('NOECHO') with key
  /* Check for ESCAPE */
  IF key='1B'X THEN EXIT 1
END

Say key
RETURN (key='Y')


/* ===================================================================
  DATABASE definition procedures: MODIFY ALL when modifying database
  definition!
   ------------------------------------------------------------------- */
DeclareDatabase:


  DBVarName.1='DBloginId'
  DBVarName.2='DBloginPW'
  DBVarName.3='DBsessionLimit'
  DBVarName.4='DBsessionTimeout'
  DBVarName.5='DBFixedIP'

  /* Update when adding a new field!!! */
  numDBFields = 5

  /* Assign numbers to database field names */
  do i=1 TO numDBFields
     cmd=DBVarName.i"="i
     INTERPRET cmd
  end


  RETURN


/* -------------------------------------------------------------------
  Return a default user record, given the login ID
  ------------------------------------------------------------------- */
DefaultDbRecord: PROCEDURE
  userId=ARG(1)
  newRecord=userID || "~*~2~~~~~"
  RETURN newRecord


/* -------------------------------------------------------------------
  END DATABASE definition procedures.
  =================================================================== */




/* ------------------------------------------------------------------- */
LoadUserDatabase:
  Error=STREAM(radUserPath,C,'OPEN READ')
  IF Error\="READY:" THEN DO
     Say "Error reading file " || radUserPath
     EXIT 1
  END

  userCount=0

  haveLine=FALSE
  DO WHILE (LINES(radUserPath) > 0)
    IF \haveLine THEN DO
      line=LINEIN(radUserPath)
      testChar=SUBSTR(line, 1, 1);
    END
    haveLine=FALSE
    IF (LENGTH(line) > 3) & (testChar \= '#') THEN DO

       user.=""
       /* Translate tab to space so parse will work */
       line=TRANSLATE(line, " ", tab)
       PARSE VAR line user.DBLoginID . '= "' user.DBLoginPW'", Sessions = 'user.DBSessionLimit

       line=LINEIN(radUserPath)

       testChar=SUBSTR(line, 1, 1)
       haveData=TRUE
       DO WHILE ((testChar==" ") | (testChar=tab)) & haveData
         PARSE VAR line . "Session-Timeout = "timeout
         timeout=STRIP(timeout)
         IF timeout\="" THEN DO
           user.DBsessionTimeout=timeout
         END
         PARSE VAR line . "Framed-Address = "addr
         addr=STRIP(addr)
         IF addr\="" THEN DO
           user.DBFixedIP=addr
         END
         IF LINES(radUserPath)=0 THEN DO
           haveData=FALSE
         END
         line=LINEIN(radUserPath)
         testChar=SUBSTR(line, 1, 1);
       end


       IF (user.DBLoginID \= "DEFAULT") & (user.DBLoginID \= "DEFAULT.ppp") THEN DO
         userCount=userCount+1
         masterDB.userCount=MakeDBRecord()
       END
       haveLine=TRUE
    END


  END

  Error=STREAM(radUserPath,C,'CLOSE')

  masterDB.dbLineCount=userCount

  RETURN




/* -------------------------------------------------------------------
  Reverse of ParseUserRecord above; reassemble
     'user.' into a database record and RETURN it
  ------------------------------------------------------------------- */
MakeDBRecord:
  retString=""
  do parseField=1 TO numDBFields
     retString=retString || user.parseField || '~'
  end

  RETURN retString



/* -------------------------------------------------------------------
  Search user database in memory for the passed userId record.
  **** Modifies the global user. database record *********
  RETURN TRUE if found with user information in global user database record.
  ------------------------------------------------------------------- */
FindUserRecord:
  userId=LowerCase(ARG(1))
  i=1
  found=FALSE
  do WHILE (i<=masterDB.dbLineCount) & \found
    call ParseUser(masterDB.i)
    if userId=LowerCase(user.DBloginID) then do
      found=TRUE
    end
    i=i+1
  end


  RETURN found



/* -------------------------------------------------------------------
  Show the record fields for current 'user.'
  ------------------------------------------------------------------- */
ShowSettings:

/*
login ID:
login PW:
Session Limit   : 1           
Session Timeout : 0
Fixed IP        :
*/
  Say
  say "-------------------------Current Settings ---------------"
  say "login ID: "user.DBloginID
  say "login PW: "user.DBloginPW
  say "Session Limit   : "user.DBsessionLimit
  CALL CHAROUT, "Session Timeout : "
  IF user.DBsessionTimeout \= "" THEN DO
    Say user.DBsessionTimeout/60" Minutes"
  END
  ELSE DO
    Say
  END
  say "Fixed IP	       : "user.DBFixedIP

  say "---------------------------------------------------------"

  RETURN




/* -------------------------------------------------------------------
  Get a string, accepting F10 and setting boolean 'moreEdit'
  ------------------------------------------------------------------- */
GetString:
   newString=""
   do while (1)
      key = SysGetKey("noecho")
      d2ckey = C2D(key)

      SELECT
         WHEN d2ckey = 13 THEN      /* Carriage-return was pressed */
            DO
               moreEdit=TRUE
               Say
               if newString==" " then do
                  /* Return tilde.  It will be stripped out, thus
                     creating a blank string to replace the old string. */
                  newString="~"
               end
               RETURN newString
            END

         WHEN d2ckey = 8 THEN       /* Backspace pressed */
            DO
               IF length(newString) > 0 THEN DO
                 call CharOut, "08"x "08"x
                 newLen=LENGTH(newString)-1
                 newString=SUBSTR(newString, 1, newLen)
               END
            END

         WHEN d2ckey = 27 THEN DO     /* ESCape was pressed, abort */
            Say
            exit 1
         END

         WHEN d2ckey = 224 | d2ckey = 0 THEN   /* escape-sequence in hand ? */
            DO
               key2 = SysGetKey("noecho")       /* get next code */
               d2ckey2 = C2D(key2)

               IF d2ckey2 = 68 THEN do    /* F10 was pressed, save */
                 moreEdit=FALSE
                 say
                 RETURN newString
               END
            END
         WHEN (d2ckey>=32) & (d2ckey <= 127) THEN
            DO
               newString=newString || key
               call CharOut, key
            END

      END
   end
   RETURN ""


/* -------------------------------------------------------------------*/
EditString:
  recNum=ARG(1)
  prompt=ARG(2)
  call CharOut, Prompt "<"user.recNum">: "
  testStr=GetString()
  if testStr \= "" then do
     user.recNum=testStr
  end
  return ""


/* -------------------------------------------------------------------*/
EditBool:
  moreEdit=TRUE
  recNum=ARG(1)
  prompt=ARG(2)
  if user.recNum then do
     default="Y"
  end
  else do
     default="N"
  end
  call CharOut, Prompt "(Y/N) <"default">: "

   DO UNTIL ( (key='Y') | (key='N') | (key='0D'x) )
     parse upper value SysGetKey('NOECHO') with key
     /* Check for ESCAPE */
     IF key='1B'X THEN EXIT 1
     IF key='00'X THEN DO
         key2 = SysGetKey("noecho")       /* get next code */
         d2ckey2 = C2D(key2)

         IF d2ckey2 = 68 THEN do    /* F10 was pressed, save */
           moreEdit=FALSE
           key="0D"x
         END
     END
   END

   if key="Y" then do
     user.recNum=TRUE
   end
   if key="N" then do
     user.recNum=FALSE
   end

  Say key
  return ""

/* -------------------------------------------------------------------*/
EditEnum:
  moreEdit=TRUE
  recNum=ARG(1)
  prompt=ARG(2)
  say
  validChoices="0D"x
  do i=1 to DBenumCount.recNum
     Call CharOut, "<" || DBenumString.recNum.i || "> - "
     validChoices=validChoices || DBenumString.recNum.i
     Say DBenumDescr.recNum.i
  end
  call CharOut, tab Prompt ": <"user.recNum">: "

   DO UNTIL ( POS(key, validChoices) > 0)
     parse upper value SysGetKey('NOECHO') with key
     /* Check for ESCAPE */
     IF key='1B'X THEN EXIT 1
     IF key='00'X THEN DO
         key2 = SysGetKey("noecho")       /* get next code */
         d2ckey2 = C2D(key2)

         IF d2ckey2 = 68 THEN do    /* F10 was pressed, save */
           moreEdit=FALSE
           key="0D"x
         END
     END
   END

   if key \= "0D"x then do
     user.recNum=key
   end

  Say key
  Say
  return ""

/* -------------------------------------------------------------------
  Edit the database fields in 'user.'
  ------------------------------------------------------------------- */
UserRecordEdit:

  recToEdit=ARG(1)
  call ParseUser(recToEdit)

  call ShowSettings
  Say
  Say "Edit record for Login Id "user.DBloginId
  Say "  (F10 to accept remaining fields)"
  x=EditString(dbloginPW, "Login Password")

  if moreEdit then
    x=EditString(DBSessionLimit, "Max simultaneous logins")

  IF user.dbSessionTimeout \= "" THEN DO
    user.dbSessionTimeout=user.DBSessionTimeout / 60
  END
  if moreEdit then
    x=EditString(DBSessionTimeout, "Max logon time (blank or 0 for unlimited)")

  if moreEdit then
    x=EditString(DBFixedIP, "Fixed IP Address (blank for dynamic)")

  do stripField=1 TO numDBFields
    user.stripField=STRIP(user.StripField,"B")
    DO WHILE POS('~', user.stripField) > 0
       user.stripField = DELSTR(user.stripField, POS('~', user.stripField), 1)
    END
  end

  if DATATYPE(user.DBsessionTimeout)\="NUM" then do
    user.DBSessionTimeout=""
  end
  else do
     user.dbSessionTimeout=user.DBSessionTimeout * 60
     if user.DBsessionTimeout=0 then do
       user.DBSessionTimeout=""
     end
  end

  call ShowSettings
  RETURN MakeDBRecord()


/* -------------------------------------------------------------------
  Parse the passed database record to the compound variable
     'user.'
  ------------------------------------------------------------------- */
ParseUser:
  dbRecord=ARG(1)
  rawdbRecord=ARG(1)
  do parseField=1 TO numDBFields
     parse VAR dbRecord user.parseField '~' dbRecord
  end

  if DATATYPE(user.DBsessionLimit)\="NUM" then do
     say "Database record is corrupt- Non-numeric session limit-"
     say rawdbRecord
     EXIT 1
  end

  if (user.DBsessionLimit < 0) | (user.DBsessionLimit > 20) then do
     say "Database record is corrupt: Bogus session limit-" user.DBsessionLimit
     say rawdbRecord
     EXIT 1
  end

  if POS("~", user.numDBFields) > 0 THEN DO
     say "Database record is corrupt: too many fields in-"
     say rawdbRecord
     EXIT 1
  end

  RETURN


/* =================================================================== */
StringToEnum:
  index=ARG(1)
  retStr="A"
  do enumIndex=1 TO DBenumCount.index
     if DBenumDescr.index.enumIndex=user.index then do
        retStr=DBenumString.index.enumIndex
     end
  end
  return retStr

/* =================================================================== */
EnumToString:
  index=ARG(1)
  retStr=DBenumDescr.1
  do enumIndex=1 TO DBenumCount.index
     if DBenumString.index.enumIndex=user.index then do
        retStr=DBenumDescr.index.enumIndex
     end
  end
  return retStr




/* ------------------------------------------------------------------- */
LowerCase: PROCEDURE

  string=ARG(1)
  lowString=TRANSLATE(string, "abcdefghijklmnopqrstuvwxyz",,
                              "ABCDEFGHIJKLMNOPQRSTUVWXYZ")

  RETURN (lowString)


