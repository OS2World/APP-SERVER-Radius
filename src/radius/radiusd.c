/*
 *
 *	RADIUS
 *	Remote Authentication Dial In User Service
 *
 *
 *	Livingston Enterprises, Inc.
 *	6920 Koll Center Parkway
 *	Pleasanton, CA   94566
 *
 *	Copyright 1992 Livingston Enterprises, Inc.
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
 
 /* Jan-5-99  mn Updated warning message logic in session count check */

/* don't look here for the version, run radiusd -v or look in version.c */
static char sccsid[] =
"@(#)radiusd.c	1.17 Copyright 1992 Livingston Enterprises Inc";

#ifndef OS2
  #define OS2
#endif

#define _INCLUDE_POSIX_SOURCE
#define BSD_SELECT

#include	<sys/types.h>
#include	<sys/socket.h>
#include	<sys/time.h>
/* #include	<sys/file.h> **** ?? */
#include	<direct.h>
#include	<stdio.h>
#include	<stdlib.h>
#include	<netinet/in.h>
#include	<utils.h>
#include	<sys/select.h>

#include	<netdb.h>
#include	<fcntl.h>
#include	<pwd.h>
#include	<time.h>
#include	<ctype.h>
/* #include	<unistd.h>  ***** ?? */
#include	<signal.h>
#include	<errno.h>
#include   <process.h>
/* #include	<sys/wait.h>  **** ?? */
#include	<io.h>

#define INCL_DOSERRORS
#define INCL_DOSPROCESS
#define INCL_DOSSEMAPHORES
#include <os2.h>

#include	"radius.h"
#include	"crypt.h"

static u_char   recv_buffer[4096];
static u_char   send_buffer[4096];
char    *progname;
int     debug_flag=0;
int     show_names_flag=0;
int     show_pwFail_flag=0;
char    *radius_dir;
char    *radacct_dir;
char    radius_dir_hold[256];
char    radacct_dir_hold[256];
static UINT4    expiration_seconds;
static UINT4    warning_seconds;

static int    tidAcct=0;                  /* Accounting thread ID    */
static HMTX   authSem;




#ifdef OS2
struct passwd *getpwnam(const char * pw) {
  return (NULL);
}
#endif

/*************************************************************************
 *
 *	Function: unix_pass
 *
 *	Purpose: Check the users password against the standard UNIX
 *		 password table.
 *
 *************************************************************************/
int unix_pass(char *name, char *passwd)
{
  struct  passwd  *pwd;
  struct  passwd  *getpwnam();
  char  *encpw;
  char  *crypt();
  char  *encrypted_pass;

#if !defined(NOSHADOW)
  #if defined(M_UNIX)
  struct passwd *spwd;
  #else
  struct passwd *spwd;
  #endif
#endif /* !NOSHADOW */

  encpw = NULL;

  /* Get encrypted password from password file */
  if ((pwd = getpwnam(name)) == NULL) {
    return (-1);
  }

  encrypted_pass = pwd->pw_passwd;

#if !defined(NOSHADOW)
  if (strcmp(pwd->pw_passwd, "x") == 0) {
/*		if((spwd = getspnam(name)) == NULL) {
      return(-1);
    }
    */
  #if defined(M_UNIX)
    encrypted_pass = spwd->pw_passwd;
  #else
    encrypted_pass = spwd->pw_passwd;
  #endif	/* M_UNIX */
  }
#endif	/* !NOSHADOW */

  /* Run encryption algorythm */
  encpw = crypt(passwd, encrypted_pass);

  /* Check it */
  if (strcmp(encpw, encrypted_pass)) {
    return (-1);
  }
  return (0);
}


/*************************************************************************
 *
 *	Function: send_reject
 *
 *	Purpose: Reply to the request with a REJECT.  Also attach
 *		 any user message provided.
 *
 *************************************************************************/
void send_reject(AUTH_REQ *authreq, char *msg, int activefd)
{
  AUTH_HDR  *auth;
  struct    sockaddr  saremote;
  struct    sockaddr_in *sin;
  char    *ip_hostname();
  char    digest[AUTH_VECTOR_LEN];
  int     secretlen;
  int     total_length;
  u_char    *ptr;
  int     len;

  auth = (AUTH_HDR *)send_buffer;

  /* Build standard response header */
  if (authreq->code == PW_PASSWORD_REQUEST) {
    auth->code = PW_PASSWORD_REJECT;
  } else {
    auth->code = PW_AUTHENTICATION_REJECT;
  }
  auth->id = authreq->id;
  memcpy(auth->vector, authreq->vector, AUTH_VECTOR_LEN);
  total_length = AUTH_HDR_LEN;

  /* Append the user message */
  if (msg != (char *)NULL) {
    len = strlen(msg);
    if (len > 0 && len < AUTH_STRING_LEN) {
      ptr = auth->data;
      *ptr++ = PW_PORT_MESSAGE;
      *ptr++ = len + 2;
      memcpy(ptr, msg, len);
      ptr += len;
      total_length += len + 2;
    }
  }

  /* Set total length in the header */
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

  DEBUG("Sending Reject of id %d to %lx (%s)\n",
        authreq->id, (u_long)authreq->ipaddr,
        ip_hostname(authreq->ipaddr));

  /* Send it to the user */
  sendto(activefd, (char *)auth, (int)total_length, (int)0,
         (struct sockaddr *) &saremote, sizeof(struct sockaddr_in));
}

/*************************************************************************
 *
 *	Function: send_challenge
 *
 *	Purpose: Reply to the request with a CHALLENGE.  Also attach
 *		 any user message provided and a state value.
 *
 *************************************************************************/
