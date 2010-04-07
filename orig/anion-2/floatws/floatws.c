/*
 * ion/floatws/floatws.c
 *
 * Copyright (c) Tuomo Valkonen 1999-2004. 
 *
 * Ion is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 */

#include <string.h>

#include <ioncore/common.h>
#include <ioncore/rootwin.h>
#include <ioncore/focus.h>
#include <ioncore/global.h>
#include <ioncore/objp.h>
#include <ioncore/region.h>
#include <ioncore/manage.h>
#include <ioncore/screen.h>
#include <ioncore/names.h>
#include <ioncore/saveload.h>
#include <ioncore/attach.h>
#include <ioncore/regbind.h>
#include <ioncore/genframe-pointer.h>
#include <ioncore/stacking.h>
#include <ioncore/extlconv.h>
#include <ioncore/defer.h>
#include <ioncore/minmax.h>

#include "floatws.h"
#include "floatframe.h"
#include "placement.h"
#include "main.h"


WObjList *floatws_sticky_list=NULL;


/*{{{ region dynfun implementations */


static void floatws_fit(WFloatWS *ws, const WRectangle *geom)
{
    REGION_GEOM(ws)=*geom;
}


static bool reparent_floatws(WFloatWS *ws, WWindow *parent, 
                             const WRectangle *geom)
{
    WRegion *sub, *next;
    bool rs;
    int xdiff, ydiff;
    
    if(!same_rootwin((WRegion*)ws, (WRegion*)parent))
        return FALSE;
    
    region_detach_parent((WRegion*)ws);
    XReparentWindow(wglobal.dpy, ws->dummywin, parent->win, geom->x, geom->h);
    region_attach_parent((WRegion*)ws, (WRegion*)parent);
    
    xdiff=geom->x-REGION_GEOM(ws).x;
    ydiff=geom->y-REGION_GEOM(ws).y;
    
    FOR_ALL_MANAGED_ON_LIST_W_NEXT(ws->managed_list, sub, next){
        WRectangle g=REGION_GEOM(sub);
        g.x+=xdiff;
        g.y+=ydiff;
        if(!region_reparent(sub, parent, &g)){
            warn("Problem: can't reparent a %s managed by a WFloatWS"
                 "being reparented. Detaching from this object.",
                 WOBJ_TYPESTR(sub));
            region_detach_manager(sub);
        }
    }
    
    return TRUE;
}


static void move_sticky(WFloatWS *ws)
{
    WRegion *reg, *par=REGION_PARENT(ws);
    
    if(par==NULL || !WOBJ_IS(par, WMPlex))
        return;
    
    ITERATE_OBJLIST(WRegion*, reg, floatws_sticky_list){
        WFloatWS *rmgr;
        if(REGION_PARENT(reg)!=par)
            continue;
        rmgr=REGION_MANAGER_CHK(reg, WFloatWS);
        if(rmgr==NULL)
            continue;
        if(rmgr->current_managed==reg)
            rmgr->current_managed=NULL;
        region_unset_manager(reg, (WRegion*)rmgr, &(rmgr->managed_list));
        region_set_manager(reg, (WRegion*)ws, &(ws->managed_list));
        if(REGION_IS_ACTIVE(reg) && ws->current_managed==NULL)
            ws->current_managed=reg;
    }
}


static void floatws_map(WFloatWS *ws)
{
    WRegion *reg;

    move_sticky(ws);

    MARK_REGION_MAPPED(ws);
    XMapWindow(wglobal.dpy, ws->dummywin);

    FOR_ALL_MANAGED_ON_LIST(ws->managed_list, reg){
        region_map(reg);
    }
}


static void floatws_unmap(WFloatWS *ws)
{
    WRegion *reg;
    
    MARK_REGION_UNMAPPED(ws);
    XUnmapWindow(wglobal.dpy, ws->dummywin);
    
    FOR_ALL_MANAGED_ON_LIST(ws->managed_list, reg){
        region_unmap(reg);
    }
}


