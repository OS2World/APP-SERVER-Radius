/*
 *
 *	RADIUS Accounting
 *	Remote Authentication Dial In User Service
 *
 *
 *	Livingston Enterprises, Inc.
 *	6920 Koll Center Parkway
 *	Pleasanton, CA   94566
 *
 *	Copyright 1992 - 1994 Livingston Enterprises, Inc.
 *
 *	Permission to use, copy, modify, and distribute this software for any
 *	purpose and without fee is hereby granted, provided that this
 *	copyright and permission notice appear on all copies and supporting
 *	documentation, the name of Livingston Enterprises, Inc. not be used
 *	in advertising or publicity pertaining to distribution of the
 *	program without specific prior permission, and notice be given
 *	in supporting documentation that copying and distribution is by
 *	permission of Livingston Enterprises, Inc.
 *
 *	Livingston Enterprises, Inc. makes no representations about
 *	the suitability of this software for any purpose.  It is
 *	provided "as is" without express or implied warranty.
 *
 */
 
 /* Changes
  05Jan99  mn  o Corrected user session count code (Made CountSessions __cdecl to
                 match WhosOn.
               o Use session id to detect accounting server sending STOP record
                 before START, leaving a user incorrectly appearing to be logged in.
*/

static char sccsid[] =
"@(#)acct.c	1.6  Copyright 1994 Livingston Enterprises Inc";

#include	<sys/types.h>
#include	<sys/socket.h>
#include	<sys/time.h>
#include	<direct.h>
#include	<netinet/in.h>

#include	<stdio.h>
#include	<stdlib.h>
#include	<netdb.h>
#include	<pwd.h>
#include	<time.h>
#include	<ctype.h>
#include <utils.h>
#include	<signal.h>
#include	<errno.h>

#define INCL_DOSERRORS
#define INCL_DOSSEMAPHORES
#include <os2.h>

typedef unsigned short int UINT2;

#include	"radius.h"

#include "tree.h"
#include "shrMemry.h"
#include "clients.h"

static u_char		recv_buffer[4096];
static u_char		send_buffer[4096];
static HMTX  acctSem;
static HMTX  logSem;
extern char		*progname;
extern int		debug_flag;
extern int		show_names_flag;
extern char		*radacct_dir;
extern char		*radius_dir;

static char * globalSearchName;
static int globalUserSessionCount;

/* accounting status type for global Accounting ON message from
   USR HiperArc.  (May be used by other vendors also).  */
#define USR_ACCT_START 7


/* pointer to root marker in shared memory */
static struct RootMarker * rootMarker = NULL;


void FreeUserTreeNode(struct USERS * userNode) {
  if (userNode->name) {
    SHfree(userNode->name);
  }
  SHfree(userNode);
}

int CompareNode(struct USERS *val1, struct USERS * val2) {
  if (val1->nasPort < val2->nasPort) {
    return (-1);
  }
  if (val1->nasPort > val2->nasPort) {
    return (1);
  }
  return (0);
}

int __cdecl CountSessions(struct USERS * userNode) {
  if (userNode->name) {
    if (stricmp(userNode->name, globalSearchName) == 0) {
      globalUserSessionCount++;
    }
  }
  return (1);
}