void send_challenge(AUTH_REQ  *authreq, char *msg, char *state, int activefd)
{
  AUTH_HDR  *auth;
  struct    sockaddr_in saremote;
  struct    sockaddr_in *sin;
  char    *ip_hostname();
  char    digest[AUTH_VECTOR_LEN];
  int     secretlen;
  int     total_length;
  u_char    *ptr;
  int     len;

  auth = (AUTH_HDR *)send_buffer;

  /* Build standard response header */
  auth->code = PW_ACCESS_CHALLENGE;
  auth->id = authreq->id;
  memcpy(auth->vector, authreq->vector, AUTH_VECTOR_LEN);
  total_length = AUTH_HDR_LEN;

  /* Append the user message */
  if (msg != (char *)NULL) {
    len = strlen(msg);
    if (len > 0 && len < AUTH_STRING_LEN) {
      ptr = auth->data;
      *ptr++ = PW_PORT_MESSAGE;
      *ptr++ = len + 2;
      memcpy(ptr, msg, len);
      ptr += len;
      total_length += len + 2;
    }
  }

  /* Append the state info */
  if ((state != (char *)NULL) && (strlen(state) > 0)) {
    len = strlen(state);
    *ptr++ = PW_STATE;
    *ptr++ = len + 2;
    memcpy(ptr, state, len);
    ptr += len;
    total_length += len + 2;
  }

  /* Set total length in the header */
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

  DEBUG("Sending Challenge of id %d to %lx (%s)\n",
        authreq->id, (u_long)authreq->ipaddr,
        ip_hostname(authreq->ipaddr));

  /* Send it to the user */
  sendto(activefd, (char *)auth, (int)total_length, (int)0,
         (struct sockaddr *) &saremote, sizeof(struct sockaddr_in));
}

/*************************************************************************
 *
 *	Function: send_pwack
 *
 *	Purpose: Reply to the request with an ACKNOWLEDGE.
 *		 User password has been successfully changed.
 *
 *************************************************************************/
void send_pwack(AUTH_REQ  *authreq, int activefd)
{
  AUTH_HDR  *auth;
  struct    sockaddr  saremote;
  struct    sockaddr_in *sin;
  char    *ip_hostname();
  char    digest[AUTH_VECTOR_LEN];
  int     secretlen;

  auth = (AUTH_HDR *)send_buffer;

  /* Build standard response header */
  auth->code = PW_PASSWORD_ACK;
  auth->id = authreq->id;
  memcpy(auth->vector, authreq->vector, AUTH_VECTOR_LEN);
  auth->length = htons(AUTH_HDR_LEN);

  /* Calculate the response digest */
  secretlen = strlen(authreq->secret);
  DEBUG("PWACK Secret :%s :%d\n",authreq->secret,secretlen);
  memcpy(send_buffer + AUTH_HDR_LEN, authreq->secret, secretlen);
  md5_calc(digest, (char *)auth, AUTH_HDR_LEN + secretlen);
  memcpy(auth->vector, digest, AUTH_VECTOR_LEN);
  memset(send_buffer + AUTH_HDR_LEN, 0, secretlen);

  sin = (struct sockaddr_in *) &saremote;
  memset ((char *) sin, '\0', sizeof (saremote));
  sin->sin_family = AF_INET;
  sin->sin_addr.s_addr = htonl(authreq->ipaddr);
  sin->sin_port = htons(authreq->udp_port);

  DEBUG("Sending PW Ack of id %d to %lx (%s)\n",
        authreq->id, (u_long)authreq->ipaddr,
        ip_hostname(authreq->ipaddr));

  /* Send it to the user */
  sendto(activefd, (char *)auth, (int)AUTH_HDR_LEN, (int)0,
         &saremote, sizeof(struct sockaddr_in));
}

/*************************************************************************
 *
 *	Function: send_accept
 *
 *	Purpose: Reply to the request with an ACKNOWLEDGE.  Also attach
 *		 reply attribute value pairs and any user message provided.
 *
 *************************************************************************/
