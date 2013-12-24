/* Shared memory structure for 'whoson' information */

#include "tree.h"

#define SharedMemName "\\SHAREMEM\\RADIUS\\CLIENTS.MEM"
/* Reserve 256 K for heap */
#define HeapSize 1024*256

#define EyeCatcher "<EYECATCHER>"
#define ClientsSemName "\\SEM32\\RADIUSD\\CLIENTS"

#define LogSemName "\\SEM32\\RADIUSD\\LOGS"

struct USERS {				/* user table data */
	char		*name;		/* pointer to user name */
  UINT4     userIPAddress; /* Client's IP Address */
  int       nasPort;  /* Client port number */
	time_t		began;		/* when user logged on */
  int       lastSessionID;  /* To detect STOP before START */
}	;

struct CLIENTS {			/* client usage data */
	UINT4		ipaddr;		/* IP address of client */
	char		*clientname;	/* name of client */
  int    clientCount;   /* Number of clients currently logged in */
	UINT2		sessionmarkwas;	/* last known session mark */
	struct tree_s	*userTree;		/* pointer to user data tree */
	struct CLIENTS	*next;		/* pointer to next client data */
}	;

struct RootMarker {
  char eyeCatcher[16];  /* == "<EYECATCHER>" */
  struct CLIENTS *client_list; /* pointer to client user chain */
  time_t lastUpdated;
};