static WRegion *floatws_get_focus_to(WFloatWS *ws)
{
    WRegion *next=NULL;
    WWindow *par=REGION_PARENT_CHK(ws, WWindow);
    
    if(par!=NULL){
        Window root=None, parent=None, *children=NULL;
        uint nchildren=0;
        WRegion *r;
        XQueryTree(wglobal.dpy, region_x_window((WRegion*)par),
                   &root, &parent, &children, &nchildren);
        while(nchildren>0){
            nchildren--;
            r=find_window(children[nchildren]);
            if(r!=NULL && REGION_MANAGER(r)==(WRegion*)ws){
                next=r;
                break;
            }
        }
        XFree(children);
    }
    
    return (next ? next : ws->managed_list);
}


static void floatws_set_focus_to(WFloatWS *ws, bool warp)
{
    WRegion *r=ws->current_managed;
        
    if(r==NULL)
        r=floatws_get_focus_to(ws);

    if(r==NULL){
        SET_FOCUS(ws->dummywin);
        if(warp)
            do_warp((WRegion*)ws);
        return;
    }
    
    region_set_focus_to(r, warp);
}


static bool floatws_display_managed(WFloatWS *ws, WRegion *reg)
{
    if(!region_is_fully_mapped((WRegion*)ws))
       return FALSE;
    region_map(reg);
    return TRUE;
}


static void floatws_remove_managed(WFloatWS *ws, WRegion *reg)
{
    WRegion *next=NULL;
    bool mcf=region_may_control_focus((WRegion*)ws);
    
    region_unset_manager(reg, (WRegion*)ws, &(ws->managed_list));
    region_remove_bindmap_owned(reg, &floatws_bindmap, (WRegion*)ws);
    
    if(ws->current_managed!=reg)
        return;
    
    ws->current_managed=NULL;
    
    if(mcf){
        if(reg->stacking.above!=NULL)
            next=reg->stacking.above;
        else
            next=floatws_get_focus_to(ws);
        
        do_set_focus(next!=NULL ? next : (WRegion*)ws, FALSE);
    }
}


static void floatws_managed_activated(WFloatWS *ws, WRegion *reg)
{
    ws->current_managed=reg;
}


static Window floatws_x_window(const WFloatWS *ws)
{
    return ws->dummywin;
}


/*}}}*/


/*{{{ Create/destroy */


static bool floatws_init(WFloatWS *ws, WWindow *parent, 
                         const WRectangle *bounds)
{
    if(!WOBJ_IS(parent, WWindow))
        return FALSE;

    ws->dummywin=XCreateWindow(wglobal.dpy, parent->win,
                                bounds->x, bounds->y, 1, 1, 0,
                                CopyFromParent, InputOnly,
                                CopyFromParent, 0, NULL);
    if(ws->dummywin==None)
        return FALSE;

    XSelectInput(wglobal.dpy, ws->dummywin,
                 FocusChangeMask|KeyPressMask|KeyReleaseMask|
                 ButtonPressMask|ButtonReleaseMask);
    XSaveContext(wglobal.dpy, ws->dummywin, wglobal.win_context,
                 (XPointer)ws);
    
    ws->managed_list=NULL;
    ws->current_managed=NULL;

    genws_init(&(ws->genws), parent, bounds);

    region_add_bindmap((WRegion*)ws, &floatws_bindmap);
    
    return TRUE;
}


WFloatWS *create_floatws(WWindow *parent, const WRectangle *bounds)
{
    CREATEOBJ_IMPL(WFloatWS, floatws, (p, parent, bounds));
}


void floatws_deinit(WFloatWS *ws)
{
    while(ws->managed_list!=NULL)
        destroy_obj((WObj*)(ws->managed_list));

    genws_deinit(&(ws->genws));

    XDeleteContext(wglobal.dpy, ws->dummywin, wglobal.win_context);
    XDestroyWindow(wglobal.dpy, ws->dummywin);
}


static bool floatws_do_rescue_clientwins(WFloatWS *ws, WRegion *dst)
{
    return region_do_rescue_managed_clientwins((WRegion*)ws, dst,
                                               ws->managed_list);
}


/*EXTL_DOC
 * Destroys \var{ws} unless this would put the WM in a possibly unusable
 * state.
 */
