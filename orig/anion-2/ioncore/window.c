/*
 * ion/ioncore/window.c
 *
 * Copyright (c) Tuomo Valkonen 1999-2004. 
 *
 * Ion is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 */

#include "common.h"
#include "objp.h"
#include "global.h"
#include "window.h"
#include "focus.h"
#include "rootwin.h"
#include "stacking.h"
#include "minmax.h"


/*{{{ Dynfuns */


void window_draw(WWindow *wwin, bool complete)
{
    CALL_DYN(window_draw, wwin, (wwin, complete));
}


void window_insstr(WWindow *wwin, const char *buf, size_t n)
{
    CALL_DYN(window_insstr, wwin, (wwin, buf, n));
}


int window_press(WWindow *wwin, XButtonEvent *ev, WRegion **reg_ret)
{
    int area=0;
    CALL_DYN_RET(area, int, window_press, wwin, (wwin, ev, reg_ret));
    return area;
}


void window_release(WWindow *wwin)
{
    CALL_DYN(window_release, wwin, (wwin));
}


bool region_reparent(WRegion *reg, WWindow *par, const WRectangle *geom)
{
    bool ret=FALSE;
    CALL_DYN_RET(ret, bool, region_reparent, reg, (reg, par, geom));
    return ret;
}


/*}}}*/
    

/*{{{ Init, create */


bool window_init(WWindow *wwin, WWindow *parent, Window win,
                 const WRectangle *geom)
{
    wwin->win=win;
    wwin->xic=NULL;
    wwin->keep_on_top_list=NULL;
    region_init(&(wwin->region), (WRegion*)parent, geom);
    if(win!=None){
        XSaveContext(wglobal.dpy, win, wglobal.win_context, (XPointer)wwin);
        if(parent!=NULL)
            stacking_init_window(parent, win);
    }
    return TRUE;
}


bool window_init_new(WWindow *wwin, WWindow *parent, const WRectangle *geom)
{
    Window win;
    
    win=create_simple_window(ROOTWIN_OF(parent), parent->win, geom);
    if(win==None)
        return FALSE;
    /* window_init does not fail */
    return window_init(wwin, parent, win, geom);
}


void window_deinit(WWindow *wwin)
{
    region_deinit((WRegion*)wwin);
    
    if(wwin->xic!=NULL)
        XDestroyIC(wwin->xic);

    if(wwin->win!=None){
        XDeleteContext(wglobal.dpy, wwin->win, wglobal.win_context);
        XDestroyWindow(wglobal.dpy, wwin->win);
    }
}


/*}}}*/


/*{{{ Find, X Window -> WRegion */


WRegion *find_window(Window win)
{
    WRegion *reg;
    
    if(XFindContext(wglobal.dpy, win, wglobal.win_context,
                    (XPointer*)&reg)!=0)
        return NULL;
    
    return reg;
}


WRegion *find_window_t(Window win, const WObjDescr *descr)
{
    WRegion *reg=find_window(win);
    
    if(reg==NULL)
        return NULL;
    
    if(!wobj_is((WObj*)reg, descr))
        return NULL;
    
    return reg;
}


/*}}}*/


/*{{{ Region dynfuns */


static void reparent_or_fit_window(WWindow *wwin, WWindow *parent,
                                   const WRectangle *geom)
{
    bool move=(REGION_GEOM(wwin).x!=geom->x ||
               REGION_GEOM(wwin).y!=geom->y);
    int w=maxof(1, geom->w);
    int h=maxof(1, geom->h);

    if(parent!=NULL){
        region_detach_parent((WRegion*)wwin);
        XReparentWindow(wglobal.dpy, wwin->win, parent->win, geom->x, geom->y);
        XResizeWindow(wglobal.dpy, wwin->win, w, h);
        region_attach_parent((WRegion*)wwin, (WRegion*)parent);
    }else{
        XMoveResizeWindow(wglobal.dpy, wwin->win, geom->x, geom->y, w, h);
    }
    
    REGION_GEOM(wwin)=*geom;

    if(move)
        region_notify_subregions_move(&(wwin->region));
}


bool reparent_window(WWindow *wwin, WWindow *parent, const WRectangle *geom)
{
    if(!same_rootwin((WRegion*)wwin, (WRegion*)parent))
        return FALSE;
    reparent_or_fit_window(wwin, parent, geom);
    return TRUE;
}


void window_fit(WWindow *wwin, const WRectangle *geom)
{
    reparent_or_fit_window(wwin, NULL, geom);
}


void window_map(WWindow *wwin)
{
    XMapWindow(wglobal.dpy, wwin->win);
    MARK_REGION_MAPPED(wwin);
}


void window_unmap(WWindow *wwin)
{
    XUnmapWindow(wglobal.dpy, wwin->win);
    MARK_REGION_UNMAPPED(wwin);
}


void window_set_focus_to(WWindow *wwin, bool warp)
{
    if(warp)
        do_warp((WRegion*)wwin);
    SET_FOCUS(wwin->win);
}


void do_restack_window(Window win, Window other, int stack_mode)
{
    XWindowChanges wc;
    int wcmask;
    
    wcmask=CWStackMode;
    wc.stack_mode=stack_mode;
    if((wc.sibling=other)!=None)
        wcmask|=CWSibling;

    XConfigureWindow(wglobal.dpy, win, wcmask, &wc);
}


Window window_restack(WWindow *wwin, Window other, int mode)
{
    do_restack_window(wwin->win, other, mode);
    return wwin->win;
}


Window window_x_window(const WWindow *wwin)
{
    return wwin->win;
}


/*}}}*/


/*{{{�Dynamic function table and class implementation */


static DynFunTab window_dynfuntab[]={
    {region_fit, window_fit},
    {region_map, window_map},
    {region_unmap, window_unmap},
    {region_set_focus_to, window_set_focus_to},
    {(DynFun*)region_reparent, (DynFun*)reparent_window},
    {(DynFun*)region_restack, (DynFun*)window_restack},
    {(DynFun*)region_x_window, (DynFun*)window_x_window},
    END_DYNFUNTAB
};


IMPLOBJ(WWindow, WRegion, window_deinit, window_dynfuntab);

    
/*}}}*/