void rad_accounting(AUTH_REQ	*authreq, int activefd)
{
	FILE		*outfd;
	char		*ip_hostname();
	char		clientname[128];
	char		buffer[512];
	VALUE_PAIR	*pair;
	long		curtime;
  APIRET rc;
	char		username[256];
	int		clientPort = -1;
	int		statustype = -1;
  int  rebootFlag = 0;
	UINT2		sessionmark = 0;
  UINT4 sessionID = 0;
  UINT4 userIPAddr=0;
  int userWasOn=0;
	struct CLIENTS 	*client;
	struct USERS 	*userNode;
	int 		x;
	char		*p;
  int searchPort;  /* RAMRACK hack */
	time_t		now;


	strcpy(clientname, ip_hostname(authreq->ipaddr));

  /* Wait up to 5 seconds for sem.  Assume problem if no success */
  rc = DosRequestMutexSem(acctSem, 5000);

	for(client=rootMarker->client_list; client; client=client->next)
		if(client->ipaddr == authreq->ipaddr)
			break;

	if(!client) {				/* new client */
		client = (struct CLIENTS *)SHmalloc(sizeof(struct CLIENTS));
		client->ipaddr = authreq->ipaddr;
		client->clientname = SHstrdup(clientname);
    client->clientCount = 0;
		client->sessionmarkwas = 0;
		client->userTree = NULL;
		client->next = rootMarker->client_list;
		rootMarker->client_list = client;
	}

	*username = '\0';
	pair = authreq->request;
	while(pair != (VALUE_PAIR *)NULL) {
		switch(pair->attribute)
		{
		case PW_USER_NAME:
			strcpy(username,pair->strvalue);
      p=(char *)strchr(username,'.');
			if(p)
				*p = '\0';
			break;
		case PW_ACCT_SESSION_ID:
			sessionmark = *(UINT2 *)pair->strvalue;
      sessionID = atoi(pair->strvalue);
			break;
    case PW_FRAMED_ADDRESS:
      userIPAddr=pair->lvalue;
			break;
		case PW_CLIENT_PORT_ID:
			clientPort = pair->lvalue;
			break;
		case PW_ACCT_STATUS_TYPE:
			statustype = pair->lvalue;
		}
		pair = pair->next;
	}



	if(*username && sessionmark && (clientPort >= 0)) {

   #ifdef PORTMASTER_SESSIONS
     /* 1st 2 characters of Portmaster session indicate a unique session */
		if(sessionmark != client->sessionmarkwas) {
			if(client->sessionmarkwas) {
        rebootFlag=1;
			}
			client->sessionmarkwas = sessionmark;
		}
   #else
   
   /* Check for USR Accounting ON message */
   if (statustype==USR_ACCT_START) {
     rebootFlag=1;
   }
   
   #endif
   
   if (rebootFlag) {
     /* clear user tables for this client */
     tree_mung(&client->userTree, FreeUserTreeNode);
     client->userTree = NULL;
     DEBUG("Client %s has rebooted\n",clientname);
   }


   if (clientPort > 0) {
     
     struct USERS testUser;
     testUser.name=NULL;
     testUser.nasPort=clientPort;

     userNode= (struct USERS *)
         tree_srch(&client->userTree, CompareNode, &testUser);
  
     if (!userNode) {
       userNode = (struct USERS *)SHmalloc(sizeof(struct USERS));
       if (userNode) {
         userNode->name=NULL;
         userNode->nasPort=clientPort;
         if (!tree_add(&client->userTree, CompareNode, userNode, FreeUserTreeNode)) {
           FreeUserTreeNode(userNode);
           userNode=NULL;
         }
       }
     }
  
     if (userNode) {
      /* fill in user port data */

       if(statustype == PW_STATUS_STOP) {
         DEBUG("User %s logged off %s S%d\n",  username,clientname,clientPort);
         if(userNode->name) {
           userNode->lastSessionID = sessionID;
           SHfree(userNode->name);
           client->clientCount--;
         }
         userNode->name = NULL;
       }
       else if(statustype == PW_STATUS_START) {
         DEBUG("User %s logged on %s S%d\n",  username,clientname,clientPort);
         if(userNode->name) {
           SHfree(userNode->name);
           userWasOn=TRUE;
         } else {
           client->clientCount++;
         }
         if ((sessionID > 0) && 
             (sessionID==userNode->lastSessionID) &&
             !userWasOn ){
           /*  Accounting STOP before START with same session ID on the
                same NAS port.  Assume user is already gone. */
           client->clientCount--;
         } else {
           userNode->name = SHstrdup(username);
         }
         userNode->lastSessionID = sessionID;

         time(&now);
         userNode->began = now;
         userNode->userIPAddress=userIPAddr;
       }
       
     }

   }


	}

  time(&rootMarker->lastUpdated);

  rc=DosReleaseMutexSem(acctSem);

	/*
	 * Create a directory for this client.
	 */
	sprintf(buffer, "%s/%s", radacct_dir, clientname);
	mkdir(buffer);

	/*
	 * Write Detail file.
	 */
	sprintf(buffer, "%s/%s/detail", radacct_dir, clientname);
  rc = DosRequestMutexSem(logSem, SEM_INDEFINITE_WAIT);
	if((outfd = fopen(buffer, "a")) == (FILE *)NULL) {
		sprintf(buffer,
			"Acct: Couldn't open file %s/%s/detail\n",
			radacct_dir, clientname);
		log_err(buffer);
		/* don't respond if we can't save record */
	} else {

		/* Post a timestamp */
		curtime = time(0);
		fputs(ctime(&curtime), outfd);

		/* Write each attribute/value to the log file */
		pair = authreq->request;
		while(pair != (VALUE_PAIR *)NULL) {
			fputs("\t", outfd);
			fprint_attr_val(outfd, pair);
			fputs("\n", outfd);
			pair = pair->next;
		}
		fputs("\n", outfd);
		fclose(outfd);
		/* let NAS know it is OK to delete from buffer */
		send_acct_reply(authreq, (VALUE_PAIR *)NULL,
				(char *)NULL,activefd);
	}
  rc=DosReleaseMutexSem(logSem);

	pairfree(authreq->request);
	memset(authreq, 0, sizeof(AUTH_REQ));
	free(authreq);
	return;
}