void send_accept(AUTH_REQ *authreq, VALUE_PAIR  *reply, char * msg, int activefd)
{
  AUTH_HDR  *auth;
  u_short   total_length;
  struct    sockaddr  saremote;
  struct    sockaddr_in *sin;
  u_char    *ptr;
  int     len;
  UINT4   lvalue;
  u_char    digest[16];
  int     secretlen;
  char    *ip_hostname();

  auth = (AUTH_HDR *)send_buffer;

  /* Build standard header */
  auth->code = PW_AUTHENTICATION_ACK;
  auth->id = authreq->id;
  memcpy(auth->vector, authreq->vector, AUTH_VECTOR_LEN);

  DEBUG("Sending Ack of id %d to %lx (%s)\n",
        authreq->id, (u_long)authreq->ipaddr,
        ip_hostname(authreq->ipaddr));

  total_length = AUTH_HDR_LEN;

  /* Load up the configuration values for the user */
  ptr = auth->data;
  while (reply != (VALUE_PAIR *)NULL) {
    debug_pair(stdout, reply);
    *ptr++ = reply->attribute;

    switch (reply->type) {
    
    case PW_TYPE_STRING:
      len = strlen(reply->strvalue);
      if (len >= AUTH_STRING_LEN) {
        len = AUTH_STRING_LEN - 1;
      }
      *ptr++ = len + 2;
      memcpy(ptr, reply->strvalue,len);
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
  if (msg != (char *)NULL) {
    len = strlen(msg);
    if (len > 0 && len < AUTH_STRING_LEN) {
      *ptr++ = PW_PORT_MESSAGE;
      *ptr++ = len + 2;
      memcpy(ptr, msg, len);
      ptr += len;
      total_length += len + 2;
    }
  }

  auth->length = htons(total_length);

  /* Append secret and calculate the response digest */
  secretlen = strlen(authreq->secret);
  DEBUG("ACCPT Secret :%s :%d\n",authreq->secret,secretlen);
  memcpy(send_buffer + total_length, authreq->secret, secretlen);
  md5_calc(digest, (char *)auth, total_length + secretlen);
  memcpy(auth->vector, digest, AUTH_VECTOR_LEN);
  memset(send_buffer + total_length, 0, secretlen);

  sin = (struct sockaddr_in *) &saremote;
  memset ((char *) sin, '\0', sizeof (saremote));
  sin->sin_family = AF_INET;
  sin->sin_addr.s_addr = htonl(authreq->ipaddr);
  sin->sin_port = htons(authreq->udp_port);
  /*DEBUG("sin_port = %d\n", sin->sin_port); */
  /*(DEBUG("s_addr   = %ld\n",sin->sin_addr.s_addr); */
  /* Send it to the user */
  sendto(activefd, (char *)auth, (int)total_length, (int)0,
         &saremote, sizeof(struct sockaddr_in));
}


/*************************************************************************
 *
 *	Function: set_expiration
 *
 *	Purpose: Set the new expiration time by updating or adding
     the Expiration attribute-value pair.
 *
 *************************************************************************/
int set_expiration(VALUE_PAIR *user_check,
                   UINT4 expiration)
{
  VALUE_PAIR  *exppair;
  VALUE_PAIR  *prev;
  struct    timeval tp;
  struct    timezone  tzp;

  if (user_check == (VALUE_PAIR *)NULL) {
    return (-1);
  }

  /* Look for an existing expiration entry */
  exppair = user_check;
  prev = (VALUE_PAIR *)NULL;
  while (exppair != (VALUE_PAIR *)NULL) {
    if (exppair->attribute == PW_EXPIRATION) {
      break;
    }
    prev = exppair;
    exppair = exppair->next;
  }
  if (exppair == (VALUE_PAIR *)NULL) {
    /* Add a new attr-value pair */
    if ((exppair = (VALUE_PAIR *)malloc(sizeof(VALUE_PAIR))) ==
        (VALUE_PAIR *)NULL) {
      fprintf(stderr, "%s: no memory\n", progname);
      exit(-1);
    }
    /* Initialize it */
    strcpy(exppair->name, "Expiration");
    exppair->attribute = PW_EXPIRATION;
    exppair->type = PW_TYPE_DATE;
    *exppair->strvalue = '\0';
    exppair->lvalue = (UINT4)0;
    exppair->next = (VALUE_PAIR *)NULL;

    /* Attach it to the list. */
    prev->next = exppair;
  }

  /* calculate a new expiration */
  gettimeofday(&tp, &tzp);
  exppair->lvalue = tp.tv_sec + expiration;
  return (0);
}


/*************************************************************************
 *
 *	Function: rad_passchange
 *
 *	Purpose: Change a users password
 *
 *************************************************************************/

void rad_passchange(AUTH_REQ *authreq, int activefd)
{
  VALUE_PAIR  *namepair;
  VALUE_PAIR  *check_item;
  VALUE_PAIR  *newpasspair;
  VALUE_PAIR  *oldpasspair;
  VALUE_PAIR  *curpass;
  VALUE_PAIR  *user_check;
  VALUE_PAIR  *user_reply;
  char    pw_digest[16];
  char    string[64];
  char    passbuf[AUTH_PASS_LEN];
  int     i;
  int     secretlen;
  char    msg[128];
  char    *ip_hostname();
  int     user_find_result=0;
  APIRET  rc;

  /* Get the username */
  namepair = authreq->request;
  while (namepair != (VALUE_PAIR *)NULL) {
    if (namepair->attribute == PW_USER_NAME) {
      break;
    }
    namepair = namepair->next;
  }
  if (namepair == (VALUE_PAIR *)NULL) {
    sprintf(msg,
            "Passchange: from %s - No User name supplied\n",
            ip_hostname(authreq->ipaddr));
    log_err(msg);
    pairfree(authreq->request);
    memset(authreq, 0, sizeof(AUTH_REQ));
    free(authreq);
    return;
  }

  /*
   * Look the user up in the database
     wait a MAX of 2 seconds for user file semaphre.  If we don't get it, go
     ahead and try anyway since we don't want to block forever.
   */
  rc = DosRequestMutexSem(authSem, 2000);
  user_find_result = user_find(namepair->strvalue, &user_check, &user_reply);
  rc=DosReleaseMutexSem(authSem);
  if (user_find_result != 0) {
    sprintf(msg,
            "Passchange: from %s - Invalid User: %s\n",
            ip_hostname(authreq->ipaddr), namepair->strvalue);
    log_err(msg);
    send_reject(authreq, (char *)NULL, activefd);
    pairfree(authreq->request);
    memset(authreq, 0, sizeof(AUTH_REQ));
    free(authreq);
    return;
  }

  /*
   * Validate the user -
   *
   * We have to unwrap this in a special way to decrypt the
   * old and new passwords.  The MD5 calculation is based
   * on the old password.  The vector is different.  The old
   * password is encrypted using the encrypted new password
   * as its vector.  The new password is encrypted using the
   * random encryption vector in the request header.
   */

  /* Extract the attr-value pairs for the old and new passwords */
  check_item = authreq->request;
  while (check_item != (VALUE_PAIR *)NULL) {
    if (check_item->attribute == PW_PASSWORD) {
      newpasspair = check_item;
    } else if (check_item->attribute == PW_OLD_PASSWORD) {
      oldpasspair = check_item;
    }
    check_item = check_item->next;
  }

  /* Verify that both encrypted passwords were supplied */
  if (newpasspair == (VALUE_PAIR *)NULL ||
      oldpasspair == (VALUE_PAIR *)NULL) {
    /* Missing one of the passwords */
    sprintf(msg,
            "Passchange: from %s - Missing Password: %s\n",
            ip_hostname(authreq->ipaddr), namepair->strvalue);
    log_err(msg);
    send_reject(authreq, (char *)NULL, activefd);
    pairfree(authreq->request);
    pairfree(user_check);
    pairfree(user_reply);
    memset(authreq, 0, sizeof(AUTH_REQ));
    free(authreq);
    return;
  }

  /* Get the current password from the database */
  curpass = user_check;
  while (curpass != (VALUE_PAIR *)NULL) {
    if (curpass->attribute == PW_PASSWORD) {
      break;
    }
    curpass = curpass->next;
  }
  if ((curpass == (VALUE_PAIR *)NULL) || curpass->strvalue == (char *)NULL) {
    /* Missing our local copy of the password */
    sprintf(msg,
            "Passchange: from %s - Missing Local Password: %s\n",
            ip_hostname(authreq->ipaddr), namepair->strvalue);
    log_err(msg);
    send_reject(authreq, (char *)NULL, activefd);
    pairfree(authreq->request);
    pairfree(user_check);
    pairfree(user_reply);
    memset(authreq, 0, sizeof(AUTH_REQ));
    free(authreq);
    return;
  }
  if (strcmp(curpass->strvalue,"UNIX") == 0) {
    /* Can't change passwords that aren't in users file */
    sprintf(msg,
            "Passchange: from %s - system password change not allowed: %s\n",
            ip_hostname(authreq->ipaddr), namepair->strvalue);
    log_err(msg);
    send_reject(authreq, (char *)NULL, activefd);
    pairfree(authreq->request);
    pairfree(user_check);
    pairfree(user_reply);
    memset(authreq, 0, sizeof(AUTH_REQ));
    free(authreq);
    return;
  }

  /* Decrypt the old password */
  secretlen = strlen(curpass->strvalue);
  memcpy(string, curpass->strvalue, secretlen);
  memcpy(string + secretlen, newpasspair->strvalue, AUTH_VECTOR_LEN);
  md5_calc(pw_digest, string, AUTH_VECTOR_LEN + secretlen);
  memcpy(passbuf, oldpasspair->strvalue, AUTH_PASS_LEN);
  for (i = 0;i < AUTH_PASS_LEN;i++) {
    passbuf[i] ^= pw_digest[i];
  }

  /* Did they supply the correct password ??? */
  if (strncmp(passbuf, curpass->strvalue, AUTH_PASS_LEN) != 0) {
    sprintf(msg,
            "Passchange: from %s - Incorrect Password: %s\n",
            ip_hostname(authreq->ipaddr), namepair->strvalue);
    log_err(msg);
    send_reject(authreq, (char *)NULL, activefd);
    pairfree(authreq->request);
    pairfree(user_check);
    pairfree(user_reply);
    memset(authreq, 0, sizeof(AUTH_REQ));
    free(authreq);
    return;
  }

  /* Decrypt the new password */
  memcpy(string, curpass->strvalue, secretlen);
  memcpy(string + secretlen, authreq->vector, AUTH_VECTOR_LEN);
  md5_calc(pw_digest, string, AUTH_VECTOR_LEN + secretlen);
  memcpy(passbuf, newpasspair->strvalue, AUTH_PASS_LEN);
  for (i = 0;i < AUTH_PASS_LEN;i++) {
    passbuf[i] ^= pw_digest[i];
  }

  /* Update the users password */
  strncpy(curpass->strvalue, passbuf, AUTH_PASS_LEN);

  /* Add a new expiration date if we are aging passwords */
  if (expiration_seconds != (UINT4)0) {
    set_expiration(user_check, expiration_seconds);
  }

  /* Update the database */
  if (user_update(namepair->strvalue, user_check, user_reply) != 0) {
    send_reject(authreq, (char *)NULL, activefd);
    sprintf(msg,
            "Passchange: unable to update password for %s\n",
            namepair->strvalue);
    log_err(msg);

  } else {
    send_pwack(authreq, activefd);
  }
  pairfree(authreq->request);
  pairfree(user_check);
  pairfree(user_reply);
  memset(authreq, 0, sizeof(AUTH_REQ));
  free(authreq);
  return;
}

/*************************************************************************
 *
 *	Function: pw_expired
 *
 *	Purpose: Tests to see if the users password has expired.
 *
 *	Return: Number of days before expiration if a warning is required
 *		otherwise 0 for success and -1 for failure.
 *
 *************************************************************************/
int pw_expired(UINT4 exptime)
{
  struct  timeval tp;
  struct  timezone  tzp;
  UINT4 exp_remain;
  int   exp_remain_int;

  if (expiration_seconds == (UINT4)0) {
    return (0);
  }

  gettimeofday(&tp, &tzp);
  if (tp.tv_sec > exptime) {
    return (-1);
  }
  if (warning_seconds != (UINT4)0) {
    if (tp.tv_sec > exptime - warning_seconds) {
      exp_remain = exptime - tp.tv_sec;
      exp_remain /= (UINT4)SECONDS_PER_DAY;
      exp_remain_int = exp_remain;
      return (exp_remain_int);
    }
  }
  return (0);
}

void sig_fatal(int sig)
{
  fprintf(stderr, "%s: exit on signal (%d)\n", progname, sig);
  fflush(stderr);
  exit(1);
}


/*************************************************************************
 *
 *	Function: rad_authenticate
 *
 *	Purpose: Process and reply to an authentication request
 *
 *************************************************************************/
void rad_authenticate(AUTH_REQ *authreq, int activefd)
{
  VALUE_PAIR  *namepair;
  VALUE_PAIR  *check_item;
  VALUE_PAIR  *auth_item;
  VALUE_PAIR  *user_check;
  VALUE_PAIR  *user_reply;
  int     result;
  char    pw_digest[16];
  char    string[128];
  int     i;
  char    msg[128];
  char    umsg[128];
  char    *user_msg;
  char    *userName=NULL;
  char    *ip_hostname();
  int     retval;
  int     activeSessions;
  char    *ptr;
  int     user_find_result=0;
  int     pwLength;
  APIRET  rc;

  /* Get the username from the request */
  namepair = authreq->request;
  while (namepair != (VALUE_PAIR *)NULL) {
    if (namepair->attribute == PW_USER_NAME) {
      break;
    }
    namepair = namepair->next;
  }
  if ((namepair == (VALUE_PAIR *)NULL) ||
      (strlen(namepair->strvalue) == 0)) {
    sprintf(msg,  "Authenticate: from %s - No User Name\n",
            ip_hostname(authreq->ipaddr));
    log_err(msg);
    pairfree(authreq->request);
    memset(authreq, 0, sizeof(AUTH_REQ));
    free(authreq);
    return;
  }

  /* Verify the client and Calculate the MD5 Password Digest */
  if (calc_digest(pw_digest, authreq) != 0) {
    DEBUG("failed password !!\n");
    /* We dont respond when this fails */
    sprintf(msg, "Authenticate: from %s - Security Breach: %s\n",
            ip_hostname(authreq->ipaddr), namepair->strvalue);
    log_err(msg);
    pairfree(authreq->request);
    memset(authreq, 0, sizeof(AUTH_REQ));
    free(authreq);
    return;
  }

  /* Get the user from the database */
  rc = DosRequestMutexSem(authSem, 2000);
  user_find_result = user_find(namepair->strvalue, &user_check, &user_reply);
  rc=DosReleaseMutexSem(authSem);
  userName=namepair->strvalue;
  if (user_find_result != 0) {
    DEBUG("Authenticate: from %s - Invalid User: %s\n",
          ip_hostname(authreq->ipaddr), namepair->strvalue);
    sprintf(msg, "Authenticate: from %s - Invalid User: %s\n",
            ip_hostname(authreq->ipaddr), namepair->strvalue);
    log_err(msg);
    send_reject(authreq, (char *)NULL, activefd);
    pairfree(authreq->request);
    memset(authreq, 0, sizeof(AUTH_REQ));
    free(authreq);
    DEBUG("failed authentication\n");
    return;
  }

  /* Validate the user */
  DEBUG("we ought to be able to validate from here\n");
  /* Look for matching check items */
  result = 0;
  user_msg = (char *)NULL;
  check_item = user_check;
  while (result == 0 && check_item != (VALUE_PAIR *)NULL) {
    DEBUG("inside the while loop\n");
    /*
     * Check expiration date if we are doing password aging.
     */
    if (check_item->attribute == PW_EXPIRATION) {
      /* Has this user's password expired */
      retval = pw_expired(check_item->lvalue);
      if (retval < 0) {
        result = -1;
        user_msg = "Password Has Expired\r\n";
        DEBUG("Password Has Expired\n");
      } else {
        if (retval > 0) {
          sprintf(umsg, "Password Will Expire in %d Days\r\n", retval);
          user_msg = umsg;
        }
        check_item = check_item->next;
      }
      continue;
    }

    /*
     * See if the user is exceeding his/her max number of sessions.
     */
    if (check_item->attribute == PW_SESSIONS) {
      if (check_item->lvalue) {
        activeSessions = user_sessions(namepair->strvalue);
        DEBUG("Subscriber %s found in %d other sessions (out of %d max).\n",
              namepair->strvalue, activeSessions, check_item->lvalue);
        if ((activeSessions > 0) && ((activeSessions+1) >= check_item->lvalue) ) {
          sprintf(umsg,
                  "Subscriber %s in %d other sessions.\r\n",
                  namepair->strvalue, activeSessions, check_item->lvalue);
          log_err(umsg);
        }
        if (check_item->lvalue <= activeSessions) {
          sprintf(umsg,
                  "Too many sessions, only %d allowed\r\n",
                  check_item->lvalue);
          user_msg = umsg;
          result = -1;
        }
      }
      check_item = check_item->next;
      continue;
    }

    /*
     * Look for the matching attribute in the request.
     */
    auth_item = authreq->request;
    while (auth_item != (VALUE_PAIR *)NULL) {
      if (check_item->attribute == auth_item->attribute) {
        break;
      }
      if (check_item->attribute == PW_PASSWORD &&
          auth_item->attribute == PW_CHAP_PASSWORD) {
        break;
      }

      auth_item = auth_item->next;
    }
    if (auth_item == (VALUE_PAIR *)NULL) {
      DEBUG("At end of list.\n");
      result = -1;
      continue;
    }

    /*
     * Special handling for passwords which are encrypted,
     * and sometimes authenticated against the UNIX passwd database.
     * Also they can come using the Three-Way CHAP.
     *
     */
    if (check_item->attribute == PW_PASSWORD) {
      if (auth_item->attribute == PW_CHAP_PASSWORD) {
        /* Use MD5 to verify */
        ptr = string;
        *ptr++ = *auth_item->strvalue;
        strcpy(ptr, check_item->strvalue);
        pwLength=strlen(check_item->strvalue);
        ptr += pwLength;
        memcpy(ptr, authreq->vector, AUTH_VECTOR_LEN);
        md5_calc(pw_digest, string, 1 + CHAP_VALUE_LENGTH + pwLength);
        /* Compare them */
        if (memcmp(pw_digest, auth_item->strvalue + 1, CHAP_VALUE_LENGTH) != 0) {
          /* Try CAPS password and compare again */
          for (i=0; i<pwLength; i++) {
            string[i+1]=toupper(string[i+1]);
          }
          md5_calc(pw_digest, string, 1 + CHAP_VALUE_LENGTH + pwLength);
          /* Compare them */
          if (memcmp(pw_digest, auth_item->strvalue + 1, CHAP_VALUE_LENGTH) != 0) {
            sprintf(msg, "Wrong CHAP password by %s\n", userName);
            log_err(msg);
            DEBUG("CHAP Password mismatch\n");
            result = -1;
          }
        }
      } else {
        /* Decrypt the password */
        memcpy(string, auth_item->strvalue, AUTH_PASS_LEN);
        for (i = 0;i < AUTH_PASS_LEN;i++) {
          string[i] ^= pw_digest[i];
        }
        string[AUTH_PASS_LEN] = '\0';
        /* Test Code for Challenge */
        if (strcmp(string, "challenge") == 0) {
          send_challenge(authreq,
                         "You want me to challenge you??\r\nOkay I will",
                         "1",activefd);
          pairfree(authreq->request);
          memset(authreq, 0, sizeof(AUTH_REQ));
          free(authreq);
          return;
        }
        if (strcmp(check_item->strvalue, "UNIX") == 0) {
          if (unix_pass(namepair->strvalue, string) != 0) {
            result = -1;
            user_msg = "DEFAULT UNIX user not in unix database";
          }
        } else if (stricmp(check_item->strvalue, string) != 0) {
          if (show_pwFail_flag) {
            sprintf(msg, "Wrong password by %s: (%s)\n", userName, string);
          } else {
            sprintf(msg, "Wrong password by %s\n", userName);
          }
          DEBUG("failed password check\n");
          log_err(msg);
          result = -1;
          user_msg = (char *)NULL;
        }
      }
    } else {
      switch (check_item->type) {
      
      case PW_TYPE_STRING:
        if (strcmp(check_item->strvalue, auth_item->strvalue) != 0) {
          DEBUG("Plaintext password mismatch.\n");
          result = -1;
        }
        break;

      case PW_TYPE_INTEGER:
      case PW_TYPE_IPADDR:
        if (check_item->lvalue != auth_item->lvalue) {
          DEBUG("Integer password mismatch.\n");
          result = -1;
        }
        break;

      default:
        DEBUG("Unknown password item type.\n");
        result = -1;
        break;
      }
    }
    check_item = check_item->next;
  }
  DEBUG("outside the while loop, result == %d\n", result);
  if (result != 0) {
    send_reject(authreq, user_msg, activefd);
    if (user_msg != NULL) {
      DEBUG("failed : %s\n", user_msg);
      sprintf(msg, "Login failed (%s): %s\n", userName, user_msg);
      log_err(msg);
    }
  } else {
    send_accept(authreq, user_reply, user_msg, activefd);
    DEBUG("Succeeded : \b%s\nSucceeded : %s\n", user_reply->name, user_msg);
  }
  pairfree(authreq->request);
  memset(authreq, 0, sizeof(AUTH_REQ));
  free(authreq);
  pairfree(user_check);
  pairfree(user_reply);
  return;
}


/*************************************************************************
 *
 *	Function: calc_digest
 *
 *	Purpose: Validates the requesting client NAS.  Calculates the
 *		 digest to be used for decrypting the users password
 *		 based on the clients private key.
 *
 *************************************************************************/
int calc_digest(u_char *digest, AUTH_REQ *authreq)
{
  FILE  *clientfd;
  u_char  buffer[128];
  u_char  secret[64];
  char  hostnm[256];
  char  msg[128];
  char  *ip_hostname();
  int   secretlen;
  UINT4 ipaddr;
  UINT4 get_ipaddr();

  /* Find the client in the database */
  sprintf(buffer, "%s/%s", radius_dir, RADIUS_CLIENTS);

  if ((clientfd = fopen(buffer, "r")) == (FILE *)NULL) {
    fprintf(stderr, "%s: couldn't open %s to find clients\n", progname, buffer);
    return (-1);
  }
  ipaddr = (UINT4)0;
  while (fgets(buffer, sizeof(buffer), clientfd) != (char *)NULL) {
    if (*buffer == '#') {
      continue;
    }
    if (sscanf(buffer, "%s%s", hostnm, secret) != 2) {
      continue;
    }
    DEBUG("Hostname: %s\n",hostnm);
    ipaddr = get_ipaddr(hostnm);
    DEBUG("IP ADDR: %s\n",ip_hostname(ipaddr));
    if (ipaddr == authreq->ipaddr) {
      break;
    }
  }
  fclose(clientfd);
  memset(buffer, 0, sizeof(buffer));

  /*
   * Validate the requesting IP address -
   * Not secure, but worth the check for accidental requests
   */
  if (ipaddr != authreq->ipaddr) {
    strcpy(hostnm,ip_hostname(ipaddr));
    sprintf(msg, "requester address mismatch: %s != %s\n",
            hostnm,
            ip_hostname(authreq->ipaddr));
    log_err(msg);
    memset(secret, 0, sizeof(secret));
    return (-1);
  }

  /* Use the secret to setup the decryption digest */
  secretlen = strlen(secret);
  strcpy(buffer, secret);
  memcpy(buffer + secretlen, authreq->vector, AUTH_VECTOR_LEN);
  md5_calc(digest, buffer, secretlen + AUTH_VECTOR_LEN);
  strcpy(authreq->secret, secret);
  memset(buffer, 0, sizeof(buffer));
  memset(secret, 0, sizeof(secret));
  return (0);
}

/*************************************************************************
 *
 *	Function: debug_pair
 *
 *	Purpose: Print the Attribute-value pair to the desired File.
 *
 *************************************************************************/
void debug_pair(FILE *fd, VALUE_PAIR *pair)
{
  if (debug_flag || (show_names_flag && 
                     ( (pair->attribute == PW_USER_NAME) )) ) {
    fputs("    ", fd);
    fprint_attr_val(fd, pair);
    fputs("\n", fd);
  }
}

/*************************************************************************
 *
 *	Function: usage
 *
 *	Purpose: Display the syntax for starting this program.
 *
 *************************************************************************/
void usage(void)
{
  fprintf(stderr, "Usage: %s [ -a acct_dir ] [ -s ] [ -x ] [ -d db_dir ]\n",progname);
  exit(-1);
}

/*************************************************************************
 *
 *	Function: log_err
 *
 *	Purpose: Log the error message provided to the error log with
     a time stamp.
 *
 *************************************************************************/
int log_err(char *msg)
{
  FILE  *msgfd;
  char  buffer[128];
  time_t  timeval;

  sprintf(buffer, "%s/%s", radius_dir, RADIUS_LOG);

  if ((msgfd = fopen(buffer, "a")) == (FILE *)NULL) {
    fprintf(stderr, "%s:Couldn't open %s for logging\n", progname, buffer);
    return (-1);
  }
  timeval = time(0);
  fprintf(msgfd, "%-24.24s: %s", ctime(&timeval), msg);
  fclose(msgfd);
  return (0);
}

/*************************************************************************
 *
 *	Function: config_init
 *
 *	Purpose: intializes configuration values:
 *
 *		 expiration_seconds - When updating a user password,
 *			the amount of time to add to the current time
 *			to set the time when the password will expire.
 *			This is stored as the VALUE Password-Expiration
 *			in the dictionary as number of days.
 *
 *		warning_seconds - When acknowledging a user authentication
 *			time remaining for valid password to notify user
 *			of password expiration.
 *
 *************************************************************************/
int config_init(void)
{
  DICT_VALUE  *dval;
  DICT_VALUE  *dict_valfind();

  if ((dval = dict_valfind("Password-Expiration")) == (DICT_VALUE *)NULL) {
    expiration_seconds = (UINT4)0;
  } else {
    expiration_seconds = dval->value * (UINT4)SECONDS_PER_DAY;
  }
  if ((dval = dict_valfind("Password-Warning")) == (DICT_VALUE *)NULL) {
    warning_seconds = (UINT4)0;
  } else {
    warning_seconds = dval->value * (UINT4)SECONDS_PER_DAY;
  }
  return (0);
}

/*************************************************************************
 *
 *	Function: radrespond
 *
 *	Purpose: Respond to supported requests:
 *
 *		 PW_AUTHENTICATION_REQUEST - Authentication request from
 *				a client network access server.
 *
 *		 PW_ACCOUNTING_REQUEST - Accounting request from
 *				a client network access server.
 *
 *		 PW_PASSWORD_REQUEST - User request to change a password.
 *
 *************************************************************************/
int radrespond(AUTH_REQ *authreq, int activefd)
{
  switch (authreq->code) {
  
  case PW_AUTHENTICATION_REQUEST:
    rad_authenticate(authreq, activefd);
    break;

  case PW_ACCOUNTING_REQUEST:
    rad_accounting(authreq, activefd);
    break;

  case PW_PASSWORD_REQUEST:
    rad_passchange(authreq, activefd);
    break;

  default:
    break;
  }
  return (0);
}


/*************************************************************************
 *
 *	Function: radrecv
 *
 *	Purpose: Receive UDP client requests, build an authorization request
 *		 structure, and attach attribute-value pairs contained in
 *		 the request to the new structure.
 *
 *************************************************************************/

AUTH_REQ  *radrecv(UINT4 host,
                   u_short udp_port,
                   u_char *buffer,
                   int length)
{
  u_char    *ptr;
  AUTH_HDR  *auth;
  int     totallen;
  int     attribute;
  int     attrlen;
  DICT_ATTR *attr;
  DICT_ATTR *dict_attrget();
  UINT4   lvalue;
  char    *ip_hostname();
  VALUE_PAIR  *first_pair;
  VALUE_PAIR  *prev;
  VALUE_PAIR  *pair;
  AUTH_REQ  *authreq;

  /*
   * Pre-allocate the new request data structure
   */

  if ((authreq = (AUTH_REQ *)malloc(sizeof(AUTH_REQ))) == (AUTH_REQ *)NULL) {
    fprintf(stderr, "%s: no memory\n", progname);
    exit(-1);
  }

  auth = (AUTH_HDR *)buffer;
  totallen = ntohs(auth->length);

  DEBUG("radrecv: Request from host %lx code=%d, id=%d, length=%d\n",
        (u_long)host, auth->code, auth->id, totallen);

  /*
   * Fill header fields
   */
  authreq->ipaddr = host;
  authreq->udp_port = udp_port;
  authreq->id = auth->id;
  authreq->code = auth->code;
  memcpy(authreq->vector, auth->vector, AUTH_VECTOR_LEN);

  /*
   * Extract attribute-value pairs
   */
  ptr = auth->data;
  length -= AUTH_HDR_LEN;
  first_pair = (VALUE_PAIR *)NULL;
  prev = (VALUE_PAIR *)NULL;

  while (length > 0) {

    attribute = *ptr++;
    attrlen = *ptr++;
    if (attrlen < 2) {
      length = 0;
      continue;
    }
    attrlen -= 2;
    if ((attr = dict_attrget(attribute)) == (DICT_ATTR *)NULL) {
      DEBUG("Received unknown attribute %d\n", attribute);
    } else if ( attrlen >= AUTH_STRING_LEN ) {
      DEBUG("attribute %d too long, %d >= %d\n", attribute,
            attrlen, AUTH_STRING_LEN);
    } else {
      if ((pair = (VALUE_PAIR *)malloc(sizeof(VALUE_PAIR))) ==
          (VALUE_PAIR *)NULL) {
        fprintf(stderr, "%s: no memory\n", progname);
        exit(-1);
      }
      strcpy(pair->name, attr->name);
      pair->attribute = attr->value;
      pair->type = attr->type;
      pair->next = (VALUE_PAIR *)NULL;

      switch (attr->type) {
      
      case PW_TYPE_STRING:
        memcpy(pair->strvalue, ptr, attrlen);
        pair->strvalue[attrlen] = '\0';
        debug_pair(stdout, pair);
        if (first_pair == (VALUE_PAIR *)NULL) {
          first_pair = pair;
        } else {
          prev->next = pair;
        }
        prev = pair;
        break;

      case PW_TYPE_INTEGER:
      case PW_TYPE_IPADDR:
        memcpy(&lvalue, ptr, sizeof(UINT4));
        pair->lvalue = ntohl(lvalue);
        debug_pair(stdout, pair);
        if (first_pair == (VALUE_PAIR *)NULL) {
          first_pair = pair;
        } else {
          prev->next = pair;
        }
        prev = pair;
        break;

      default:
        DEBUG("    %s (Unknown Type %d)\n", attr->name,attr->type);
        free(pair);
        break;
      }

    }
    ptr += attrlen;
    length -= attrlen + 2;
  }
  authreq->request = first_pair;

  return (authreq);
}


/*************************************************************************
 *
 *	Function: main
 *
 *************************************************************************/
int main(int argc, char **argv)
{
  int     salen;
  int     result;
  struct    sockaddr  salocal;
  struct    sockaddr  saremote;
  struct    sockaddr_in *sin;
  struct    servent   *svp;
  u_short   lport;
  AUTH_REQ  *authreq;
  char    argval;
  int     t;
  int     cons;
  int     status;
  int     sockfd;
  APIRET  rc;

#define STACKSIZE 8192



  progname = *argv++;
  argc--;

  debug_flag = 0;

  sprintf(radius_dir_hold,"%s%s",getenv("ETC"),RADIUS_DIR);
  sprintf(radacct_dir_hold,"%s%s",getenv("ETC"),RADACCT_DIR);
  radius_dir = radius_dir_hold;
  radacct_dir = radacct_dir_hold;

  while (argc) {

    if (**argv != '-') {
      usage();
    }

    argval = *(*argv + 1);
    argc--;
    argv++;

    switch (argval) {
    
    case 'a':
      if (argc == 0) {
        usage();
      }
      radacct_dir = *argv;
      argc--;
      argv++;
      DEBUG("diff acct dir\n");
      break;

    case 'd':
      if (argc == 0) {
        usage();
      }
      radius_dir = *argv;
      argc--;
      argv++;
      DEBUG("diff radius dir\n");
      break;

    case 'n': /* Log names */
      show_names_flag=1;
      break;

    case 'p': /* Log Password failures */
      show_pwFail_flag=1;
      break;

    case 's': /* Single process mode */
      /*
     spawn_flag = 0;
     acct_flag = 1;  */
      DEBUG("Single process mode (No-op for this multithreaded version).\n");
      break;

    case 'v':
      version();
      break;

    case 'x':
      debug_flag = 1;
      DEBUG("Debug mode\n");
      break;

    case 'i':
      DEBUG("Accounting spawn process (No-op for this multithreaded version)\n");
      break;

    default:
      usage();
      break;
    }
  }

  /* Initialize the dictionary */
  if (dict_init() != 0) {
    exit(-1);
  }
  DEBUG("dict_init complete\n");
  /* Initialize Configuration Values */
  if (config_init() != 0) {
    exit(-1);
  }
  DEBUG("config_init complete\n");

  svp = getservbyname ("radius", "udp");
  if (svp == (struct servent *) 0) {
    fprintf (stderr, "%s: No such service: radius/udp\n", progname);
    exit(-1);
  }
  lport = (u_short) svp->s_port;

  sockfd = socket (AF_INET, SOCK_DGRAM, 0);
  if (sockfd < 0) {
    psock_errno ("auth socket");
    exit(-1);
  }

  sin = (struct sockaddr_in *) & salocal;
  memset ((char *) sin, '\0', sizeof (salocal));
  sin->sin_family = AF_INET;
  sin->sin_addr.s_addr = INADDR_ANY;
  sin->sin_port = lport;

  result = bind (sockfd, & salocal, sizeof (*sin));

  if (result < 0) {
    psock_errno  ("auth bind");
    exit(-1);
  }
  DEBUG("Authorization socket complete\n");

  /* -------------------------------------- */


  rc=DosCreateMutexSem(UserFileSemName, &authSem, 0, FALSE);
  if (rc==ERROR_DUPLICATE_NAME) {
    /* Already existed */
    rc=DosOpenMutexSem(UserFileSemName, &authSem);
  }
  if (rc!=0) {
    fprintf (stderr, "%s(%s): Unable to get semaphore: %s\n",
             progname, "radauth", UserFileSemName);
    exit(-1);
  }



  tidAcct=_beginthread(AccountingThread, NULL,
                       STACKSIZE,
                       NULL);
  if (tidAcct == (int)-1) {
    fprintf (stderr, "Can't start Accounting thread\n");
    exit(-1);
  }

  /* Increase our priority to maximize response to logins */
  DosSetPriority(PRTYS_THREAD, PRTYC_NOCHANGE, +2, 0);

  /* Increase priority of accounting thread to minimize its blocking
     us inside a protected semphore area */
  DosSetPriority(PRTYS_THREAD, PRTYC_NOCHANGE, +1, tidAcct);

  /* -------------------------------------- */


  /*
   *	Receive client authorization requests
   */
  sin = (struct sockaddr_in *) & saremote;
  DEBUG("Ready to receive client authorization requests\n");
  for (;;) {

    DEBUG("Waiting for read authorization sockfd\n");
    salen = sizeof (saremote);
    result = recvfrom (sockfd, (char *) recv_buffer,
                       (int) sizeof(recv_buffer),
                       (int) 0, & saremote, & salen);

    if (result > 0) {
      DEBUG("Try to validate authreq\n");
      if (show_names_flag) {
        printf("Auth: ");
      }
      authreq =  radrecv(ntohl(sin->sin_addr.s_addr),
                         ntohs(sin->sin_port),
                         recv_buffer, result);
      radrespond(authreq, sockfd);
    } else {
      if (result < 0 && errno == EINTR) {
        result = 0;
      }
    }
  }
}