EXTL_EXPORT_MEMBER
bool floatws_relocate_and_close(WFloatWS *ws)
{
    if(!region_may_destroy((WRegion*)ws)){
        warn("Workspace may not be destroyed.");
        return FALSE;
    }
    
    /* TODO: move frames to other workspaces */
    
    if(!region_rescue_clientwins((WRegion*)ws)){
        warn("Failed to rescue some client windows!");
        return FALSE;
    }
    
    defer_destroy((WObj*)ws);
    return TRUE;
}


void floatws_close(WFloatWS *ws)
{
    if(ws->managed_list!=NULL){
        warn("Workspace %s is still managing other objects "
             " -- refusing to close.", region_name((WRegion*)ws));
        return;
    }
    
    floatws_relocate_and_close(ws);
}


/*}}}*/


/*{{{ manage_clientwin/transient */


static void floatws_add_managed(WFloatWS *ws, WRegion *reg)
{
    region_set_manager(reg, (WRegion*)ws, &(ws->managed_list));
    region_add_bindmap_owned(reg, &floatws_bindmap, (WRegion*)ws);

    if(region_is_fully_mapped((WRegion*)ws))
        region_map(reg);
}


static WRegion *floatws_do_attach(WFloatWS *ws, WRegionAttachHandler *fn,
                                  void *fnparams, const WRectangle *geom)
{
    WWindow *par;
    WRegion *reg;

    par=REGION_PARENT_CHK(ws, WWindow);
    assert(par!=NULL);
    
    reg=fn(par, geom, fnparams);

    if(reg!=NULL)
        floatws_add_managed(ws, reg);
    
    return reg;
}


static WRegion *floatws_attach_load(WFloatWS *ws, ExtlTab param)
{
    WRectangle geom;
    
    if(!extl_table_gets_geom(param, "geom", &geom)){
        warn("No geometry specified");
        return NULL;
    }
    
    geom.w=maxof(geom.w, 0);
    geom.h=maxof(geom.h, 0);

    return attach_load_helper((WRegion*)ws, param, 
                              (WRegionDoAttachFn*)floatws_do_attach,
                              &geom);
}


#define REG_OK(R) region_has_manage_clientwin(R)


static WMPlex *find_existing(WFloatWS *ws)
{
    WRegion *r=ws->current_managed;
    
    if(r!=NULL && REG_OK(r))
        return (WMPlex*)r;
    
    FOR_ALL_MANAGED_ON_LIST(ws->managed_list, r){
        if(REG_OK(r))
            return (WMPlex*)r;
    }
    
    return NULL;
}


static bool floatws_do_manage_clientwin(WFloatWS *ws, WClientWin *cwin,
                                        const WManageParams *param, 
                                        bool respectpos)
{
    WMPlex *target=NULL;
    WWindow *par;
    WRectangle fgeom=param->geom;

    par=REGION_PARENT_CHK(ws, WWindow);
    assert(par!=NULL);
    
    initial_to_floatframe_geom(ws, &fgeom, param->gravity);

    if(param->maprq && wglobal.opmode!=OPMODE_INIT){
        /* When the window is mapped by application request, position
         * request is only honoured if the position was given by the user
         * and in case of a transient (the app may know better where to 
         * place them) or if we're initialising.
         */
        respectpos=(param->tfor!=NULL || param->userpos);
    }
    
    /* However, if the requested geometry does not overlap the
     * workspaces's geometry, position request is never honoured.
     */
    if((fgeom.x+fgeom.w<=REGION_GEOM(ws).x) ||
       (fgeom.y+fgeom.h<=REGION_GEOM(ws).y) ||
       (fgeom.x>=REGION_GEOM(ws).x+REGION_GEOM(ws).w) ||
       (fgeom.y>=REGION_GEOM(ws).y+REGION_GEOM(ws).h)){
        respectpos=FALSE;
    }
    
    if(!respectpos)
        floatws_calc_placement(ws, &fgeom);
    
    target=(WMPlex*)create_floatframe(par, &fgeom);
    
    if(target==NULL){
        warn("Failed to create a new WFloatFrame for client window");
        return FALSE;
    }

    assert(same_rootwin((WRegion*)target, (WRegion*)cwin));
    
    if(!mplex_attach_simple(target, (WRegion*)cwin, param->switchto)){
        destroy_obj((WObj*)target);
        return FALSE;
    }

    floatws_add_managed(ws, (WRegion*)target);
    
    /* Don't warp, it is annoying in this case */
    if(param->switchto && region_may_control_focus((WRegion*)ws)){
        set_previous_of((WRegion*)target);
        set_focus((WRegion*)target);
    }
    
    return TRUE;
}