/*************************************************************************
 *
 *	Function: user_sessions
 *
 *	Purpose: Report how many currently active sessions a given user
 *		 has active at the moment.
 *
 *************************************************************************/

int user_sessions(char *username)
{
	struct CLIENTS 	*client;
	int		count = 0;
	char		buf[256];
	char		*p;
  APIRET rc;

	strcpy(buf,username);
	p=(char *)strchr(buf,'.');
	if(p)
		*p = '\0';

  rc = DosRequestMutexSem(acctSem, 1000);
  if (rc!=0) {
		log_err("user_sessions: Unable to get accounting Sem\n");
    return (0);
  }

  globalSearchName=buf;
  globalUserSessionCount=0;
	for(client=rootMarker->client_list; client; client=client->next)
    tree_trav(&client->userTree, CountSessions);

  count = globalUserSessionCount;
  rc=DosReleaseMutexSem(acctSem);
	return count;
}


/*************************************************************************
 *
 *	Function: send_acct_reply
 *
 *	Purpose: Reply to the request with an ACKNOWLEDGE.  Also attach
 *		 reply attribute value pairs and any user message provided.
 *
 *************************************************************************/

void send_acct_reply(AUTH_REQ	*authreq,
                     VALUE_PAIR	*reply,
                     char		*msg,
                     int activefd)
{
	AUTH_HDR		*auth;
	u_short			total_length;
	struct	sockaddr_in	saremote;
	struct	sockaddr_in	*sin;
	char			*ptr;
	int			len;
	UINT4			lvalue;
	char			digest[16];
	int			secretlen;
	char			*ip_hostname();

	auth = (AUTH_HDR *)send_buffer;

	/* Build standard header */
	auth->code = PW_ACCOUNTING_RESPONSE;
	auth->id = authreq->id;
	memcpy(auth->vector, authreq->vector, AUTH_VECTOR_LEN);

	DEBUG("Sending Accounting Ack of id %d to %lx (%s)\n",
		authreq->id, authreq->ipaddr, ip_hostname(authreq->ipaddr));

	total_length = AUTH_HDR_LEN;

	/* Load up the configuration values for the user */
	ptr = auth->data;
	while(reply != (VALUE_PAIR *)NULL) {
		debug_pair(stdout, reply);
		*ptr++ = reply->attribute;

		switch(reply->type) {

		case PW_TYPE_STRING:
			len = strlen(reply->strvalue);
			*ptr++ = len + 2;
			strcpy(ptr, reply->strvalue);
			ptr += len;
			total_length += len + 2;
			break;
			
		case PW_TYPE_INTEGER:
		case PW_TYPE_IPADDR:
			*ptr++ = sizeof(UINT4) + 2;
			lvalue = htonl(reply->lvalue);
			memcpy(ptr, &lvalue, sizeof(UINT4));
			ptr += sizeof(UINT4);
			total_length += sizeof(UINT4) + 2;
			break;

		default:
			break;
		}

		reply = reply->next;
	}

	/* Append the user message */
	if(msg != (char *)NULL) {
		len = strlen(msg);
		if(len > 0 && len < AUTH_STRING_LEN) {
			*ptr++ = PW_PORT_MESSAGE;
			*ptr++ = len + 2;
			memcpy(ptr, msg, len);
			ptr += len;
			total_length += len + 2;
		}
	}

	auth->length = htons(total_length);

	/* Calculate the response digest */
	secretlen = strlen(authreq->secret);
	memcpy(send_buffer + total_length, authreq->secret, secretlen);
	md5_calc(digest, (char *)auth, total_length + secretlen);
	memcpy(auth->vector, digest, AUTH_VECTOR_LEN);
	memset(send_buffer + total_length, 0, secretlen);

	sin = (struct sockaddr_in *) &saremote;
        memset ((char *) sin, '\0', sizeof (saremote));
	sin->sin_family = AF_INET;
	sin->sin_addr.s_addr = htonl(authreq->ipaddr);
	sin->sin_port = htons(authreq->udp_port);

	/* Send it to the user */
	sendto(activefd, (char *)auth, (int)total_length, (int)0,
			(struct sockaddr *) sin, sizeof(struct sockaddr_in));
}


