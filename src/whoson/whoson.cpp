/* PowerWeb add-in to work with backroads RADIUS server;
   Show who's on as a web page with Server-push.
   When built as CGI_BIN, operates as standard CGI-BIN add in.
*/
    
#pragma	strings(readonly)

#define INCL_DOSERRORS
#define INCL_DOSSEMAPHORES
#define INCL_DOSPROCESS
#include <os2.h>

#include	<time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef CGI_BIN
  #define HOOK_OK 0
  #define HOOK_ERROR 1
#else
  #include	<PowerAPI.hpp>
#endif

#if	defined(__IBMCPP__)
	#define	EXPORT	_Export
#else
	#define	EXPORT
#endif

enum PortFormat {nasPortOnly, nasSlotPort};


typedef unsigned short int UINT2;

#include	"radius.h"

#include "shrMemry.h"
#include "clients.h"
#include "tree.h"


#define Boolean int
#define true 1
#define false 0

const unsigned MaxMessageSize=1024*128;


static const char*  pszContent = "multipart/x-mixed-replace;boundary=\"NetscapeServerPush\"";
static const char*  boundary = "NetscapeServerPush";
static const char*  crlf = "\r\n";

/* pointer to root marker in shared memory */
static struct RootMarker * rootMarker = NULL;

static HMTX  acctSem;
static void * heapBase;



void * memfind(void * block, char * searchString, int searchSize) {
  char * start=(char *)block;
  Boolean found=false;
  int sizeLeft=searchSize;
  do {
    char * firstPos=start;
    start=(char *)memchr(start, searchString[0], sizeLeft);
    if (start) {
      sizeLeft -= (start-firstPos);
      /* This ignores pathological case of strcmp wandering off the
         end of last block...mem block is initialized to nulls */
      found=(strcmp(searchString, start) == 0);
    }
  } while (start && !found);
  return (start);
}



Boolean RootMarkerValid() {
  if (rootMarker) {
    if (strcmp(rootMarker->eyeCatcher, EyeCatcher) != 0) {
      // Program reloaded; relocate marker.
      rootMarker=0;
    }
  }
  if (!rootMarker) {
    rootMarker=(RootMarker *)memfind(heapBase, EyeCatcher, HeapSize);
  }
  return (rootMarker != 0);
}



/*  Append - has 2 functions
  1.  Prevents overflowing allocated string buffer when there are
      a large number of users and servers.
  2.  Prevents strcat from needing to search the entire string buffer
      each time.

IN - buffer: buffer to append to
     index: next position in buffer
     newStr: string to append
OUT - newStr appended to buffer if enough room
    - index updated
    newStr silently discarded on error.
*/
void Append(char *buffer, unsigned & index, const char *newStr) {
  unsigned newLen=strlen(newStr);
  if ((index+newLen+1) < MaxMessageSize) {
    strcpy(&buffer[index], newStr);
    index+=newLen;
  }
}

/*************************************************************************
 *
 *	Function: send_users
 *
 *	Purpose: Send HTML page formatted as who's on by reading shared memory image.
 *
 *************************************************************************/
/* Create HTML document like:

<TITLE>Current Users</TITLE></HEAD>
<BODY>
<CENTER><H1>Current Users</H1>

<!- Begin nested table -->
<table cellpadding=15 width=50%>

<TR><TD valign=top>

<TABLE BORDER WIDTH=100%>
<TR ALIGN=CENTER><th colspan=3><FONT SIZE=+2>dial1-96</font></tr>
<TR ALIGN=CENTER><TH>Username</TH><TH>Port</TH><TH>IP Addr</TH><TH>Time Online</TH></TR>
<TR ALIGN=CENTER><TD>mjackson</TD><TD>1</TD><TD>127.0.0.1</TD><TD>00:14:52</TD></TR>
<TR ALIGN=CENTER><TD>madonna</TD><TD>2</TD><TD>127.0.0.1</TD><TD>00:52:41</TD></TR>
<TR ALIGN=CENTER><TD>mjagger</TD><TD>3</TD><TD>127.0.0.1</TD><TD>01:08:54</TD></TR>

... repeat...

</TABLE>
</TD>

 ..optionally
  repeat for next server; 2 per row

  </TR>


</TABLE> <!- End nested table -->

</CENTER>
<P>Return to <A HREF="/">Home</A>

</BODY></HTML>
*/


static const char*	boldOn="<B>";
static const char*	boldOff="</B>";
static const char*	empty="";



/*************************************************************************
 *
 *	Function: IPToString
 *
 *	Purpose: Return an IP address in standard dot notation for the
 *		 provided address in host long notation.
 *
 *************************************************************************/
void IPToString(UINT4 ipaddr, char *buffer)
{
	int	addr_byte[4];
	int	i;
	UINT4	xbyte;

	for(i = 0;i < 4;i++) {
		xbyte = ipaddr >> (i*8);
		xbyte = xbyte & (UINT4)0x000000FF;
		addr_byte[i] = xbyte;
	}
	sprintf(buffer, "%u.%u.%u.%u", addr_byte[3], addr_byte[2],
		addr_byte[1], addr_byte[0]);
}