static bool floatws_manage_clientwin(WFloatWS *ws, WClientWin *cwin,
                                     const WManageParams *param)
{
    return floatws_do_manage_clientwin(ws, cwin, param, TRUE);
}


static bool floatws_handle_drop(WFloatWS *ws, int x, int y,
                                WRegion *dropped)
{
    WRectangle fgeom;
    WFloatFrame *target;
    WWindow *par;
    
    par=REGION_PARENT_CHK(ws, WWindow);
    
    if(par==NULL)
        return FALSE;
    
    fgeom=REGION_GEOM(dropped);
    managed_to_floatframe_geom(ROOTWIN_OF(ws), &fgeom);
    /* The x and y arguments are in root coordinate space */
    region_rootpos((WRegion*)par, &fgeom.x, &fgeom.y);
    fgeom.x=x-fgeom.x;
    fgeom.y=y-fgeom.y;

    target=create_floatframe(par, &fgeom);
    
    if(target==NULL){
        warn("Failed to create a new WFloatFrame.");
        return FALSE;
    }
    
    if(!mplex_attach_simple((WMPlex*)target, dropped, TRUE)){
        destroy_obj((WObj*)target);
        warn("Failed to attach dropped region to created WFloatFrame");
        return FALSE;
    }
    
    floatws_add_managed(ws, (WRegion*)target);

    return TRUE;
}

/*EXTL_DOC
 * Attach client window \var{cwin} on \var{ws}.
 * At least the following fields in \var{p} are supported:
 * 
 * \begin{tabularx}{\linewidth}{lX}
 *  \hline
 *  Field & Description \\
 *  \hline
 *  \var{switchto} & Should the region be switched to (boolean)? Optional. \\
 *  \var{geom} & Geometry; \var{x} and \var{y}, if set, indicates top-left of 
 *   the frame to be created while \var{width} and \var{height}, if set, indicate
 *   the size of the client window within that frame. Optional.
 * \end{tabularx}
 */
EXTL_EXPORT_MEMBER
bool floatws_attach(WFloatWS *ws, WClientWin *cwin, ExtlTab p)
{
    int posok=0;
    WManageParams param=INIT_WMANAGEPARAMS;
    ExtlTab g;
    
    param.gravity=ForgetGravity;
    param.geom.w=REGION_GEOM(cwin).w;
    param.geom.h=REGION_GEOM(cwin).h;
    
    extl_table_gets_b(p, "switchto", &(param.switchto));
    
    if(extl_table_gets_t(p, "geom", &g)){
        if(extl_table_gets_i(g, "x", &(param.geom.x)))
            posok++;
        if(extl_table_gets_i(g, "y", &(param.geom.y)))
            posok++;
    
        extl_table_gets_i(p, "w", &(param.geom.w));
        extl_table_gets_i(p, "h", &(param.geom.h));
        
        extl_unref_table(g);
    }
    
    param.geom.w=maxof(0, param.geom.w);
    param.geom.h=maxof(0, param.geom.h);

    /*if(posok==2)
        param.userpos=TRUE;*/
    
    return floatws_do_manage_clientwin(ws, cwin, &param, posok==2);
}


/* A handler for add_clientwin_alt hook to handle transients for windows
 * on WFloatWS:s differently from the normal behaviour.
 */
bool add_clientwin_floatws_transient(WClientWin *cwin, 
                                     const WManageParams *param)
{
    WRegion *stack_above;
    WFloatWS *ws;
    
    if(param->tfor==NULL)
        return FALSE;

    stack_above=(WRegion*)REGION_PARENT_CHK(param->tfor, WFloatFrame);
    if(stack_above==NULL)
        return FALSE;
    
    ws=REGION_MANAGER_CHK(stack_above, WFloatWS);
    if(ws==NULL)
        return FALSE;
    
    if(!floatws_manage_clientwin(ws, cwin, param))
        return FALSE;

    region_stack_above(REGION_MANAGER(cwin), stack_above);

    return TRUE;
}


/*}}}*/


/*{{{ Circulate */


