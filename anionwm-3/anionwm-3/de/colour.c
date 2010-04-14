/*
 * ion/de/colour.h
 *
 * Copyright (c) Tuomo Valkonen 1999-2009. 
 *
 * See the included file LICENSE for details.
 */

#include <ioncore/common.h>
#include "colour.h"


bool de_alloc_colour(WRootWin *rootwin, DEColour *ret, const char *name)
{
#ifndef XFT
    XColor c;
    bool ok=FALSE;
#else /* XFT */
    if(name==NULL)
	    return FALSE;
    return XftColorAllocName(
	    ioncore_g.dpy,
	    XftDEDefaultVisual(),
	    rootwin->default_cmap,
	    name,
	    ret);
#endif /* XFT */

#ifndef XFT
    if(name==NULL)
        return FALSE;

    if(XParseColor(ioncore_g.dpy, rootwin->default_cmap, name, &c)){
        ok=XAllocColor(ioncore_g.dpy, rootwin->default_cmap, &c);
        if(ok)
            *ret=c.pixel;
    }
    
    return ok;
#endif /* ! XFT */
}


bool de_duplicate_colour(WRootWin *rootwin, DEColour in, DEColour *out)
{
#ifndef XFT
    XColor c;
    c.pixel=in;
    XQueryColor(ioncore_g.dpy, rootwin->default_cmap, &c);
    if(XAllocColor(ioncore_g.dpy, rootwin->default_cmap, &c)){
        *out=c.pixel;
        return TRUE;
    }
    return FALSE;
#else /* XFT */
    return XftColorAllocName(
	    ioncore_g.dpy,
	    XftDEDefaultVisual(),
	    rootwin->default_cmap,
	    &(in.color),
	    out);
#endif /* XFT */
}


void de_free_colour_group(WRootWin *rootwin, DEColourGroup *cg)
{
#ifndef XFT
    DEColour pixels[5];
    
    pixels[0]=cg->bg;
    pixels[1]=cg->fg;
    pixels[2]=cg->hl;
    pixels[3]=cg->sh;
    pixels[4]=cg->pad;
    
    XFreeColors(ioncore_g.dpy, rootwin->default_cmap, pixels, 5, 0);
    
    gr_stylespec_unalloc(&cg->spec);
#else /* XFT */
    de_free_colour(rootwin, cg->bg);
    de_free_colour(rootwin, cg->fg);
    de_free_colour(rootwin, cg->hl);
    de_free_colour(rootwin, cg->sh);
    de_free_colour(rootwin, cg->pad);
#endif /* XFT */
}


void de_free_colour(WRootWin *rootwin, DEColour col)
{
#ifndef XFT
    DEColour pixels[1];
    
    pixels[0]=col;
    
    XFreeColors(ioncore_g.dpy, rootwin->default_cmap, pixels, 1, 0);
#else /* XFT */
    XftColorFree(ioncore_g.dpy, XftDEDefaultVisual(), rootwin->default_cmap, &col);
#endif /* XFT */
}

