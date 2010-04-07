/*
 * ion/query/query.c
 *
 * Copyright (c) Tuomo Valkonen 1999-2004. 
 *
 * Ion is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 */

#include <limits.h>
#include <unistd.h>
#include <string.h>

#include <ioncore/extl.h>
#include "query.h"
#include "wedln.h"


/*{{{ Generic */


/*EXTL_DOC
 * Show a query window in \var{mplex} with prompt \var{prompt}, initial
 * contents \var{dflt}. The function \var{handler} is called with
 * the entered string as the sole argument when \fnref{WEdln.finish}
 * is called. The function \var{completor} is called with the created
 * \type{WEdln} is first argument and the string to complete is the
 * second argument when \fnref{WEdln.complete} is called.
 */
EXTL_EXPORT
WEdln *query_query(WMPlex *mplex, const char *prompt, const char *dflt,
                   ExtlFn handler, ExtlFn completor)
{
    WRectangle geom;
    WEdlnCreateParams fnp;
    
    /*fprintf(stderr, "query_query called %s %s %d %d\n", prompt, dflt,
            handler, completor);*/
    
    fnp.prompt=prompt;
    fnp.dflt=dflt;
    fnp.handler=handler;
    fnp.completor=completor;
    
    return (WEdln*)mplex_add_input(mplex,
                                   (WRegionAttachHandler*)create_wedln,
                                   (void*)&fnp);
}


/*}}}*/