/*EXTL_DOC
 * Activate next object on \var{ws}.
 */
EXTL_EXPORT_MEMBER
WRegion *floatws_circulate(WFloatWS *ws)
{
    WRegion *r=NEXT_MANAGED_WRAP(ws->managed_list, ws->current_managed);
    if(r!=NULL)
        region_goto(r);
    return r;
}


/*EXTL_DOC
 * Activate previous object on \var{ws}.
 */
EXTL_EXPORT_MEMBER
WRegion *floatws_backcirculate(WFloatWS *ws)
{
    WRegion *r=PREV_MANAGED_WRAP(ws->managed_list, ws->current_managed);
    if(r!=NULL)
        region_goto(r);
    return r;
}


/*}}}*/


/*{{{ Misc. */


/*EXTL_DOC
 * Returns a list of regions managed by the workspace (frames, mostly).
 */
EXTL_EXPORT_MEMBER
ExtlTab floatws_managed_list(WFloatWS *ws)
{
    return managed_list_to_table(ws->managed_list, NULL);
}


/*EXTL_DOC
 * Returns the object that currently has or previously had focus on \var{ws}
 * (if no other object on the workspace currently has focus).
 */
EXTL_EXPORT_MEMBER
WRegion* floatws_current(WFloatWS *ws)
{
    return ws->current_managed;
}


/*}}}*/


/*{{{ Save/load */


static bool floatws_save_to_file(WFloatWS *ws, FILE *file, int lvl)
{
    WRegion *mgd;
    
    begin_saved_region((WRegion*)ws, file, lvl);
    save_indent_line(file, lvl);
    fprintf(file, "managed = {\n");
    FOR_ALL_MANAGED_ON_LIST(ws->managed_list, mgd){
        save_indent_line(file, lvl+1);
        fprintf(file, "{\n");
        if(region_save_to_file((WRegion*)mgd, file, lvl+2))
            save_geom(&REGION_GEOM(mgd), file, lvl+2);
        save_indent_line(file, lvl+1);
        fprintf(file, "},\n");
    }
    save_indent_line(file, lvl);
    fprintf(file, "},\n");
    
    return TRUE;
}


WRegion *floatws_load(WWindow *par, const WRectangle *geom, ExtlTab tab)
{
    WFloatWS *ws;
    ExtlTab substab, subtab;
    int i, n;
    
    ws=create_floatws(par, geom);
    
    if(ws==NULL)
        return NULL;
        
    if(!extl_table_gets_t(tab, "managed", &substab))
        return (WRegion*)ws;

    n=extl_table_get_n(substab);
    for(i=1; i<=n; i++){
        if(extl_table_geti_t(substab, i, &subtab)){
            floatws_attach_load(ws, subtab);
            extl_unref_table(subtab);
        }
    }
    
    extl_unref_table(substab);

    return (WRegion*)ws;
}


/*}}}*/


/*{{{ Dynamic function table and class implementation */


static DynFunTab floatws_dynfuntab[]={
    {region_fit, 
     floatws_fit},
    {(DynFun*)region_reparent,
     (DynFun*)reparent_floatws},

    {region_map, 
     floatws_map},
    {region_unmap, 
     floatws_unmap},
    {(DynFun*)region_display_managed, 
     (DynFun*)floatws_display_managed},

    {region_set_focus_to, 
     floatws_set_focus_to},
    {region_managed_activated, 
     floatws_managed_activated},
    
    {(DynFun*)region_manage_clientwin, 
     (DynFun*)floatws_manage_clientwin},
    {(DynFun*)region_handle_drop,
     (DynFun*)floatws_handle_drop},
    {region_remove_managed,
     floatws_remove_managed},
    
    {(DynFun*)region_save_to_file, 
     (DynFun*)floatws_save_to_file},

    {(DynFun*)region_x_window,
     (DynFun*)floatws_x_window},

    {region_close,
     floatws_close},

    {(DynFun*)region_current,
     (DynFun*)floatws_current},
    
    {(DynFun*)region_do_rescue_clientwins,
     (DynFun*)floatws_do_rescue_clientwins},
    
    END_DYNFUNTAB
};


IMPLOBJ(WFloatWS, WGenWS, floatws_deinit, floatws_dynfuntab);


/*}}}*/

