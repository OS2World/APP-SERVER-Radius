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
/*
 *	This file was further modified by me, leaving the above copyright
 *	notice intact as required by Livingston.
 *
 *		Leo Savage	(leo@esva.net)

   ** BEGIN Backroads mod list **
   05 Feb 97  (MN)  Made user name search case insensitive
 */

static char sccsid[] =
"@(#)users.c	1.12 Copyright 1992 Livingston Enterprises Inc";

#include	<sys/types.h>
#include	<sys/socket.h>
#include	<sys/time.h>
#include	<netinet/in.h>

#include	<stdio.h>
#include	<netdb.h>
#include	<pwd.h>
#include	<time.h>
#include	<ctype.h>
#include  <process.h>
#include  <stdlib.h>

#ifdef DBM

#include	<dbm.h>

#endif /* DBM */

#include	"radius.h"




extern char		*progname;
extern int		debug_flag;
extern char		*radius_dir;

#define FIND_MODE_NAME	  0
#define FIND_MODE_DEFAULT 1
#define FIND_MODE_REPLY	  2
#define FIND_MODE_SKIP    3
#define FIND_MODE_FLUSH   4




#define PARSE_MODE_NAME		0
#define PARSE_MODE_EQUAL	1
#define PARSE_MODE_VALUE	2
#define PARSE_MODE_INVALID	3



/*************************************************************************
 *
 *	Function: fieldcpy
 *
 *	Purpose: Copy a data field from the buffer.  Advance the buffer
 *		 past the data field.
 *
 *************************************************************************/

static	void fieldcpy(char *string, char **uptr)
{
	char	*ptr;

	ptr = *uptr;
	if(*ptr == '"') {
		ptr++;
		while(*ptr != '"' && *ptr != '\0' && *ptr != '\n') {
			*string++ = *ptr++;
		}
		*string = '\0';
		if(*ptr == '"') {
			ptr++;
		}
		*uptr = ptr;
		return;
	}

	while(*ptr != ' ' && *ptr != '\t' && *ptr != '\0' && *ptr != '\n' &&
						*ptr != '=' && *ptr != ',') {
			*string++ = *ptr++;
	}
	*string = '\0';
	*uptr = ptr;
	return;
}


/*************************************************************************
 *
 *	Function: user_update
 *
 *	Purpose: Updates a user in the database.  Replaces the original
 *		 entry with the name, the list of check items, and the
 *		 list of reply items which are supplied.
 *
 *************************************************************************/

int user_update(char *name, VALUE_PAIR	*user_check, VALUE_PAIR	*user_reply)
{
	FILE		*oldfd;
	FILE		*userfd;
	char		buffer[256];
	char		buffer1[256];
	int		namelen;
	int		mode;

	sprintf(buffer, "%s/%s", radius_dir, RADIUS_USERS);
	sprintf(buffer1, "%s/%s", radius_dir, RADIUS_HOLD);

	/* Move the user table to a temporary location */
	if(rename(buffer, buffer1) != 0) {
		fprintf(stderr, "%s: Couldn't rename %s\n",
				progname, buffer);
		return(-1);
	}

	/* Open the old user file (using the temporary name */
	if((oldfd = fopen(buffer1, "r")) == (FILE *)NULL) {
		fprintf(stderr, "%s: Couldn't open %s for reading\n",
				progname, buffer1);
		exit(-1);
	}

	/* Open the new user file */
	if((userfd = fopen(buffer, "w")) == (FILE *)NULL) {
		fprintf(stderr, "%s: Couldn't open %s for writing\n",
				progname, buffer);
		exit(-1);
	}

	mode = FIND_MODE_NAME;
	namelen = strlen(name);

	/* Copy the old to the new, only recreating the changed user */
	while(fgets(buffer, sizeof(buffer), oldfd) != (char *)NULL) {
		if(mode == FIND_MODE_NAME) {
			if((strncmp(buffer, name, namelen) == 0 &&
		 	 (buffer[namelen] == ' ' || buffer[namelen] == '\t'))) {

				/* Write our new information */
				fprintf(userfd, "%s\t", name);
				while(user_check != (VALUE_PAIR *)NULL) {
					fprint_attr_val(userfd, user_check);
					if(user_check->next !=
							(VALUE_PAIR *)NULL) {
						fprintf(userfd, ", ");
					}
					user_check = user_check->next;
				}
				fprintf(userfd, "\n\t");
				while(user_reply != (VALUE_PAIR *)NULL) {
					fprint_attr_val(userfd, user_reply);
					if(user_reply->next !=
							(VALUE_PAIR *)NULL) {
						fprintf(userfd, ",\n\t");
					}
					user_reply = user_reply->next;
				}
				fprintf(userfd, "\n");
				mode = FIND_MODE_SKIP;
			}
			else {
				fputs(buffer, userfd);
			}
		}
		else if(mode == FIND_MODE_SKIP) {
			if(*buffer != ' ' && *buffer != '\t') {
				fputs(buffer, userfd);
				mode = FIND_MODE_FLUSH;
			}
		}
		else {
			fputs(buffer, userfd);
		}
	}
	fclose(oldfd);
	fclose(userfd);
	return(0);
}


