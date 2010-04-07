/*
 * ion/ioncore/colormap.c
 *
 * Copyright (c) Tuomo Valkonen 1999-2004. 
 *
 * Ion is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 */

#include "common.h"
#include "global.h"
#include "property.h"
#include "clientwin.h"
#include "colormap.h"


/*{{{ Installing colormaps */


void install_cmap(WRootWin *rootwin, Colormap cmap)
{
    if(cmap==None)
        cmap=rootwin->default_cmap;
    XInstallColormap(wglobal.dpy, cmap);
}


void install_cwin_cmap(WClientWin *cwin)
{
    int i;
    bool found=FALSE;

    for(i=cwin->n_cmapwins-1; i>=0; i--){
        install_cmap(ROOTWIN_OF(cwin), cwin->cmaps[i]);
        if(cwin->cmapwins[i]==cwin->win)
            found=TRUE;
    }
    
    if(found)
        return;
    
    install_cmap(ROOTWIN_OF(cwin), cwin->cmap);
}


void handle_cwin_cmap(WClientWin *cwin, const XColormapEvent *ev)
{
    int i;
    
    if(ev->window==cwin->win){
        cwin->cmap=ev->colormap;
        if(REGION_IS_ACTIVE(cwin))
            install_cwin_cmap(cwin);
    }else{
        for(i=0; i<cwin->n_cmapwins; i++){
            if(cwin->cmapwins[i]!=ev->window)
                continue;
            cwin->cmaps[i]=ev->colormap;
            if(REGION_IS_ACTIVE(cwin))
                install_cwin_cmap(cwin);
            break;
        }
    }
}


void handle_all_cmaps(const XColormapEvent *ev)
{
    WClientWin *cwin;

    FOR_ALL_CLIENTWINS(cwin){
        handle_cwin_cmap(cwin, ev);
    }
}


/*}}}*/


/*{{{ Management */


static XContext ctx=None;


void xwindow_unmanaged_selectinput(Window win, long mask)
{
    int *p=NULL;
    
    /* We may be monitoring for colourmap changes */
    if(ctx!=None){
        if(XFindContext(wglobal.dpy, win, ctx, (XPointer*)&p)==0){
            if(*p>0)
                mask|=ColormapChangeMask;
        }
    }
    
    XSelectInput(wglobal.dpy, win, mask);
}

                
        
static void xwindow_selcmap(Window win)
{
    int *p=NULL;
    XWindowAttributes attr;
    
    if(ctx==None)
        ctx=XUniqueContext();
    
    if(XFindContext(wglobal.dpy, win, ctx, (XPointer*)&p)==0){
        (*p)++;
    }else{
        p=ALLOC(int);
        if(p==NULL){
            warn_err();
            return;
        }
        *p=1;
        if(XSaveContext(wglobal.dpy, win, ctx, (XPointer)p)!=0){
            warn("Unable to save colourmap watch info.");
            return;
        }

        if(FIND_WINDOW(win)==NULL){
            XGetWindowAttributes(wglobal.dpy, win, &attr);
            XSelectInput(wglobal.dpy, win, 
                         attr.your_event_mask|ColormapChangeMask);
        }
    }
}


static void xwindow_unselcmap(Window win)
{
    int *p=NULL;
    XWindowAttributes attr;
    
    if(ctx==None)
        return;

    if(XFindContext(wglobal.dpy, win, ctx, (XPointer*)&p)==0){
        (*p)--;
        if(*p==0){
            XDeleteContext(wglobal.dpy, win, ctx);
            free(p);
            if(FIND_WINDOW(win)==NULL){
                XGetWindowAttributes(wglobal.dpy, win, &attr);
                XSelectInput(wglobal.dpy, win, 
                             attr.your_event_mask&~ColormapChangeMask);
            }
        }
    }
}


void clientwin_get_colormaps(WClientWin *cwin)
{
    Window *wins;
    XWindowAttributes attr;
    int i, n;
    
    clientwin_clear_colormaps(cwin);
    
    n=get_property(wglobal.dpy, cwin->win, wglobal.atom_wm_colormaps,
                   XA_WINDOW, 100L, TRUE, (uchar**)&wins);
    
    if(n<=0)
        return;
    
    cwin->cmaps=ALLOC_N(Colormap, n);
    
    if(cwin->cmaps==NULL){
        warn_err();
        return;
    }
    
    cwin->cmapwins=wins;
    cwin->n_cmapwins=n;
    
    for(i=0; i<n; i++){
        if(wins[i]==cwin->win){
            cwin->cmaps[i]=cwin->cmap;
        }else{
            xwindow_selcmap(wins[i]);
            XGetWindowAttributes(wglobal.dpy, wins[i], &attr);
            cwin->cmaps[i]=attr.colormap;
        }
    }
}


void clientwin_clear_colormaps(WClientWin *cwin)
{
    int i;
    XWindowAttributes attr;

    if(cwin->n_cmapwins==0)
        return;

    /* what if managed ??? */
    for(i=0; i<cwin->n_cmapwins; i++){
        if(cwin->cmapwins[i]!=cwin->win)
            xwindow_unselcmap(cwin->cmapwins[i]);
    }
    
    free(cwin->cmapwins);
    free(cwin->cmaps);
    cwin->n_cmapwins=0;
    cwin->cmapwins=NULL;
    cwin->cmaps=NULL;
}


/*}}}*/

