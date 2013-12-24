Os/2 Port Of RADIUS - Package version 1.16A
----------------------------------------------

Author: Mike Nice
        niceman@worldnet.att.net

        Copyright (c) Mike Nice, 1999

        This program may be freely used and modified for your own use, 
        providing this notice remains intact.
        - There are no expressed or implied warranties with this software.
        - All liabilities in the use of this software rest with the user.

Purpose:
  Provide a robust RADIUS server under OS/2 with additional features.
    o Multithreaded
    o Written in IBM VisualAge C++
    o Limit number of concurrent sessions a user can be on.
    o Compact USERS file so more users can be handled with the ASCII 
      file before requiring a database format.

Associated Utilties:
    o WhosOn utility lets you view users online through a CGI program 
      or PowerWeb add-in
    o RUNSTATS utility monitors number of users to anticipate the 
      next phone line expansion.
    o REXX stub example to update active users file.
    o PROTREN - protected rename utility renames the file 'users'
      with semaphore protection so no authentications are missed due
      to an access conflict.



Limitations of current version:
----------------------------------
 - Does not support Ascend accounting records.
 - No link to external user database

 (If you add any neat features, please send them to me!)


HISTORY
-------
1.16b  1-05-99 - Bug fix when checking for multiple logins
1.16c  1-21-99 - Correction multiple logins code
                 Added Runstats output to MRTG.


What is Radius?
----------------------------------------------

For an introduction, refer to:
http://www.livingston.com:80/Tech/Technotes/500/510015.html

Here is some additional information on how the RADIUS server
interacts with the USERS file.  RADIUS works on a transaction 
basis, with the client and server exchanging a list of 
attribute/value pairs.  Conceptually, the client sends the 
server a "message" similar to this:

	Query
	Client-Id = "205.157.137.10"
	Client-Port-Id = 15
	User-Name = "joe"
	Password = "mypass"

This is the IP address of the RADIUS client, the port, and the name and 
password given by the guy trying to log on.  The RADIUS server looks at 
this and decides if the caller should be given access.  The server then 
decides and sends back either something like this:

	Reject
	Port-Message = "Your account has expired"

or something like this:

	Accept
	User-Service-Type = Framed-User
	Framed-Protocol = PPP	
	et cetera

In addition, having this whole attribute/value pair system with a whole 
mechanism for encoding and decoding these things (which is what the 
dictionary file is for), they use the same idea for encoding everything 
in the users file.  This means that you have query values (which you 
never see -- they are what the client sends to the server), check values, 
and return values.

So what does this mean to you?  Hopefully it helps in understanding the 
structure of the users file.  The users file consists of a series of 
entries, one for each user.  An entry consists of exactly one 
non-indented line followed by zero or more indented lines.  The one 
non-indented line consists of a user name, followed by some space, 
followed by a series of check values.  All of the indented lines consist 
of one or more return values.

To simplify even further, the line with the user name contains the check 
values.  The indented lines contain the return values.

Check values are only used internally within the RADIUS daemon to make 
go/no go decisions on whether or not to allow the caller to log on.  This 
includes stuff like the password that must have been given, and when the 
caller's account expires.

Return values are not used by the RADIUS server in any way.  They are 
sent to the client (Terminal Server, etc) to tell it how to handle the 
caller.


Files included
----------------------------------

radius116a\FILE_ID.DIZ  Overall package description file
radius116a\readme.os2   This file
radius116a\radius\clients.example   RADIUS - sample setup file
radius116a\radius\dictionary        RADIUS - runtime protocol definition
radius116a\radius\radiusd.exe       RADIUS - Main executable
radius116a\radius\runrad.cmd        RADIUS - Command file to start RADIUS
radius116a\radius\users.example     RADIUS - sample user accounts file

radius116a\runstats\runstats.exe    Program to sample # of users logged on
radius116a\runstats\userhist.cmd    Summarize results of runstats

radius116a\runstats\whosonMRTG.cfg  Sample configuration file to use with MRTG
radius116a\runstats\getWhosOn.cmd   Called by MRTG (via whosonMRTG.cfg)
    NOTE: MRTG is not required if all you need is a text summary from userhist.cmd

radius116a\useredit\protren.exe     Protected rename utility
radius116a\useredit\raduser.cmd     USERS file edit; calls PROTREN.EXE

radius116a\whoson\readme.htm      WHOSON documentation
radius116a\whoson\whoson.dll      Executable; PowerWeb version
radius116a\whoson\whoson.exe      Executable; CGI version

radius116a\src\radius   Source code for RADIUSD.EXE in IBM Visual Age C++
radius116a\src\runstats Source code for RUNSTATS.EXE in IBM Visual Age C++
radius116a\src\whoson   Source code for WHOSON.* in IBM Visual Age C++



Installation:
----------------------------------
(There's no installation utility because it's free!)

Must be installed on an HPFS partition.

Create a directory called radius and unzip the archive there.

Find out where environment variable ETC points to and create subdirectories 
radacct and raddb.

%ETC%
  |
  |-- radacct
  |-- raddb
          dictionary
	  clients
	  users

In the raddb directory copy clients.example from the radius directory 
and name it clients and edit it. Also copy users.example and rename it 
to users and edit it.  Copy the dictionary file there also. Errors are
logged to a file called logfile in this directory.

In the radacct subdirectory, a directory for each server will be created
by radius and a file called detail will be full of accounting info.

Also edit your %etc%\services file and add the following lines:
radius          1645/udp radiusd
radacct         1646/udp

Execution

The example REXX program RUNRAD.CMD provides a good starting point.  It
contains the command line options to pass to the program as well
as making sure it restarts after a crash.

Command line options:

 -a [account dir]
   Stores accounting files in specified directory.

 -d [radius directory]
   Use specified directory for operations files
   
 -n 
   Log names to STDOUT.  Useful for seeing activity at a glance.
   
 -p
   Log password failures to LOGFILE.  Useful for debug purposes to
   assist in user troubleshooting.  This is a potential security risk
   because near passwords are leaked.
   
 -v
   Display version as program starts up.
   
 -x
   Run in Debug mode.  Display additional user messages for 
   debug assistance.
 


Livingston Info:
-----------------
 *	Permission to use, copy, modify, and distribute this software for any
 *	purpose and without fee is hereby granted, provided that this
 *	copyright and permission notice appear on all copies and supporting
 *	documentation, the name of Livingston Enterprises, Inc. not be used
 *	in advertising or publicity pertaining to distribution of the
 *	program without specific prior permission, and notice be given
 *	in supporting documentation that copying and distribution is by
 *	permission of Livingston Enterprises, Inc.   