// Global variables used for tree transversal callback functions.
// They can be global because they are protected by a semaphore
// at this time.
static char * g_str;
static unsigned g_index;
static time_t		g_now;
static PortFormat g_portFormat;


/* Callback function for tree traversal; If entry valid, add text
   row table HTML entry.  */
int __cdecl ListUser(char * treeData) {

  struct USERS * userNode = (struct USERS *)treeData;
  if (userNode->name) {

    char userEntry[300];
    // Name
    sprintf(userEntry, "<TR ALIGN=CENTER><TD>%s</TD>", userNode->name);
    Append(g_str, g_index, userEntry);

    // Port number
    if (g_portFormat ==nasPortOnly) {
      sprintf(userEntry, "<TD>%d</TD>", userNode->nasPort);
    } else {
      sprintf(userEntry, "<TD>%d:%d</TD>", userNode->nasPort / 256,
                                          userNode->nasPort % 256 );
    }
    Append(g_str, g_index, userEntry);

    // IP Address
    Append(g_str, g_index, "<TD>");
    IPToString(userNode->userIPAddress, userEntry);
    Append(g_str, g_index, userEntry);
    Append(g_str, g_index, "</TD>");

    // Time online
    int elapsed, hh, mm, ss;
    elapsed = g_now - userNode->began;
    if (elapsed < 0) {
      // System clock time change; don't show negative times.
      elapsed = 0;
    }
    const char * bold1=empty;
    const char * bold2=empty;
    if (elapsed < 60) {
      bold1=boldOn;
      bold2=boldOff;
    }
    hh = elapsed /3600;
    elapsed = elapsed % 3600;
    mm = elapsed / 60;
    ss = elapsed % 60;
    sprintf(userEntry, "<TD>%s%02ld:%02ld:%02ld%s</TD></TR>\n",
             bold1, hh, mm, ss, bold2);
    Append(g_str, g_index, userEntry);
  }

  // Send more
  return (1);
}





void send_users(char * str, time_t & lastCheck, unsigned & index,
                PortFormat portFormat)
{
	struct CLIENTS 	*client;

  Append(str, index, "<TITLE>Who's On</TITLE></HEAD>");
  Append(str, index, "<BODY><CENTER><H1>Current Users</H1>");
  Append(str, index, "<table cellpadding=15 width=50%>");

	time(&g_now);

  unsigned clientCount=0;
	for(client=rootMarker->client_list; client; client=client->next) {
    if ((clientCount % 2)==0) {
      Append(str, index, "<TR>");
    }
    clientCount++;


    Append(str, index, "<TD valign=top>");
    Append(str, index, "<CENTER><TABLE BORDER WIDTH=100%><TR>");
    Append(str, index, "<TR ALIGN=CENTER><th colspan=4><FONT SIZE=+2>");
    Append(str, index, client->clientname);
    Append(str, index, "</font></tr>");
    Append(str, index, "<TR ALIGN=CENTER><th colspan=4><td>Total users: ");
    char numberString[53];
    sprintf(numberString, "%d</TD></TR>\n", client->clientCount);
    Append(str, index, numberString);
    Append(str, index, "<TR ALIGN=CENTER><TH>Username</TH><TH>");
    if (portFormat!=nasPortOnly) {
      Append(str, index, "Slot: ");
    }
    Append(str, index, "Port</TH><TH>IP Addr</TH><TH>Time Online</TH></TR>\n");

    g_str=str;
    g_index=index;
    g_portFormat = portFormat;

    tree_trav(&client->userTree, ListUser);

    index=g_index;

    Append(str, index, "</TABLE></TD>");
  }

  Append(str, index, "</TABLE></CENTER><P>");
  Append(str, index, "Return to <A HREF=""/"">Home</A>");
  lastCheck=rootMarker->lastUpdated;
}




// -------------------------------------------------------------------------

Boolean InitializeWhosOn() {

  APIRET  rc;

  if (acctSem==0) {
    rc=DosCreateMutexSem(ClientsSemName, &acctSem, 0, FALSE);
    if (rc==ERROR_DUPLICATE_NAME) {
      /* Already existed */
      rc=DosOpenMutexSem(ClientsSemName, &acctSem);
    }
    if (rc!=0) {
      acctSem=0;
      fprintf (stderr, "WhosOn: Unable to get semaphore: %s\n",
               ClientsSemName);
      return (false);
    }
  }

  Boolean ok=true;
  if (heapBase==0) {
    rc = DosRequestMutexSem(acctSem, -1);
    if (!GetSharedMemory(SharedMemName, HeapSize, &heapBase)) {
       fprintf (stderr, "WhosOn: Unable to access shared memory\n");
       heapBase=0;
       ok=false;
    }
    rc=DosReleaseMutexSem(acctSem);
  }
  return (ok);
}



// -------------------------------------------------------------------------
void FreeResources() {

}


Boolean SameTime(time_t lastCheck) {
  Boolean same=true;

  APIRET rc = DosRequestMutexSem(acctSem, -1);
  if (RootMarkerValid()) {
    same=(rootMarker->lastUpdated == lastCheck);
  }
  rc=DosReleaseMutexSem(acctSem);

  return (same);
}



