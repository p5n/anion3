/*
 * libtu/util.c
 *
 * Copyright (c) Tuomo Valkonen 1999-2002. 
 *
 * You may distribute and modify this library under the terms of either
 * the Clarified Artistic License or the GNU LGPL, version 2.1 or later.
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef CONFIG_LOCALE
#include <libintl.h>
#endif

#include "util.h"
#include "misc.h"


static const char *progname=NULL;


void libtu_init(const char *argv0)
{
	progname=argv0;
	
#ifdef CONFIG_LOCALE
	textdomain(simple_basename(argv0));
#endif
}


void libtu_init_copt(int argc, char *const argv[],
					 const OptParserCommonInfo *cinfo)
{
	int opt;
	
	libtu_init(argv[0]);
	
	optparser_init(argc, argv, OPTP_DEFAULT, NULL, cinfo);
	
    while((opt=optparser_get_opt())){	
		switch(opt){
		default:
			optparser_print_error();
			exit(EXIT_FAILURE);
		}
	}
}


const char *prog_execname()
{
	return progname;
}

