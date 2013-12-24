/* Statistics program to monitor number of people logged in. */

#pragma	strings(readonly)

#define INCL_DOSERRORS
#define INCL_DOSSEMAPHORES
#define INCL_DOSPROCESS
#include <os2.h>

#include	<time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


typedef unsigned short int UINT2;
typedef unsigned int UINT4;

#include "shrMemry.h"
#include "clients.h"



#define Boolean int
#define true 1
#define false 0

// Time between samples, in seconds
#define Interval 300

#define LogFile "Runstats.log"

// This is used to order the report in the order specified in
// %etc\raddb\clients, not in the order data was created
// in RADIUS accounting server.
struct SERVERS {		// Accounting server data
	char		*name;		// pointer to accounting server name
	unsigned count;   // # of users on this server
  struct SERVERS *next;
}	;


/* pointer to root marker in shared memory */
static struct RootMarker * rootMarker = NULL;

static HMTX  acctSem;
static void * heapBase;
static SERVERS * serverRoot;
static Boolean serverListChanged;

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
         end of last block...this app initialized the mem block to nulls */
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

// Write list of servers to file so following number entries are validated
void WriteHeader(FILE * logFile) {

  struct SERVERS * server=serverRoot;

  while (server) {
    fprintf(logFile, "%s", server->name);
    server=server->next;
    if (server) {
      fputc(',', logFile);
    }
  }
  fprintf(logFile, "\n");
  serverListChanged=false;
}


// Return pointer to server record.  If necessary, create a new record.
struct SERVERS * ServerRecord(const char *serverName) {

  struct SERVERS * newServ=0;

  if (!serverRoot) {
    serverRoot=new SERVERS;
    newServ=serverRoot;
  } else {
    newServ=serverRoot;
    if (strcmp(serverName, newServ->name)==0) {
      return (newServ);
    }
    while (newServ->next) {
      newServ=newServ->next;
      if (strcmp(serverName, newServ->name)==0) {
        return (newServ);
      }
    }
    newServ->next=new SERVERS;
    newServ=newServ->next;
  }

  serverListChanged=true;
  newServ->next=0;
  newServ->count=0;
  newServ->name=new char[strlen(serverName)+1];
  strcpy(newServ->name, serverName);

  return (newServ);
}


// Zero counts for all servers
void ZeroCounts(){

  struct SERVERS * server=serverRoot;

  while (server) {
    server->count=0;
    server=server->next;
  }
}




/*************************************************************************
 *
 *	Function: count_users
 *
 *	Purpose: Count total users on.
 *
 *************************************************************************/


unsigned count_users()
{
	struct CLIENTS 	*client;
	unsigned numon=0;

  ZeroCounts();
	for(client=rootMarker->client_list; client; client=client->next) {
    struct SERVERS * server=ServerRecord(client->clientname);
    server->count=client->clientCount;
    numon+=client->clientCount;
  }
  return (numon);
}


void CreateServerList() {

  #define RADIUS_DIR "\\raddb"
  #define CLIENTS_FILE "\\clients"

	char	buffer[300];
	char	hostnm[300];
	char	secret[300];
  FILE * clientfd;

  sprintf(buffer, "%s%s",getenv("ETC"),RADIUS_DIR);
  strcat(buffer, CLIENTS_FILE);

	if((clientfd = fopen(buffer, "r")) == (FILE *)NULL) {
		fprintf(stderr, "RunStats: couldn't open %s to find clients\n", buffer);
		return;
	}
	while(fgets(buffer, sizeof(buffer), clientfd) != (char *)NULL) {
		if(buffer[0] == '#') {
			continue;
		}
		if(sscanf(buffer, "%s%s", hostnm, secret) != 2) {
			continue;
		}
    ServerRecord(hostnm);
    printf("Added accounting server: %s\n",hostnm);
	}
	fclose(clientfd);

}


// -------------------------------------------------------------------------

Boolean InitializeStats() {

  CreateServerList();

  APIRET  rc;

  rc=DosCreateMutexSem(ClientsSemName, &acctSem, 0, FALSE);
  if (rc==ERROR_DUPLICATE_NAME) {
    /* Already existed */
    rc=DosOpenMutexSem(ClientsSemName, &acctSem);
  }
  if (rc!=0) {
    fprintf (stderr, "RunStats: Unable to get semaphore: %s\n",
             ClientsSemName);
    return (false);
  }

  Boolean ok=true;
  rc = DosRequestMutexSem(acctSem, -1);
  if (!GetSharedMemory(SharedMemName, HeapSize, &heapBase)) {
     fprintf (stderr, "RunStats: Unable to access shared memory\n");
     ok=false;
  }
  rc=DosReleaseMutexSem(acctSem);
  return (ok);
}



// -------------------------------------------------------------------------
void FreeResources() {
  if (acctSem) {
    DosCloseMutexSem(acctSem);
  }
  if (heapBase) {
    ReleaseSharedMemory();
  }
}



// -------------------------------------------------------------------------

int main(int argc, char**argv) {

  if (!InitializeStats()) {
    FreeResources();
    exit(1);
  }

  Boolean quitting=false;

  time_t now;
  struct tm * pTime;
  do {

    time(&now);

    if (!pTime) {
      fprintf (stderr, "RunStats: Unable to access localtime\n");
      FreeResources();
      exit(1);
    }

    pTime=localtime(&now);

    unsigned secs=pTime->tm_sec + (pTime->tm_min * 60);
    secs=Interval - (secs%Interval);
    DosSleep(secs*1000);

    time(&now);
    pTime=localtime(&now);

    char timeStamp[30];
    sprintf(timeStamp, "[%02d/%02d/%02d:%02d:%02d:%02d] ",
          pTime->tm_year%100,
          pTime->tm_mon+1,
          pTime->tm_mday,
          pTime->tm_hour,
          pTime->tm_min,
          pTime->tm_sec);


    APIRET rc = DosRequestMutexSem(acctSem, -1);
    if (RootMarkerValid()) {

      printf("%s %d users\n", timeStamp, count_users());

      // See if file exists
      FILE * logFile=fopen(LogFile, "r");
      if (logFile != NULL) {
        fclose(logFile);
      } else {
        // New file; Force header to be rewritten
        serverListChanged=true;
      }


      logFile=fopen(LogFile, "a");
      if (logFile != NULL) {

        if (serverListChanged) {
          WriteHeader(logFile);
        }

        struct SERVERS * server=serverRoot;

        fprintf(logFile, "%s", timeStamp);
        while (server) {
          fprintf(logFile," %d", server->count);
          server->count=0;
          server=server->next;
        }
        fprintf(logFile, "\n");
        fclose(logFile);
      } else {
        fprintf(stderr, "Unable to open file %s\n", LogFile);
      }

    } else {
      printf("- Accounting program not running\n");
    }
    rc=DosReleaseMutexSem(acctSem);

  } while (!quitting);

  FreeResources();

}