// -------------------------------------------------------------------------
#ifdef CGI_BIN
/* Function to search from 'start' to next '='.  Then extract up to
   (resultSize-1) chars into result or until space or '&'.  Add 
   trailing nul and return.
   If argument is too long to fit into result, it is truncated */
void GetCGIArg(char * start, char * result, int resultSize) {
  char * ch=start;

  int charCount=0;

  while ((*ch != '=') && (*ch != '&') && (*ch != 0) ) {
    ch++;
  }

  if (*ch=='=') {
    ch++;
    while (*ch == ' ') {
      ch++;
    }

    while ( ((charCount +1)< resultSize) && 
            (*ch!=' ') && (*ch!='&') && (*ch) ) {
      result[charCount] = *ch;
      ch++;
      charCount++;
    }
    
  }
  result[charCount] = 0;

}
#endif




// -------------------------------------------------------------------------
//	This function is called directly from a URL without needing an
//	associated resource to manage it.

#ifdef CGI_BIN
  int main (int argc, char *argv[])
#else
  long		APIENTRY	EXPORT WhosOn	(void*	parcel)
#endif
{
	void*		html;
  time_t lastCheck=0;


  if (!InitializeWhosOn()) {
    FreeResources();
    #ifdef CGI_BIN
      puts("Content-type: text/html<HTML> Whoson: unable to initialize resources");
    #endif
    return(HOOK_ERROR);
  }

  PortFormat portFormat=nasPortOnly;

  char * str=new char[MaxMessageSize];
  str[0]=0;
  unsigned index=0;

#ifndef CGI_BIN

	//	Get a handle to the HTML result output variable.
	ServerFind(parcel, "Request:/Result", &html);

	// Output our message, surrounded by standard HTML
  ServerWriteText(parcel, "Request:/Header/Out/Content-type", pszContent);
  ServerWriteText(parcel, "Request:/ImmediateWrite", "1");

  char portType[10];
  ServerReadText(parcel, "Request:/Argument/portFormat", portType, sizeof(portType)-1);
  portType[sizeof(portType)-1]=0;
  if (stricmp(portType, "slot")==0) {
    portFormat=nasSlotPort;
  }


  ServerWriteText(html, 0, "HTTP/1.0 200 OK\r\n");

  strcpy(str, "Content-type: ");
  strcat(str, pszContent);
  strcat(str, crlf);
  strcat(str, crlf);
	ServerAppendText(html, 0, str);

  // Send the boundary text for the first document
  // Mime start
  char mimeStart[100];
  strcpy(mimeStart, "--");
  strcat(mimeStart, boundary);
  strcpy(str, mimeStart);
  strcat(str, crlf);
	ServerAppendText(html, 0, str);

#else
  char * cgiQueryString=getenv("QUERY_STRING");
  // Look for port type specification
  if (cgiQueryString) {
    char * portPos=strstr(cgiQueryString, "portFormat");
    if (portPos) {
      char portType[10];
      GetCGIArg(portPos, portType, sizeof(portType));
      if (stricmp(portType, "slot") == 0) {
        portFormat=nasSlotPort;
      }
    }
  }

#endif

  Boolean quitting=false;

  do {

    index = 0;
    Append(str, index, "Content-type: text/html");

    Append(str, index, crlf);
    Append(str, index, crlf);


    #ifdef CGI_BIN
    Append(str, index, "<HTML>");
    Append(str, index, crlf);
    if (cgiQueryString) {
      char * refreshPos=strstr(cgiQueryString, "refreshRate");
      if (refreshPos) {
        char refreshRateStr[10];
        GetCGIArg(refreshPos, refreshRateStr, sizeof(refreshRateStr));
        int refreshInterval = atoi(refreshRateStr);
        if (refreshInterval > 0) {
          if (strlen(refreshRateStr) > 0) {
            Append(str, index, "<META HTTP-EQUIV=\"REFRESH\" CONTENT=");
            Append(str, index, refreshRateStr);
            Append(str, index, " >");
          }
        }
      }
    }

    #endif
    

    APIRET rc = DosRequestMutexSem(acctSem, -1);
    if (RootMarkerValid()) {
      send_users(str, lastCheck, index, portFormat);
    } else {
      Append(str, index, "<h2>Accounting program not running </h2>");
    }
    rc=DosReleaseMutexSem(acctSem);

    #ifdef CGI_BIN
    Append(str, index, "</HTML>");
    puts(str);
    quitting = true;
    #else
    Append(str, index, crlf);
    Append(str, index, mimeStart);
    Append(str, index, crlf);

    long sendResult=ServerAppendText(html, 0, str);

    if (sendResult != ERR_NONE) {
      quitting = true;
      // printf("WhosOn: Link broken!\n");
    } else {
      // printf("Sent\n");
    }

    int tries=0;
    do {
      DosSleep(15000);
      tries ++;
    } while (!quitting && SameTime(lastCheck) && tries < 6);
    #endif

  } while (!quitting);

  FreeResources();
  delete[] str;

  // printf("WhosOn: exiting.\n");
	return HOOK_OK;
}