/*************************************************************************
 *
 *	Function: user_prtime
 *
 *	Purpose: Turns printable string into correct tm struct entries
 *
 *************************************************************************/

static char *months[] = {
	"Jan", "Feb", "Mar", "Apr", "May", "Jun",
	"Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

user_gettime(char		*valstr, struct tm	*tm)
{
	char	buffer[48];
	int	i;

	/* Get the month */
	for(i = 0;i < 12;i++) {
		if(strncmp(months[i], valstr, 3) == 0) {
			tm->tm_mon = i;
			i = 13;
		}
	}

	/* Get the Day */
	tm->tm_mday = atoi(&valstr[4]);

	/* Now the year */
	tm->tm_year = atoi(&valstr[7]) - 1900;
  return (0);
}



/*************************************************************************
 *
 *	Function: userparse
 *
 *	Purpose: Parses the buffer to extract the attribute-value pairs.
 *
 *************************************************************************/

static int userparse(char *buffer, VALUE_PAIR	**first_pair)
{
	int		mode;
	char		attrstr[64];
	char		valstr[64];
	DICT_ATTR	*attr;
	DICT_ATTR	*dict_attrfind();
	DICT_VALUE	*dval;
	DICT_VALUE	*dict_valfind();
	VALUE_PAIR	*pair;
	VALUE_PAIR	*link, *plink;
	UINT4		ipstr2long();
	UINT4		get_ipaddr();
	struct tm	*tm;
	time_t		timeval;

	mode = PARSE_MODE_NAME;
	while(*buffer != '\n' && *buffer != '\0') {

		if(*buffer == ' ' || *buffer == '\t' || *buffer == ',') {
			buffer++;
			continue;
		}

		switch(mode) {

		case PARSE_MODE_NAME:
			/* Attribute Name */
			fieldcpy(attrstr, &buffer);
			if((attr = dict_attrfind(attrstr)) ==
						(DICT_ATTR *)NULL) {
				return(-1);
			}
			mode = PARSE_MODE_EQUAL;
			break;

		case PARSE_MODE_EQUAL:
			/* Equal sign */
			if(*buffer == '=') {
				mode = PARSE_MODE_VALUE;
				buffer++;
			}
			else {
				return(-1);
			}
			break;

		case PARSE_MODE_VALUE:
			/* Value */
			fieldcpy(valstr, &buffer);

			if((pair = (VALUE_PAIR *)malloc(sizeof(VALUE_PAIR))) ==
						(VALUE_PAIR *)NULL) {
				fprintf(stderr, "%s: no memory\n",
						progname);
				exit(-1);
			}
			strcpy(pair->name, attr->name);
			pair->attribute = attr->value;
			pair->type = attr->type;

			switch(pair->type) {

			case PW_TYPE_STRING:
				strcpy(pair->strvalue, valstr);
				break;

			case PW_TYPE_INTEGER:
				if(isdigit(*valstr)) {
					pair->lvalue = atoi(valstr);
				}
				else if((dval = dict_valfind(valstr)) ==
							(DICT_VALUE *)NULL) {
					free(pair);
					return(-1);
				}
				else {
					pair->lvalue = dval->value;
				}
				break;

			case PW_TYPE_IPADDR:
				pair->lvalue = get_ipaddr(valstr);
				break;

			case PW_TYPE_DATE:
				timeval = time(0);
				tm = localtime(&timeval);
				user_gettime(valstr, tm);
#ifdef TIMELOCAL
				pair->lvalue = (UINT4)timelocal(tm);
#else /* TIMELOCAL */
				pair->lvalue = (UINT4)mktime(tm);
#endif /* TIMELOCAL */
				break;

			default:
				free(pair);
				return(-1);
			}

			plink = (VALUE_PAIR *)NULL;
			link = *first_pair;

			while(link != (VALUE_PAIR *)NULL) {
				if(link->attribute == pair->attribute)
					break;
				plink = link;
				link = link->next;
			}

			if(link != (VALUE_PAIR *)NULL) {
				pair->next = link->next;
				free(link);
			}
			else	pair->next = (VALUE_PAIR *)NULL;

			if(plink != (VALUE_PAIR *)NULL)
				plink->next = pair;
			else	*first_pair = pair;

			mode = PARSE_MODE_NAME;
			break;

		default:
			mode = PARSE_MODE_NAME;
			break;
		}
	}
	return(0);
}




/*************************************************************************
 *
 *	Function: user_find
 *
 *	Purpose: Find the named user in the database.  Create the
 *		 set of attribute-value pairs to check and reply with
 *		 for this user from the database.
 *
 *************************************************************************/

user_find(char *name, VALUE_PAIR	**check_pairs, VALUE_PAIR	**reply_pairs)
{
	FILE		*userfd;
	char		buffer[256];
	char		msg[128];
	char		*ptr;
	int		namelen;
	char		dfltname[256];
	int		dfltnamelen;
	int		mode;
	VALUE_PAIR	*check_first;
	VALUE_PAIR	*reply_first;
#ifdef DBM
	datum		named;
	datum		contentd;
#endif /* DBM */

	/*
	 * Check for valid input, zero length names not permitted
	 */

	mode = FIND_MODE_NAME;

	ptr=name; strcpy(dfltname,"DEFAULT");
	while (*ptr != '\0') {
		if (*ptr == ' ' || *ptr == '\t') {
			*ptr = '\0';
		}
		else if(*ptr == '.') {
			strcat(dfltname,ptr);
			*ptr = '\0';
		} else {
			ptr++;
		}
	}

	namelen=strlen(name);
	dfltnamelen = strlen(dfltname);

	if (namelen < 1) {
		fprintf(stderr, "%s: zero length username not permitted\n",progname);
		return(-1);
	}


	/*
	 * Open the user table
	 */
	sprintf(buffer, "%s/%s", radius_dir, RADIUS_USERS);
#ifdef DBM
	if(dbminit(buffer) != 0) {
#else /* DBM */
	if((userfd = fopen(buffer, "r")) == (FILE *)NULL) {
#endif /* DBM */
		fprintf(stderr, "%s:Couldn't open %s for reading\n",
				progname, buffer);
		return(-1);
	}

	check_first = (VALUE_PAIR *)NULL;
	reply_first = (VALUE_PAIR *)NULL;


#ifdef DBM
	named.dptr = name;
	named.dsize = strlen(name);
	contentd = fetch(named);

	if(contentd.dsize == 0) {
		named.dptr = "DEFAULT";
		named.dsize = strlen("DEFAULT");
		contentd = fetch(named);
		if(contentd.dsize == 0) {
			dbmclose();
			return(-1);
		}
	}

	/*
	 * Parse the check values
	 */
	ptr = contentd.dptr;
	contentd.dptr[contentd.dsize] = '\0';

	if(userparse(ptr, &check_first) != 0) {
		sprintf(msg, "%s: Parse error for user %s\n",
				progname, name);
		fprintf(stderr, msg);
		log_err(msg);
		pairfree(check_first);
		dbmclose();
		return(-1);
	}
	while(*ptr != '\n' && *ptr != '\0') {
		ptr++;
	}
	if(*ptr != '\n') {
		pairfree(check_first);
		dbmclose();
		return(-1);
	}
	ptr++;
	/*
	 * Parse the reply values
	 */
	if(userparse(ptr, &reply_first) != 0) {
		fprintf(stderr, "%s: Parse error for user %s\n",
			progname, name);
		pairfree(check_first);
		pairfree(reply_first);
		dbmclose();
		return(-1);
	}
	dbmclose();

#else /* DBM */

	while(fgets(buffer, sizeof(buffer), userfd) != (char *)NULL) {
		if(mode != FIND_MODE_NAME) {
			if(*buffer == ' ' || *buffer == '\t') {
				/*
				 * Parse the reply values
				 */
				if(userparse(buffer, &reply_first) != 0) {
					fprintf(stderr,
						"%s: Parse error for user %s\n",
						progname, name);
					pairfree(check_first);
					pairfree(reply_first);
					fclose(userfd);
					return(-1);
				}
			}
			else if(mode == FIND_MODE_DEFAULT) {
				mode = FIND_MODE_NAME;
			}
			else {
				/* We are done */

				fclose(userfd);
				*check_pairs = check_first;
				*reply_pairs = reply_first;
				return(0);
			}
		}

		if(mode == FIND_MODE_NAME) {
			/*
			 * Find the entry starting with the users name
			 */
			if((strnicmp(buffer, name, namelen) == 0)
			&& isspace(buffer[namelen])) {
				ptr = &buffer[namelen];
				mode = FIND_MODE_REPLY;
			}

			else if((strncmp(buffer, dfltname, dfltnamelen) == 0)
			&& isspace(buffer[dfltnamelen])) {
				ptr = &buffer[dfltnamelen];
				mode = FIND_MODE_DEFAULT;
			}

			else if((strncmp(buffer, "DEFAULT", 7) == 0)
			&& isspace(buffer[7])) {
				ptr = &buffer[7];
				mode = FIND_MODE_DEFAULT;
			}

			else if((strncmp(buffer, "UNLISTED", 8) == 0)
			&& isspace(buffer[8])) {
				ptr = &buffer[8];
				mode = FIND_MODE_REPLY;
			}

			else continue;

			/*
			 * Parse the check values
			 */
			if(userparse(ptr, &check_first) != 0) {
				sprintf(msg,"%s: Parse error for user %s\n",
					progname, name);
				fprintf(stderr,msg);
				log_err(msg);
				pairfree(check_first);
				fclose(userfd);
				return(-1);
			}
		}
	}
	fclose(userfd);
#endif /* DBM */

	/* Update the callers pointers */
	if(mode == FIND_MODE_REPLY) {
		if(reply_first != (VALUE_PAIR *)NULL) {
			*check_pairs = check_first;
			*reply_pairs = reply_first;
			return(0);
		}
	}

	pairfree(check_first);
	pairfree(reply_first);
	return(-1);
}
