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

static char sccsid[] =
"@(#)@(#)attrprint.c	1.3 Copyright 1992 Livingston Enterprises Inc";

#include	<sys/types.h>
#include	<sys/socket.h>
#include	<sys/time.h>
#include	<netinet/in.h>

#include	<stdio.h>
#include	<netdb.h>
#include	<pwd.h>
#include	<time.h>
#include	<ctype.h>

#include	"radius.h"

/*************************************************************************
 *
 *	Function: fprint_attr_val
 *
 *	Purpose: Write a printable version of the attribute-value
 *		 pair to the supplied File.
 *
 *************************************************************************/
void
fprint_attr_val(fd, pair)
FILE		*fd;
VALUE_PAIR	*pair;
{
	DICT_VALUE	*dict_valget();
	DICT_VALUE	*dval;
	char		buffer[32];
	u_char		*ptr;

	switch(pair->type) {

	case PW_TYPE_STRING:
		fprintf(fd, "%s = \"", pair->name);
		ptr = (u_char *)pair->strvalue;
		while(*ptr != '\0') {
			if(!(isprint(*ptr))) {
				fprintf(fd, "\\%03o", *ptr);
			}
			else {
				fputc(*ptr, fd);
			}
			ptr++;
		}
		fputc('"', fd);
		break;
			
	case PW_TYPE_INTEGER:
		dval = dict_valget(pair->lvalue, pair->name);
		if(dval != (DICT_VALUE *)NULL) {
			fprintf(fd, "%s = %s", pair->name, dval->name);
		}
		else {
			fprintf(fd, "%s = %ld", pair->name, pair->lvalue);
		}
		break;

	case PW_TYPE_IPADDR:
		ipaddr2str(buffer, pair->lvalue);
		fprintf(fd, "%s = %s", pair->name, buffer);
		break;

	case PW_TYPE_DATE:
		strftime(buffer, sizeof(buffer), "%b %e %Y",
					gmtime((time_t *)&pair->lvalue));
		fprintf(fd, "%s = \"%s\"", pair->name, buffer);
		break;

	default:
		fprintf(fd, "Unknown type %d", pair->type);
		break;
	}
}