void GetSem(char * semName, HMTX * semHandle) {
  APIRET rc;
  rc=DosCreateMutexSem(semName, semHandle, 0, FALSE);
  if (rc==ERROR_DUPLICATE_NAME) {
    /* Already existed */
    rc=DosOpenMutexSem(semName, semHandle);
  }
  if (rc!=0) {
    fprintf (stderr, "%s(%s): Unable to get semaphore: %s\n",
      progname, "radacct", semName);
    exit(-1);
  }
}




void ThreadedCode AccountingThread(void * arg) {

	struct		sockaddr	salocal;
	struct		sockaddr	saremote;
	struct		sockaddr_in	*sin;
	struct		servent		*svp;
  u_short		lport;
	int			result;
	int			salen;
	AUTH_REQ	*authreq;
  int			acctfd;
  APIRET  rc;

  GetSem(ClientsSemName, &acctSem);

  GetSem(LogSemName, &logSem);

  rc = DosRequestMutexSem(acctSem, -1);
  if (!CreateSharedMemory(SharedMemName, HeapSize)) {
     fprintf (stderr, "%s: Unable to allocate shared memory: %s\n",
       progname, "radacct");
     exit(-1);
  }

  rootMarker=SHmalloc(sizeof(struct RootMarker));
  strcpy(rootMarker->eyeCatcher, EyeCatcher);
  rootMarker->client_list = 0;
  rootMarker->lastUpdated = 0;

  rc=DosReleaseMutexSem(acctSem);


	/*
	 * Open Accounting Socket.
	 */
  svp = getservbyname ("radacct", "udp");
  if (svp == (struct servent *) 0) {
     fprintf (stderr, "%s: No such service: %s/%s\n",
      progname, "radacct", "udp");
     exit(-1);
  }
  /* do not delete:  lport = htons(ntohs(lport) +1);  */
  lport = (u_short) svp->s_port;
  acctfd = socket (AF_INET, SOCK_DGRAM, 0);
  if (acctfd < 0) {
     psock_errno  ("acct socket");
     exit(-1);
  }

  sin = (struct sockaddr_in *) & salocal;
  memset ((char *) sin, '\0', sizeof (salocal));
  sin->sin_family = AF_INET;
  sin->sin_addr.s_addr = INADDR_ANY;
  sin->sin_port = lport;

  result = bind (acctfd, & salocal, sizeof (*sin));

  if (result < 0) {
     psock_errno  ("acct bind");
     exit(-1);
  }
  DEBUG("Radacct socket complete\n");

	sin = (struct sockaddr_in *) & saremote;
	for(;;) {
    DEBUG("Waiting for acctfd\n");
    salen = sizeof (saremote);
    result = recvfrom (acctfd, (char *) recv_buffer,
      (int) sizeof(recv_buffer),
      (int) 0, & saremote, & salen);

    if(result > 0) {
      if (show_names_flag) {
        printf("Acct: ");
      }

      authreq = radrecv(	ntohl(sin->sin_addr.s_addr),
                ntohs(sin->sin_port),
                recv_buffer, result);
      radrespond(authreq, acctfd);
    }
    else if(result < 0 && errno == EINTR) {
      result = 0;
    }
  }
}

