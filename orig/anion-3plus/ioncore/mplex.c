/*
 * ion/ioncore/mplex.c
 *
 * Copyright (c) Tuomo Valkonen 1999-2009. 
 *
 * See the included file LICENSE for details.
 */

#include <limits.h>
#include <string.h>

#include <libtu/objp.h>
#include <libtu/minmax.h>
#include <libtu/rb.h>
#include <libextl/extl.h>
#include <libmainloop/defer.h>

#include "common.h"
#include "window.h"
#include "global.h"
#include "rootwin.h"
#include "focus.h"
#include "event.h"
#include "attach.h"
#include "manage.h"
#include "resize.h"
#include "tags.h"
#include "sizehint.h"
#include "extlconv.h"
#include "frame-pointer.h"
#include "bindmaps.h"
#include "regbind.h"
#include "saveload.h"
#include "xwindow.h"
#include "mplexpholder.h"
#include "grouppholder.h"
#include "llist.h"
#include "names.h"
#include "sizepolicy.h"
#include "stacking.h"
#include "group.h"
#include "navi.h"


#define SUBS_MAY_BE_MAPPED(MPLEX) \
    (REGION_IS_MAPPED(MPLEX) && !MPLEX_MGD_UNVIEWABLE(MPLEX))

#define PASSIVE(ST) ((ST)->level<=STACKING_LEVEL_MODAL1          \
                     && ((ST)->reg==NULL                         \
                         || (ST)->reg->flags&REGION_SKIP_FOCUS))

#define CAN_MANAGE_STDISP(REG) HAS_DYN(REG, region_manage_stdisp)


/*{{{ Stacking list stuff */


WStacking *mplex_get_stacking(WMPlex *mplex)
{
    return window_get_stacking(&mplex->win);
}


WStacking **mplex_get_stackingp(WMPlex *mplex)
{
    return window_get_stackingp(&mplex->win);
}


void mplex_iter_init(WMPlexIterTmp *tmp, WMPlex *mplex)
{
    stacking_iter_mgr_init(tmp, mplex->mgd, NULL, mplex);
}


WRegion *mplex_iter(WMPlexIterTmp *tmp)
{
    return stacking_iter_mgr(tmp);
}


WStacking *mplex_iter_nodes(WMPlexIterTmp *tmp)
{
    return stacking_iter_mgr_nodes(tmp);
}


/*}}}*/


/*{{{ Destroy/create mplex */


bool mplex_do_init(WMPlex *mplex, WWindow *parent, 
                   const WFitParams *fp, Window win)
{
    mplex->flags=0;
    
    mplex->mx_list=NULL;
    mplex->mx_current=NULL;
    mplex->misc_phs=NULL;
    mplex->mx_count=0;
    
    mplex->mgd=NULL;
    
    watch_init(&(mplex->stdispwatch));
    mplex->stdispinfo.pos=MPLEX_STDISP_BL;
    mplex->stdispinfo.fullsize=FALSE;
    
    if(!window_do_init((WWindow*)mplex, parent, fp, win))
        return FALSE;
    
    mplex->win.region.flags|=REGION_BINDINGS_ARE_GRABBED;
    
    window_select_input(&(mplex->win), IONCORE_EVENTMASK_CWINMGR);
    
    region_register((WRegion*)mplex);
    
    /* Call this to set MPLEX_MANAGED_UNVIEWABLE if necessary. */
    mplex_fit_managed(mplex);
    
    return TRUE;
}


bool mplex_init(WMPlex *mplex, WWindow *parent, const WFitParams *fp)
{
    return mplex_do_init(mplex, parent, fp, None);
}


WMPlex *create_mplex(WWindow *parent, const WFitParams *fp)
{
    CREATEOBJ_IMPL(WMPlex, mplex, (p, parent, fp));
}


void mplex_deinit(WMPlex *mplex)
{
    WMPlexIterTmp tmp;
    WRegion *reg;
    
    FOR_ALL_MANAGED_BY_MPLEX(mplex, reg, tmp){
        destroy_obj((Obj*)reg);
    }
    
    assert(mplex->mgd==NULL);
    assert(mplex->mx_list==NULL);

    while(mplex->misc_phs!=NULL){
        assert(mplexpholder_move(mplex->misc_phs, NULL, NULL, NULL));
    }
    
    window_deinit((WWindow*)mplex);
}


/*}}}*/


/*{{{ Node lookup etc. */


WStacking *mplex_find_stacking(WMPlex *mplex, WRegion *reg)
{
    WStacking *st;
    
    /* Some routines that call us expect us to this check. */
    if(reg==NULL || REGION_MANAGER(reg)!=(WRegion*)mplex)
        return NULL;
    
    st=ioncore_find_stacking(reg);

    assert(st==NULL || st->mgr_prev!=NULL);
    
    return st;
}


WStacking *mplex_current_node(WMPlex *mplex)
{
    WStacking *st=NULL;
    WRegion *reg;
    
    reg=REGION_ACTIVE_SUB(mplex);
    reg=region_managed_within((WRegion*)mplex, reg);
    if(reg!=NULL)
        st=mplex_find_stacking(mplex, reg);

    if(st!=NULL)
        return st;
    else
        return (mplex->mx_current!=NULL ? mplex->mx_current->st : NULL);
}


WRegion *mplex_current(WMPlex *mplex)
{
    WStacking *node=mplex_current_node(mplex);
    return (node==NULL ? NULL : node->reg);
}


/*}}}*/


/*{{{ Exclusive list management and exports */

/*EXTL_DOC
 * Returns the number of objects on the mutually exclusive list of \var{mplex}.
 */
EXTL_SAFE
EXTL_EXPORT_MEMBER
int mplex_mx_count(WMPlex *mplex)
{
    return mplex->mx_count;
}


/*EXTL_DOC
 * Returns the managed object currently active within the mutually exclusive
 * list of \var{mplex}.
 */
EXTL_SAFE
EXTL_EXPORT_MEMBER
WRegion *mplex_mx_current(WMPlex *mplex)
{
    WLListNode *lnode=mplex->mx_current;
    return (lnode==NULL ? NULL : lnode->st->reg);
}


/*EXTL_DOC
 * Returns the \var{n}:th object on the mutually exclusive
 * list of \var{mplex}.
 */
EXTL_SAFE
EXTL_EXPORT_MEMBER
WRegion *mplex_mx_nth(WMPlex *mplex, uint n)
{
    WLListNode *lnode=llist_nth_node(mplex->mx_list, n);
    return (lnode==NULL ? NULL : lnode->st->reg);
}


/*EXTL_DOC
 * Iterate over numbered/mutually exclusive region list of \var{mplex} 
 * until \var{iterfn} returns \code{false}.
 * The function is called in protected mode.
 * This routine returns \code{true} if it reaches the end of list
 * without this happening.
 */
EXTL_SAFE
EXTL_EXPORT_MEMBER
bool mplex_mx_i(WMPlex *mplex, ExtlFn iterfn)
{
    WLListIterTmp tmp;
    llist_iter_init(&tmp, mplex->mx_list);
    
    return extl_iter_objlist_(iterfn, (ObjIterator*)llist_iter_regions, &tmp);
}


/*EXTL_DOC
 * Iterate over managed regions of \var{mplex} until \var{iterfn} returns
 * \code{false}.
 * The function is called in protected mode.
 * This routine returns \code{true} if it reaches the end of list
 * without this happening.
 */
EXTL_SAFE
EXTL_EXPORT_MEMBER
bool mplex_managed_i(WMPlex *mplex, ExtlFn iterfn)
{
    WMPlexIterTmp tmp;
    mplex_iter_init(&tmp, mplex);
    
    return extl_iter_objlist_(iterfn, (ObjIterator*)mplex_iter, &tmp);
}


/*EXTL_DOC
 * Set index of \var{reg} to \var{index} within the mutually exclusive 
 * list of \var{mplex}. Special values for \var{index} are:
 * \begin{tabularx}{\linewidth}{lX}
 *   $-1$ & Last. \\
 *   $-2$ & After \fnref{WMPlex.mx_current}. \\
 * \end{tabularx}
 */
EXTL_EXPORT_MEMBER
void mplex_set_index(WMPlex *mplex, WRegion *reg, int index)
{
    WLListNode *lnode, *after;
    WStacking *node;
    
    node=mplex_find_stacking(mplex, reg);

    if(node==NULL)
        return;
    
    lnode=node->lnode;
    
    if(lnode==NULL){
        lnode=ALLOC(WLListNode);
        if(lnode==NULL)
            return;
        lnode->next=NULL;
        lnode->prev=NULL;
        lnode->phs=NULL;
        lnode->st=node;
        node->lnode=lnode;
        mplex->mx_count++;
    }else{
        mplex_move_phs_before(mplex, lnode);
        llist_unlink(&(mplex->mx_list), lnode);
    }
    
    after=llist_index_to_after(mplex->mx_list, mplex->mx_current, index);
    llist_link_after(&(mplex->mx_list), after, lnode);
    mplex_managed_changed(mplex, MPLEX_CHANGE_REORDER, FALSE, reg);
}


/*EXTL_DOC
 * Get index of \var{reg} on the mutually exclusive list of \var{mplex}.
 * The indices begin from zero.. If \var{reg} is not on the list,
 * -1 is returned.
 */
EXTL_SAFE
EXTL_EXPORT_MEMBER
int mplex_get_index(WMPlex *mplex, WRegion *reg)
{
    WLListIterTmp tmp;
    WLListNode *lnode;
    int index=0;
    
    FOR_ALL_NODES_ON_LLIST(lnode, mplex->mx_list, tmp){
        if(reg==lnode->st->reg)
            return index;
        index++;
    }
    
    return -1;
}


/*EXTL_DOC
 * Move \var{r} ``right'' within objects managed by \var{mplex} on list 1.
 */
EXTL_EXPORT_MEMBER
void mplex_inc_index(WMPlex *mplex, WRegion *r)
{
    if(r==NULL)
        r=mplex_mx_current(mplex);
    if(r!=NULL)
        mplex_set_index(mplex, r, mplex_get_index(mplex, r)+1);
}


/*EXTL_DOC
 * Move \var{r} ``left'' within objects managed by \var{mplex} on list 1.
 */
EXTL_EXPORT_MEMBER
void mplex_dec_index(WMPlex *mplex, WRegion *r)
{
    if(r==NULL)
        r=mplex_mx_current(mplex);
    if(r!=NULL)
        mplex_set_index(mplex, r, mplex_get_index(mplex, r)-1);
}


/*}}}*/


/*{{{ Mapping */


static void mplex_map_mgd(WMPlex *mplex)
{
    WMPlexIterTmp tmp;
    WStacking *node;

    FOR_ALL_NODES_IN_MPLEX(mplex, node, tmp){
        if(!STACKING_IS_HIDDEN(node))
            region_map(node->reg);
    }
}


static void mplex_unmap_mgd(WMPlex *mplex)
{
    WMPlexIterTmp tmp;
    WStacking *node;

    FOR_ALL_NODES_IN_MPLEX(mplex, node, tmp){
        if(!STACKING_IS_HIDDEN(node))
            region_unmap(node->reg);
    }
}



void mplex_map(WMPlex *mplex)
{
    window_map((WWindow*)mplex);
    /* A lame requirement of the ICCCM is that client windows should be
     * unmapped if the parent is unmapped.
     */
    if(!MPLEX_MGD_UNVIEWABLE(mplex))
        mplex_map_mgd(mplex);
}


void mplex_unmap(WMPlex *mplex)
{
    window_unmap((WWindow*)mplex);
    /* A lame requirement of the ICCCM is that client windows should be
     * unmapped if the parent is unmapped.
     */
    if(!MPLEX_MGD_UNVIEWABLE(mplex))
        mplex_unmap_mgd(mplex);
}


/*}}}*/


/*{{{ Resize and reparent */


bool mplex_fitrep(WMPlex *mplex, WWindow *par, const WFitParams *fp)
{
    bool wchg=(REGION_GEOM(mplex).w!=fp->g.w);
    bool hchg=(REGION_GEOM(mplex).h!=fp->g.h);
    
    if(!window_fitrep(&(mplex->win), par, fp))
        return FALSE;
    
    if(wchg || hchg){
        mplex_fit_managed(mplex);
        mplex_size_changed(mplex, wchg, hchg);
    }
    
    return TRUE;
}


void mplex_do_fit_managed(WMPlex *mplex, WFitParams *fp)
{
    WRectangle geom;
    WMPlexIterTmp tmp;
    WStacking *node;
    WFitParams fp2;
    
    if(!MPLEX_MGD_UNVIEWABLE(mplex) && (fp->g.w<=1 || fp->g.h<=1)){
        mplex->flags|=MPLEX_MANAGED_UNVIEWABLE;
        if(REGION_IS_MAPPED(mplex))
            mplex_unmap_mgd(mplex);
    }else if(MPLEX_MGD_UNVIEWABLE(mplex) && !(fp->g.w<=1 || fp->g.h<=1)){
        mplex->flags&=~MPLEX_MANAGED_UNVIEWABLE;
        if(REGION_IS_MAPPED(mplex))
            mplex_map_mgd(mplex);
    }
    
    if(!MPLEX_MGD_UNVIEWABLE(mplex)){
        FOR_ALL_NODES_IN_MPLEX(mplex, node, tmp){
            fp2=*fp;
            sizepolicy(&node->szplcy, node->reg, NULL, 0, &fp2);
            region_fitrep(node->reg, NULL, &fp2);
        }
    }
}


void mplex_fit_managed(WMPlex *mplex)
{
    WFitParams fp;
    
    fp.mode=REGION_FIT_EXACT;
    mplex_managed_geom(mplex, &(fp.g));

    mplex_do_fit_managed(mplex, &fp);
}


static void mplex_managed_rqgeom(WMPlex *mplex, WRegion *sub,
                                 const WRQGeomParams *rq,
                                 WRectangle *geomret)
{
    WRectangle rg;
    WFitParams fp;
    WStacking *node;

    node=mplex_find_stacking(mplex, sub);
    
    assert(node!=NULL);
    
    fp.mode=0;
    mplex_managed_geom(mplex, &fp.g);
    
    sizepolicy(&node->szplcy, sub, &rq->geom, rq->flags, &fp);
    
    if(geomret!=NULL)
        *geomret=fp.g;
    
    if(!(rq->flags&REGION_RQGEOM_TRYONLY))
        region_fitrep(sub, NULL, &fp);
}


void mplex_set_szplcy(WMPlex *mplex, WRegion *sub, WSizePolicy szplcy)
{
    WStacking *node;

    node=mplex_find_stacking(mplex, sub);
    
    if(node!=NULL)
        node->szplcy=szplcy;
}


WSizePolicy mplex_get_szplcy(WMPlex *mplex, WRegion *sub)
{
    WStacking *node;

    node=mplex_find_stacking(mplex, sub);
    
    return (node==NULL ? SIZEPOLICY_DEFAULT : node->szplcy);
}


/*}}}*/


/*{{{ Focus  */


typedef struct{
    WMPlex *mplex;
    WStacking *to_try;
    WStacking *group_st;
    PtrList **hidelist;
    bool try_hard;
} FiltData;


static WRegion *manager_within(WMPlex *mplex, WStacking *st)
{
    return region_managed_within((WRegion*)mplex, st->reg);
}


static WStacking *stacking_within(WMPlex *mplex, WStacking *st)
{
    WRegion *reg=manager_within(mplex, st);
    
    return (reg==NULL 
            ? NULL
            : (reg==st->reg
               ? st
               : ioncore_find_stacking(reg)));
}


/* Mutually exclusive regions can't be pseudomodal */
#define IS_PSEUDOMODAL(ST) ((ST)->lnode==NULL && (ST)->pseudomodal)


static bool mapped_pseudomodal_include_filt(WStacking *st, void *data_)
{
    FiltData *data=(FiltData*)data_;
    WStacking *stw;
    
    if(st->reg==NULL || !REGION_IS_MAPPED(st->reg))
        return FALSE;
        
    if(!data->hidelist
       || (data->to_try==NULL && data->group_st==NULL) 
       || st->level<STACKING_LEVEL_MODAL1){
        return TRUE;
    }
    
    /* Ok, modal node in the way. Let's see if it is pseudomodal
     * and can be hidden.
     */
    
    stw=stacking_within(data->mplex, st);
    
    /* This should not happen */
    if(stw==NULL || stw->reg==NULL)
        return FALSE;
    
    /* The node is within the same group, so it can not be hidden. 
     * Latter case should not happen.
     */
    if(stw==data->group_st || stw==data->to_try)
        return TRUE;
    
    if(IS_PSEUDOMODAL(stw)){
        /* Don't insert multiple times. */
        return !ptrlist_reinsert_first(data->hidelist, stw);
    }
        
    return TRUE;
}


static bool mgr_pseudomodal_approve_filt(WStacking *st, void *data_)
{
    FiltData *data=(FiltData*)data_;
    
    return (data->group_st==NULL || st==data->group_st ||
            manager_within(data->mplex, st)==data->group_st->reg);
}


WStacking *mplex_find_to_focus(WMPlex *mplex,
                               WStacking *to_try,
                               WStacking *group_st,
                               PtrList **hidelist)
{
    WStackingFilter *fi=mapped_pseudomodal_include_filt;
    WStackingFilter *fa=mgr_pseudomodal_approve_filt;
    WStacking *stacking=mplex_get_stacking(mplex);
    FiltData data;
    WStacking *st;
    
    if(stacking==NULL)
        return NULL;
    
    if(to_try!=NULL && (to_try->reg==NULL || !REGION_IS_MAPPED(to_try->reg)))
        to_try=NULL;
    
    data.mplex=mplex;
    data.to_try=to_try;
    data.group_st=group_st;
    data.hidelist=hidelist;
    
    st=stacking_find_to_focus(stacking, to_try, fi, fa, &data);
    
    if(st==NULL && hidelist!=NULL)
        ptrlist_clear(hidelist);
    
    return st;
}


static WStacking *mplex_do_to_focus_on(WMPlex *mplex, WStacking *node,
                                       WStacking *to_try, 
                                       PtrList **hidelist, bool *within)
{
    WGroup *grp=OBJ_CAST(node->reg, WGroup);
    WStacking *st;
    
    if(grp!=NULL){
        if(to_try==NULL)
            to_try=grp->current_managed;
        /* Only will return stuff within 'node' */
        st=mplex_find_to_focus(mplex, to_try, node, hidelist);
        if(st!=NULL){
            if(within!=NULL)
                *within=TRUE;
            return st;
        }
    }
    
    st=mplex_find_to_focus(mplex, node, NULL, hidelist);
    
    /* If 'node' points to a group, it isn't actually on the stacking list. 
     * Give it the focus, if there's nothing "proper" that could be focussed.
     */
    if(st==NULL && grp!=NULL && REGION_IS_MAPPED(grp))
        st=node;
    
    if(st==node && within!=NULL)
        *within=TRUE;
        
    return st;
}


static WStacking *maybe_focusable(WRegion *reg)
{
    if(reg==NULL || !REGION_IS_MAPPED(reg))
        return NULL;

    return ioncore_find_stacking(reg);
}


static WStacking *has_stacking_within(WMPlex *mplex, WRegion *reg)
{
    while(reg!=NULL && REGION_MANAGER(reg)!=(WRegion*)mplex)
        reg=REGION_MANAGER(reg);
                
    return maybe_focusable(reg);
}


/* 1. Try keep focus in REGION_ACTIVE_SUB.
 * 2. Choose something else, attempting previous in focus history.
 */
static WStacking *mplex_to_focus(WMPlex *mplex)
{
    WStacking *foc=NULL, *fallback=NULL;
    WRegion *reg=NULL;
    
    foc=maybe_focusable(REGION_ACTIVE_SUB(mplex));
    
    if(foc==NULL){
        /* Search focus history if no specific attempt set.*/
        for(reg=ioncore_g.focus_current; reg!=NULL; reg=reg->active_next){
            foc=has_stacking_within(mplex, reg);
            if(foc!=NULL)
                break;
        }
    }
    
    if(foc!=NULL){
        /* In the history search case, 'foc' might point to a group,
         * since we don't properly try to find a stacking within it...
         */
        return mplex_do_to_focus_on(mplex, foc, NULL, NULL, NULL);
    }else{
        return mplex_find_to_focus(mplex, NULL, NULL, NULL);
    }
}


void mplex_do_set_focus(WMPlex *mplex, bool warp)
{
    if(!MPLEX_MGD_UNVIEWABLE(mplex)){
        WStacking *st=mplex_to_focus(mplex);
        
        if(st==NULL){
            st=(mplex->mx_current!=NULL
                ? mplex->mx_current->st
                : NULL);
        }
        
        if(st!=NULL){
            region_do_set_focus(st->reg, warp);
            return;
        }
    }
    
    window_do_set_focus((WWindow*)mplex, warp);
}


static void mplex_refocus(WMPlex *mplex, WStacking *node, bool warp)
{
    bool within=FALSE;
    WStacking *foc=NULL;
    
    if(node!=NULL)
        foc=mplex_do_to_focus_on(mplex, node, NULL, NULL, &within);
        
    if(foc==NULL || !within)
        foc=mplex_to_focus(mplex);
    
    if(foc!=NULL)
        region_maybewarp(foc->reg, warp);
}


/*}}}*/


/*{{{ Switch */


static void mplex_do_remanage_stdisp(WMPlex *mplex, WRegion *sub)
{
    WRegion *stdisp=(WRegion*)(mplex->stdispwatch.obj);

    /* Move stdisp */
    if(sub!=NULL && CAN_MANAGE_STDISP(sub)){
        if(stdisp!=NULL){
            WRegion *omgr=REGION_MANAGER(stdisp);
            if(omgr!=sub && omgr!=NULL){
                if(CAN_MANAGE_STDISP(omgr))
                    region_unmanage_stdisp(omgr, FALSE, FALSE);
                region_detach_manager(stdisp);
            }
            
            region_manage_stdisp(sub, stdisp, 
                                 &(mplex->stdispinfo));
        }else{
            region_unmanage_stdisp(sub, TRUE, FALSE);
        }
    }
}


void mplex_remanage_stdisp(WMPlex *mplex)
{
    mplex_do_remanage_stdisp(mplex, (mplex->mx_current!=NULL
                                     ? mplex->mx_current->st->reg
                                     : NULL));
}


static void mplex_do_node_display(WMPlex *mplex, WStacking *node,
                                  bool call_changed)
{
    WRegion *sub=node->reg;
    WLListNode *mxc=mplex->mx_current;

    if(!STACKING_IS_HIDDEN(node))
        return;
    
    if(node->lnode!=NULL && node->lnode!=mxc)
        mplex_do_remanage_stdisp(mplex, sub);

    node->hidden=FALSE;
    
    if(SUBS_MAY_BE_MAPPED(mplex))
        region_map(sub);
    else
        region_unmap(sub);
    
    if(node->lnode!=NULL){
        if(mxc!=NULL){
            /* Hide current mx region. We do it after mapping the
             * new one to avoid flicker.
             */
            if(REGION_IS_MAPPED(mplex))
                region_unmap(mxc->st->reg);
            mxc->st->hidden=TRUE;
        }
        
        mplex->mx_current=node->lnode;
        
        /* Ugly hack:
         * Many programs will get upset if the visible, although only 
         * such, client window is not the lowest window in the mplex. 
         * xprop/xwininfo will return the information for the lowest 
         * window. 'netscape -remote' will not work at all if there are
         * no visible netscape windows.
         */
        {
            WGroup *grp=(WGroup*)OBJ_CAST(sub, WGroupCW);
            if(grp!=NULL){
                WRegion *bottom=group_bottom(grp);
                if(bottom!=NULL){
                    region_managed_rqorder((WRegion*)grp, bottom,
                                           REGION_ORDER_BACK);
                }
            }
        }
        
        if(call_changed)
            mplex_managed_changed(mplex, MPLEX_CHANGE_SWITCHONLY, TRUE, sub);
    }
}


bool mplex_do_prepare_focus(WMPlex *mplex, WStacking *node,
                            WStacking *sub, int flags, 
                            WPrepareFocusResult *res)
{
    bool ew=(flags&REGION_GOTO_ENTERWINDOW);
    PtrList *hidelist=NULL;
    PtrList **hidelistp=(ew ? NULL : &hidelist);
    WStacking *foc;
    /*bool within=FALSE;*/
    
    if(sub==NULL && node==NULL)
        return FALSE;
    
    /* Display the node in any case */
    if(node!=NULL && !ew)
        mplex_do_node_display(mplex, node, TRUE);
    
    if(!region_prepare_focus((WRegion*)mplex, flags, res))
        return FALSE;

    foc=mplex_do_to_focus_on(mplex, node, sub, hidelistp, NULL /*&within*/);

    if(foc!=NULL){
        while(hidelist!=NULL){
            WStacking *st=(WStacking*)ptrlist_take_first(&hidelist);
            st->hidden=TRUE;
            region_unmap(st->reg);
        }
        
        if(ioncore_g.autoraise && 
           !(flags&REGION_GOTO_ENTERWINDOW) &&
           foc->level>STACKING_LEVEL_BOTTOM){
            WStacking **stackingp=mplex_get_stackingp(mplex);
            stacking_restack(stackingp, foc, None, NULL, NULL, FALSE);
        }
        
        res->reg=foc->reg;
        res->flags=flags;
        
        return (foc==sub || (sub==NULL && foc==node));
    }else{
        return FALSE;
    }
}


bool mplex_managed_prepare_focus(WMPlex *mplex, WRegion *disp, 
                                 int flags, WPrepareFocusResult *res)
{
    WStacking *node=mplex_find_stacking(mplex, disp);
    
    if(node==NULL)
        return FALSE;
    else
        return mplex_do_prepare_focus(mplex, node, NULL, flags, res);
}


/*}}}*/


/*{{{ Switch exports */


static void do_switch(WMPlex *mplex, WLListNode *lnode)
{
    WStacking *node=(lnode!=NULL ? lnode->st : NULL);
    
    if(node!=NULL){
        bool mcf=region_may_control_focus((WRegion*)mplex);
    
        mplex_do_node_display(mplex, node, TRUE);
        
        if(mcf)
            mplex_refocus(mplex, node, TRUE);
    }
}


/*EXTL_DOC
 * Have \var{mplex} display the \var{n}:th object managed by it.
 */
EXTL_EXPORT_MEMBER
void mplex_switch_nth(WMPlex *mplex, uint n)
{
    do_switch(mplex, llist_nth_node(mplex->mx_list, n));
}


/*EXTL_DOC
 * Have \var{mplex} display next (wrt. currently selected) object managed 
 * by it.
 */
EXTL_EXPORT_MEMBER
void mplex_switch_next(WMPlex *mplex)
{
    do_switch(mplex, LIST_NEXT_WRAP(mplex->mx_list, mplex->mx_current, 
                                    next, prev));
}


/*EXTL_DOC
 * Have \var{mplex} display previous (wrt. currently selected) object
 * managed by it.
 */
EXTL_EXPORT_MEMBER
void mplex_switch_prev(WMPlex *mplex)
{
    do_switch(mplex, LIST_PREV_WRAP(mplex->mx_list, mplex->mx_current,
                                    next, prev));
}


bool mplex_set_hidden(WMPlex *mplex, WRegion *reg, int sp)
{
    bool mcf=region_may_control_focus((WRegion*)mplex);
    WStacking *node=mplex_find_stacking(mplex, reg);
    bool hidden, nhidden;
    
    if(node==NULL)
        return FALSE;
    
    hidden=STACKING_IS_HIDDEN(node);
    nhidden=libtu_do_setparam(sp, hidden);
    
    if(!hidden && nhidden){
        node->hidden=TRUE;
        
        if(REGION_IS_MAPPED(mplex) && !MPLEX_MGD_UNVIEWABLE(mplex))
            region_unmap(reg);
        
        /* lnode -> switch next? */
    }else if(hidden && !nhidden){
        mplex_do_node_display(mplex, node, TRUE);
    }
    
    if(mcf && !PASSIVE(node))
        mplex_refocus(mplex, (nhidden ? NULL : node), TRUE);
    
    return STACKING_IS_HIDDEN(node);
}


/*EXTL_DOC
 * Set the visibility of the region \var{reg} on \var{mplex}
 * as specified with the parameter \var{how} 
 * (one of \codestr{set}, \codestr{unset}, or \codestr{toggle}).
 * The resulting state is returned.
 */
EXTL_EXPORT_AS(WMPlex, set_hidden)
bool mplex_set_hidden_extl(WMPlex *mplex, WRegion *reg, const char *how)
{
    return mplex_set_hidden(mplex, reg, libtu_string_to_setparam(how));
}


/*EXTL_DOC
 * Is \var{reg} on within \var{mplex} and hidden?
 */
EXTL_SAFE
EXTL_EXPORT_MEMBER
bool mplex_is_hidden(WMPlex *mplex, WRegion *reg)
{
    WStacking *node=mplex_find_stacking(mplex, reg);
    
    return (node!=NULL && STACKING_IS_HIDDEN(node));
}


/*}}}*/


/*{{{ Navigation */


static WStacking *mplex_nxt(WMPlex *mplex, WStacking *st, bool wrap)
{
    return (st->mgr_next!=NULL 
            ? st->mgr_next 
            : (wrap ? mplex->mgd : NULL));
}


static WStacking *mplex_prv(WMPlex *mplex, WStacking *st, bool wrap)
{
    return (st!=mplex->mgd
            ? st->mgr_prev
            : (wrap ? st->mgr_prev : NULL));
}


typedef WStacking *NxtFn(WMPlex *mplex, WStacking *st, bool wrap);


static WRegion *do_navi(WMPlex *mplex, WStacking *sti, 
                        NxtFn *fn, WRegionNaviData *data, 
                        bool sti_ok, bool wrap)
{
    WStacking *st, *stacking;
    uint min_level=0;
    
    stacking=mplex_get_stacking(mplex);
    
    if(stacking!=NULL)
        min_level=stacking_min_level_mapped(stacking);

    st=sti;
    while(1){
        st=fn(mplex, st, wrap); 
        
        if(st==NULL || (st==sti && !sti_ok))
            break;
        
        if(!st->hidden){
            if(OBJ_IS(st->reg, WGroup)){
                /* WGroup navigation code should respect modal stuff. */
                WRegion *res=region_navi_cont((WRegion*)mplex, st->reg, data);
                if(res!=NULL && res!=st->reg)
                    return res;
            }else{
                if(st->level>=min_level && !PASSIVE(st))
                    return region_navi_cont((WRegion*)mplex, st->reg, data);
            }
        }
	        
        if(st==sti)
            break;
    }
    
    return NULL;
}
                        

WRegion *mplex_navi_first(WMPlex *mplex, WRegionNavi nh,
                          WRegionNaviData *data)
{
    WStacking *lst=mplex->mgd;
    WRegion *res=NULL;
    
    if(lst!=NULL){
        if(nh==REGION_NAVI_ANY){
            /* ? */
        }
    
        if(nh==REGION_NAVI_ANY || nh==REGION_NAVI_END || 
           nh==REGION_NAVI_BOTTOM || nh==REGION_NAVI_RIGHT){
            res=do_navi(mplex, lst, mplex_prv, data, TRUE, TRUE);
        }else{
            res=do_navi(mplex, lst->mgr_prev, mplex_nxt, data, TRUE, TRUE);
        }
    }
    
    return region_navi_cont((WRegion*)mplex, res, data);
}

    
WRegion *mplex_navi_next(WMPlex *mplex, WRegion *rel, WRegionNavi nh,
                         WRegionNaviData *data)
{
    WStacking *st;
    WRegion *res;
    
    if(rel!=NULL){
        st=mplex_find_stacking(mplex, rel);
        if(st==NULL)
            return NULL;
    }else if(mplex->mx_current!=NULL){
        st=mplex->mx_current->st;
    }else{
        return mplex_navi_first(mplex, nh, data);
    }
    
    if(nh==REGION_NAVI_ANY){
        /* ? */
    }
    
    if(nh==REGION_NAVI_ANY || nh==REGION_NAVI_END || 
       nh==REGION_NAVI_BOTTOM || nh==REGION_NAVI_RIGHT){
        res=do_navi(mplex, st, mplex_nxt, data, FALSE, FALSE);
    }else{
        res=do_navi(mplex, st, mplex_prv, data, FALSE, FALSE);
    }
    
    return region_navi_cont((WRegion*)mplex, res, data);
}


/*}}}*/


/*{{{ Stacking */


bool mplex_managed_rqorder(WMPlex *mplex, WRegion *reg, WRegionOrder order)
{
    WStacking **stackingp=mplex_get_stackingp(mplex);
    WStacking *st;

    if(stackingp==NULL || *stackingp==NULL)
        return FALSE;

    st=mplex_find_stacking(mplex, reg);
    
    if(st==NULL)
        return FALSE;
    
    stacking_restack(stackingp, st, None, NULL, NULL,
                     (order!=REGION_ORDER_FRONT));
    
    return TRUE;
}


/*}}}*/


/*{{{ Attach */


static bool mplex_stack(WMPlex *mplex, WStacking *st)
{
    WStacking *tmp=NULL;
    WStacking **stackingp=mplex_get_stackingp(mplex);
    
    if(stackingp==NULL)
        return FALSE;

    LINK_ITEM_FIRST(tmp, st, next, prev);
    stacking_weave(stackingp, &tmp, FALSE);
    assert(tmp==NULL);
    
    return TRUE;
}


static void mplex_unstack(WMPlex *mplex, WStacking *st)
{
    WStacking *stacking;
    
    stacking=mplex_get_stacking(mplex);
    
    stacking_unstack(&mplex->win, st);
}


/* WMPlexWPHolder is used for position marking in order to allow
 * WLListNodes be safely removed in the attach handler hnd, that
 * could remove something this mplex is managing.
 */
bool mplex_do_attach_final(WMPlex *mplex, WRegion *reg, WMPlexPHolder *ph)
{
    WStacking *node=NULL;
    WLListNode *lnode=NULL;
    WMPlexAttachParams *param=&ph->param;
    bool mx_was_empty, sw, modal, mcf, hidden;
    WSizePolicy szplcy;
    uint level;
    
    mcf=region_may_control_focus((WRegion*)mplex);
    
    mx_was_empty=(mplex->mx_list==NULL);
    
    szplcy=((param->flags&MPLEX_ATTACH_SIZEPOLICY &&
             param->szplcy!=SIZEPOLICY_DEFAULT)
            ? param->szplcy
            : (param->flags&MPLEX_ATTACH_UNNUMBERED
               ? SIZEPOLICY_FULL_BOUNDS
               : SIZEPOLICY_FULL_EXACT));

    modal=(param->flags&MPLEX_ATTACH_LEVEL
           && param->level>=STACKING_LEVEL_MODAL1);
    
    level=(param->flags&MPLEX_ATTACH_LEVEL
           ? param->level
           : (param->flags&MPLEX_ATTACH_UNNUMBERED
              ? STACKING_LEVEL_NORMAL
              : STACKING_LEVEL_BOTTOM));
    
    hidden=(param->flags&MPLEX_ATTACH_HIDDEN
            && (param->flags&MPLEX_ATTACH_UNNUMBERED
                || !mx_was_empty));
    
    sw=(!hidden && (param->flags&MPLEX_ATTACH_SWITCHTO 
                    || (param->flags&MPLEX_ATTACH_UNNUMBERED
                        ? FALSE
                        : (mplex_current_node(mplex)==NULL))));
    
    hidden=(hidden || (!sw && !(param->flags&MPLEX_ATTACH_UNNUMBERED)));
    
    node=create_stacking();
    
    if(node==NULL)
        return FALSE;
    
    if(!(param->flags&MPLEX_ATTACH_UNNUMBERED)){
        lnode=ALLOC(WLListNode);
        if(lnode==NULL){
            stacking_free(node);
            return FALSE;
        }
        lnode->next=NULL;
        lnode->prev=NULL;
        lnode->phs=NULL;
        lnode->st=node;
        node->lnode=lnode;
    }
    
    if(!stacking_assoc(node, reg)){
        if(lnode!=NULL){
            node->lnode=NULL;
            free(lnode);
        }
        stacking_free(node);
        return FALSE;
    }

    node->hidden=TRUE;
    node->szplcy=szplcy;
    node->level=level;
    node->pseudomodal=(param->flags&MPLEX_ATTACH_PSEUDOMODAL ? 1 : 0);
    
    if(lnode!=NULL){
        WMPlexPHolder *ph2, *phn, *php;
        
        llist_link_after(&(mplex->mx_list), 
                         (ph!=NULL ? ph->after : NULL), 
                         lnode);
        mplex->mx_count++;
        
        
        /* Move placeholders after new node */
        for(php=NULL, ph2=ph; ph2!=NULL; php=ph2, ph2=phn){
            phn=ph2->next;
            mplexpholder_move(ph2, mplex, php, lnode);
        }
    }
    
    LINK_ITEM(mplex->mgd, node, mgr_next, mgr_prev);

    if(!OBJ_IS(reg, WGroup))
        mplex_stack(mplex, node);
    
    region_set_manager(reg, (WRegion*)mplex);
    
    if(param->flags&MPLEX_ATTACH_PASSIVE)
        reg->flags|=REGION_SKIP_FOCUS;
    
    if(!(param->flags&MPLEX_ATTACH_WHATEVER)){
        WFitParams fp;
        
        fp.mode=0;
        mplex_managed_geom(mplex, &(fp.g));
        
        sizepolicy(&node->szplcy, reg, 
                   (param->flags&MPLEX_ATTACH_GEOM ? &(param->geom) : NULL),
                   0, &fp);
        
        if(rectangle_compare(&fp.g, &REGION_GEOM(reg))!=RECTANGLE_SAME)
            region_fitrep(reg, NULL, &fp);
    }
    
    if(!hidden)
        mplex_do_node_display(mplex, node, FALSE);
    else
        region_unmap(reg);
        
    if(mcf){
        if(sw){
            mplex_refocus(mplex, node, FALSE);
        }else if(!hidden && 
                 (level>=STACKING_LEVEL_MODAL1 || OBJ_IS(reg, WGroup))){
            /* New modal regions may require focusing, so try to
             * give focus back to currently active object.
             * (There seems to be some problem with uncontained
             * client windows still..)
             */
            mplex_refocus(mplex, NULL, FALSE);
        }else if(!hidden){
            region_pointer_focus_hack(reg);
        }
    }else if(!hidden){
        region_pointer_focus_hack(reg);
    }
    
    if(lnode!=NULL)
        mplex_managed_changed(mplex, MPLEX_CHANGE_ADD, sw, reg);
    
    return TRUE;
}


static void mplex_attach_fp(WMPlex *mplex, const WMPlexAttachParams *param,
                            WFitParams *fp)
{
    if(param->flags&MPLEX_ATTACH_GEOM)
        fp->g=param->geom;
    else
        mplex_managed_geom(mplex, &(fp->g));
    
    fp->mode=REGION_FIT_WHATEVER|REGION_FIT_BOUNDS;
}


WRegion *mplex_do_attach_pholder(WMPlex *mplex, WMPlexPHolder *ph,
                                 WRegionAttachData *data)
{
    WFitParams fp;
    
    mplex_attach_fp(mplex, &ph->param, &fp);
    
    return region_attach_helper((WRegion*)mplex, 
                                (WWindow*)mplex, &fp,
                                (WRegionDoAttachFn*)mplex_do_attach_final,
                                (void*)ph, data);
}


WRegion *mplex_do_attach(WMPlex *mplex, WMPlexAttachParams *param,
                         WRegionAttachData *data)
{
    WMPlexPHolder *ph;
    WRegion *reg;
    
    ph=create_mplexpholder(mplex, NULL, param);
    
    if(ph==NULL)
        return NULL;

    reg=mplex_do_attach_pholder(mplex, ph, data);
    
    destroy_obj((Obj*)ph);
    
    return reg;
}


WRegion *mplex_do_attach_new(WMPlex *mplex, WMPlexAttachParams *param,
                             WRegionCreateFn *fn, void *fn_param)
{
    WRegionAttachData data;
    
    data.type=REGION_ATTACH_NEW;
    data.u.n.fn=fn;
    data.u.n.param=fn_param;
    
    return mplex_do_attach(mplex, param, &data);
}


#define MPLEX_ATTACH_SET_FLAGS (MPLEX_ATTACH_GEOM|       \
                                MPLEX_ATTACH_SIZEPOLICY| \
                                MPLEX_ATTACH_INDEX)


WRegion *mplex_attach_simple(WMPlex *mplex, WRegion *reg, int flags)
{
    WMPlexAttachParams param;
    WRegionAttachData data;
    
    param.flags=flags&~MPLEX_ATTACH_SET_FLAGS;
    
    data.type=REGION_ATTACH_REPARENT;
    data.u.reg=reg;
    
    return mplex_do_attach(mplex, &param, &data);
}


static void get_params(WMPlex *mplex, ExtlTab tab, int mask,
                       WMPlexAttachParams *par)
{
    int tmp;
    char *tmpstr;
    int ok=~mask;
    
    if(ok&MPLEX_ATTACH_LEVEL){
        if(extl_table_gets_i(tab, "level", &tmp)){
            if(tmp>=0){
                par->flags|=MPLEX_ATTACH_LEVEL;
                par->level=tmp;
            }
        }
    
        if(extl_table_is_bool_set(tab, "modal"))
            par->level=maxof(par->level, STACKING_LEVEL_MODAL1);
    }

    if(extl_table_is_bool_set(tab, "unnumbered"))
        par->flags|=MPLEX_ATTACH_UNNUMBERED&ok;
    
    if(extl_table_is_bool_set(tab, "switchto"))
        par->flags|=MPLEX_ATTACH_SWITCHTO&ok;

    if(extl_table_is_bool_set(tab, "hidden"))
        par->flags|=MPLEX_ATTACH_HIDDEN&ok;
        
    if(extl_table_is_bool_set(tab, "passive"))
        par->flags|=MPLEX_ATTACH_PASSIVE&ok;
        
    if(extl_table_is_bool_set(tab, "pseudomodal"))
        par->flags|=MPLEX_ATTACH_PSEUDOMODAL&ok;

    if(extl_table_gets_i(tab, "index", &(par->index)))
        par->flags|=MPLEX_ATTACH_INDEX&ok;

    if(ok&MPLEX_ATTACH_SIZEPOLICY){
        if(extl_table_gets_sizepolicy(tab, "sizepolicy", &par->szplcy)){
            par->flags|=MPLEX_ATTACH_SIZEPOLICY;
        }else if(extl_table_gets_i(tab, "sizepolicy", &tmp)){
            /* Backwards compat. numeric version */
            par->flags|=MPLEX_ATTACH_SIZEPOLICY;
            par->szplcy=tmp;
        }
    }
    
    if(extl_table_gets_rectangle(tab, "geom", &par->geom))
        par->flags|=MPLEX_ATTACH_GEOM&ok;
}


/*EXTL_DOC
 * Attach and reparent existing region \var{reg} to \var{mplex}.
 * The table \var{param} may contain the fields \var{index} and
 * \var{switchto} that are interpreted as for \fnref{WMPlex.attach_new}.
 */
EXTL_EXPORT_MEMBER
WRegion *mplex_attach(WMPlex *mplex, WRegion *reg, ExtlTab param)
{
    WMPlexAttachParams par=MPLEXATTACHPARAMS_INIT;
    WRegionAttachData data;

    if(reg==NULL)
        return NULL;
    
    get_params(mplex, param, 0, &par);
    
    data.type=REGION_ATTACH_REPARENT;
    data.u.reg=reg;
    
    return mplex_do_attach(mplex, &par, &data);
}


WRegion *mplex_attach_new_(WMPlex *mplex, WMPlexAttachParams *par, 
                           int mask, ExtlTab param)
{
    WRegionAttachData data;
    
    get_params(mplex, param, mask, par);
    
    data.type=REGION_ATTACH_LOAD;
    data.u.tab=param;
    
    return mplex_do_attach(mplex, par, &data);
}


/*EXTL_DOC
 * Create a new region to be managed by \var{mplex}. At least the following
 * fields in \var{param} are understood (all but \var{type} are optional).
 * 
 * \begin{tabularx}{\linewidth}{lX}
 *  \tabhead{Field & Description}
 *  \var{type} & (string) Class name (a string) of the object to be created. \\
 *  \var{name} & (string) Name of the object to be created (a string). \\
 *  \var{switchto} & (boolean) Should the region be switched to (boolean)? \\
 *  \var{unnumbered} & (boolean) Do not put on the numbered mutually 
 *                     exclusive list. \\
 *  \var{index} & (integer) Index on this list, same as for 
 *                \fnref{WMPlex.set_index}. \\
 *  \var{level} & (integer) Stacking level. \\
 *  \var{modal} & (boolean) Shortcut for modal stacking level. \\
 *  \var{hidden} & (boolean) Attach hidden, if not prevented
 *                  by e.g. the mutually exclusive list being empty.
 *                  This option overrides \var{switchto}. \\
 *  \var{passive} & (boolean) Skip in certain focusing operations. \\
 *  \var{pseudomodal} & (boolean) The attached region is ``pseudomodal''
 *                      if the stacking level dictates it to be modal.
 *                      This means that the region may be hidden to display
 *                      regions with lesser stacking levels. \\
 *  \var{sizepolicy} & (string) Size policy; see Section \ref{sec:sizepolicies}. \\
 *  \var{geom} & (table) Geometry specification. \\
 * \end{tabularx}
 * 
 * In addition parameters to the region to be created are passed in this 
 * same table.
 */
EXTL_EXPORT_MEMBER
WRegion *mplex_attach_new(WMPlex *mplex, ExtlTab param)
{
    WMPlexAttachParams par=MPLEXATTACHPARAMS_INIT;
    
    return mplex_attach_new_(mplex, &par, 0, param);
}


static bool mplex_handle_drop(WMPlex *mplex, int x, int y,
                              WRegion *dropped)
{
    WRegion *curr=mplex_mx_current(mplex);
    
    /* This code should handle dropping tabs on floating workspaces. */
    if(curr && HAS_DYN(curr, region_handle_drop)){
        int rx, ry;
        region_rootpos(curr, &rx, &ry);
        if(rectangle_contains(&REGION_GEOM(curr), x-rx, y-ry)){
            if(region_handle_drop(curr, x, y, dropped))
                return TRUE;
        }
    }
    
    return (NULL!=mplex_attach_simple(mplex, dropped, MPLEX_ATTACH_SWITCHTO));
}


WPHolder *mplex_prepare_manage(WMPlex *mplex, const WClientWin *cwin,
                               const WManageParams *param, int priority)
{
    int cpriority=MANAGE_PRIORITY_SUB(priority, MANAGE_PRIORITY_NORMAL);
    WMPlexAttachParams ap;
    WPHolder *ph=NULL;
    WMPlexPHolder *mph;
    WLListNode *after;
    
    /* Check current */ {
        WStacking *cur=mplex_current_node(mplex);
        
        if(cur!=NULL){
            ph=region_prepare_manage(cur->reg, cwin, param, cpriority);
            if(ph!=NULL)
                return ph;
        }
        
        if(mplex->mx_current!=NULL && mplex->mx_current->st!=cur){
            ph=region_prepare_manage(mplex->mx_current->st->reg, 
                                     cwin, param, cpriority);
            if(ph!=NULL)
                return ph;
        }
    }
    
    if(!MANAGE_PRIORITY_OK(priority, MANAGE_PRIORITY_NORMAL))
        return NULL;
    
    ap.flags=((param->switchto ? MPLEX_ATTACH_SWITCHTO : 0)
              |MPLEX_ATTACH_SIZEPOLICY);
    ap.szplcy=SIZEPOLICY_FULL_EXACT;
    
    mph=create_mplexpholder(mplex, NULL, &ap);
    
    if(mph!=NULL){
        WGroupPHolder *gph;
        WGroupAttachParams gp=GROUPATTACHPARAMS_INIT;
        
        gp.switchto_set=1;
        gp.switchto=1;
        gp.bottom=1;
        
        gph=create_grouppholder(NULL, NULL, &gp);
        
        if(gph!=NULL){
            gph->recreate_pholder=(WPHolder*)mph;
            return (WPHolder*)gph;
        }
    }
    
    return (WPHolder*)mph;
}


/*}}}*/


/*{{{ Remove */


void mplex_managed_remove(WMPlex *mplex, WRegion *sub)
{
    bool mx=FALSE, hadfocus=FALSE, mcf;
    WRegion *stdisp=(WRegion*)(mplex->stdispwatch.obj);
    WStacking *node, *next=NULL;
 
    mcf=region_may_control_focus((WRegion*)mplex);
    
    if(stdisp!=NULL){
        if(CAN_MANAGE_STDISP(sub) && 
           region_managed_within((WRegion*)mplex, stdisp)==sub){
            region_unmanage_stdisp(sub, TRUE, TRUE);
            region_detach_manager(stdisp);
        }
    }
    
    node=mplex_find_stacking(mplex, sub);
    
    if(node==NULL)
        return;
    
    hadfocus=(mplex_current_node(mplex)==node);
    
    if(node->lnode!=NULL){
        if(mplex->mx_current==node->lnode){
            WLListNode *lnext;
            
            mplex->mx_current=NULL;
            lnext=LIST_PREV(mplex->mx_list, node->lnode, next, prev);
            if(lnext==NULL){
                lnext=LIST_NEXT(mplex->mx_list, node->lnode, next, prev);
                if(lnext==node->lnode)
                    lnext=NULL;
            }
            if(lnext!=NULL)
                next=lnext->st;
        }
        
        mplex_move_phs_before(mplex, node->lnode);
        llist_unlink(&(mplex->mx_list), node->lnode);
        mplex->mx_count--;

        free(node->lnode);
        node->lnode=NULL;
        mx=TRUE;
    }
    
    UNLINK_ITEM(mplex->mgd, node, mgr_next, mgr_prev);
    
    mplex_unstack(mplex, node);
    
    stacking_unassoc(node);
    stacking_free(node);

    region_unset_manager(sub, (WRegion*)mplex);
    
    if(OBJ_IS_BEING_DESTROYED(mplex))
        return;
    
    if(next!=NULL)
        mplex_do_node_display(mplex, next, FALSE);

    if(hadfocus && mcf)
        mplex_refocus(mplex, next, FALSE);
    
    if(mx)
        mplex_managed_changed(mplex, MPLEX_CHANGE_REMOVE, next!=NULL, sub);
}


void mplex_child_removed(WMPlex *mplex, WRegion *sub)
{
    if(sub!=NULL && sub==(WRegion*)(mplex->stdispwatch.obj)){
        watch_reset(&(mplex->stdispwatch));
        mplex_set_stdisp(mplex, NULL, NULL);
    }
}


/*}}}*/


/*{{{ Rescue */


bool mplex_rescue_clientwins(WMPlex *mplex, WRescueInfo *info)
{
    bool ret1, ret2;
    WMPlexIterTmp tmp;
    WLListIterTmp ltmp;
    WLListNode *lnode, *was_current=mplex->mx_current;

     
    /* First all mx stuff to move them nicely to another mplex (when that 
     * is the case), switching to the current region in the target if 
     * allowed by ph_flags_mask region_rescue.
     */
    FOR_ALL_NODES_ON_LLIST(lnode, mplex->mx_list, ltmp){
        int sw=(lnode==was_current ? PHOLDER_ATTACH_SWITCHTO : 0);
        region_do_rescue_this(lnode->st->reg, info, sw);
    }
    
    /* Then the rest (possibly retrying failed mx stuff).
     */
    mplex_iter_init(&tmp, mplex);
    ret1=region_rescue_some_clientwins((WRegion*)mplex, info,
                                       (WRegionIterator*)mplex_iter,
                                       &tmp);
    
    ret2=region_rescue_child_clientwins((WRegion*)mplex, info);
    
    return (ret1 && ret2);
}


/*}}}*/


/*{{{ Status display support */


bool mplex_set_stdisp(WMPlex *mplex, WRegion *reg, 
                      const WMPlexSTDispInfo *din)
{
    WRegion *oldstdisp=(WRegion*)(mplex->stdispwatch.obj);
    WRegion *mgr=NULL;
    
    assert(reg==NULL || (reg==oldstdisp) ||
           (REGION_MANAGER(reg)==NULL && 
            REGION_PARENT(reg)==(WWindow*)mplex));
    
    if(oldstdisp!=NULL){
        mgr=region_managed_within((WRegion*)mplex, oldstdisp);
        
        if(!CAN_MANAGE_STDISP(mgr))
            mgr=NULL;
    }
    
    if(din!=NULL)
        mplex->stdispinfo=*din;
    
    if(reg==NULL){
        watch_reset(&(mplex->stdispwatch));
        
        if(mgr!=NULL){
            region_unmanage_stdisp(mgr, TRUE, FALSE);
            if(oldstdisp!=NULL)
                region_detach_manager(oldstdisp);
        }
    }else{
        watch_setup(&(mplex->stdispwatch), (Obj*)reg, NULL);
        
        mplex_remanage_stdisp(mplex);
    }
    
    if(oldstdisp!=NULL && oldstdisp!=reg)
        mainloop_defer_destroy((Obj*)oldstdisp);
    
    return TRUE;
}


void mplex_get_stdisp(WMPlex *mplex, WRegion **reg, WMPlexSTDispInfo *di)
{
    *di=mplex->stdispinfo;
    *reg=(WRegion*)mplex->stdispwatch.obj;
}


static StringIntMap pos_map[]={
    {"tl", MPLEX_STDISP_TL},
    {"tr", MPLEX_STDISP_TR},
    {"bl", MPLEX_STDISP_BL},
    {"br", MPLEX_STDISP_BR},
    {NULL, 0}
};


static bool do_attach_stdisp(WRegion *mplex, WRegion *reg, void *unused)
{
    /* We do not actually manage the stdisp. */
    return TRUE;
}


/*EXTL_DOC
 * Set/create status display for \var{mplex}. Table is a standard
 * description of the object to be created (as passed to e.g. 
 * \fnref{WMPlex.attach_new}). In addition, the following fields are
 * recognised:
 * 
 * \begin{tabularx}{\linewidth}{lX}
 *   \tabhead{Field & Description}
 *   \var{pos} & (string) The corner of the screen to place the status 
 *               display in: one of \codestr{tl}, \codestr{tr}, \codestr{bl} 
 *               or \codestr{br}. \\
 *   \var{fullsize} & (boolean) Waste all available space. \\
 *   \var{action} & (string) If this field is set to \codestr{keep}, 
 *                  \var{pos} and \var{fullsize} are changed for the existing
 *                  status display. If this field is set to \codestr{remove},
 *                  the existing status display is removed. If this
 *                  field is not set or is set to \codestr{replace}, a 
 *                  new status display is created and the old, if any,
 *                  removed. \\
 * \end{tabularx}
 */
EXTL_EXPORT_AS(WMPlex, set_stdisp)
WRegion *mplex_set_stdisp_extl(WMPlex *mplex, ExtlTab t)
{
    WRegion *stdisp=NULL;
    WMPlexSTDispInfo din=mplex->stdispinfo;
    char *s;
    
    if(extl_table_gets_s(t, "pos", &s)){
        din.pos=stringintmap_value(pos_map, s, -1);
        if(din.pos<0){
            warn(TR("Invalid position setting."));
            return NULL;
        }
    }
    
    extl_table_gets_b(t, "fullsize", &(din.fullsize));
    
    s=NULL;
    extl_table_gets_s(t, "action", &s);
    
    if(s==NULL || strcmp(s, "replace")==0){
        WRegionAttachData data;
        WFitParams fp;
        int o2;
        
        fp.g.x=0;
        fp.g.y=0;
        fp.g.w=REGION_GEOM(mplex).w;
        fp.g.h=REGION_GEOM(mplex).h;
        fp.mode=REGION_FIT_BOUNDS|REGION_FIT_WHATEVER;
        
        /* Full mplex size is stupid so use saved geometry initially
         * if there's one.
         */
        extl_table_gets_rectangle(t, "geom", &(fp.g));
        
        data.type=REGION_ATTACH_LOAD;
        data.u.tab=t;
        
        stdisp=region_attach_helper((WRegion*)mplex, 
                                    (WWindow*)mplex, &fp,
                                    do_attach_stdisp, NULL,
                                    &data);
        
        if(stdisp==NULL)
            return NULL;
        
    }else if(strcmp(s, "keep")==0){
        stdisp=(WRegion*)(mplex->stdispwatch.obj);
    }else if(strcmp(s, "remove")!=0){
        warn(TR("Invalid action setting."));
        return FALSE;
    }
    
    if(!mplex_set_stdisp(mplex, stdisp, &din)){
        destroy_obj((Obj*)stdisp);
        return NULL;
    }
    
    return stdisp;
}


static ExtlTab mplex_do_get_stdisp_extl(WMPlex *mplex, bool fullconfig)
{
    WRegion *reg=(WRegion*)mplex->stdispwatch.obj;
    ExtlTab t;
    
    if(reg==NULL)
        return extl_table_none();
    
    if(fullconfig){
        t=region_get_configuration(reg);
        extl_table_sets_rectangle(t, "geom", &REGION_GEOM(reg));
    }else{
        t=extl_create_table();
        extl_table_sets_o(t, "reg", (Obj*)reg);
    }
    
    if(t!=extl_table_none()){
        WMPlexSTDispInfo *di=&(mplex->stdispinfo);
        extl_table_sets_s(t, "pos", stringintmap_key(pos_map, di->pos, NULL));
        extl_table_sets_b(t, "fullsize", di->fullsize);
    }
    return t;
}


/*EXTL_DOC
 * Get status display information. See \fnref{WMPlex.get_stdisp} for
 * information on the fields.
 */
EXTL_SAFE
EXTL_EXPORT_AS(WMPlex, get_stdisp)
ExtlTab mplex_get_stdisp_extl(WMPlex *mplex)
{
    return mplex_do_get_stdisp_extl(mplex, FALSE);
}


/*}}}*/


/*{{{ Dynfuns */


void mplex_managed_geom_default(const WMPlex *mplex, WRectangle *geom)
{
    geom->x=0;
    geom->y=0;
    geom->w=REGION_GEOM(mplex).w;
    geom->h=REGION_GEOM(mplex).h;
}


void mplex_managed_geom(const WMPlex *mplex, WRectangle *geom)
{
    CALL_DYN(mplex_managed_geom, mplex, (mplex, geom));
}


void mplex_size_changed(WMPlex *mplex, bool wchg, bool hchg)
{
    CALL_DYN(mplex_size_changed, mplex, (mplex, wchg, hchg));
}


void mplex_managed_changed(WMPlex *mplex, int mode, bool sw, WRegion *mgd)
{
    CALL_DYN(mplex_managed_changed, mplex, (mplex, mode, sw, mgd));
}


int mplex_default_index(WMPlex *mplex)
{
    int idx=LLIST_INDEX_LAST;
    CALL_DYN_RET(idx, int, mplex_default_index, mplex, (mplex));
    return idx;
}


/* For regions managing stdisps */

void region_manage_stdisp(WRegion *reg, WRegion *stdisp, 
                          const WMPlexSTDispInfo *info)
{
    CALL_DYN(region_manage_stdisp, reg, (reg, stdisp, info));
}


void region_unmanage_stdisp(WRegion *reg, bool permanent, bool nofocus)
{
    CALL_DYN(region_unmanage_stdisp, reg, (reg, permanent, nofocus));
}


/*}}}*/


/*{{{ Changed hook helper */


static const char *mode2str(int mode)
{
    if(mode==MPLEX_CHANGE_SWITCHONLY)
        return "switchonly";
    else if(mode==MPLEX_CHANGE_REORDER)
        return "reorder";
    else if(mode==MPLEX_CHANGE_ADD)
        return "add";
    else if(mode==MPLEX_CHANGE_REMOVE)
        return "remove";
    return NULL;
}
    

static bool mrsh_chg(ExtlFn fn, WMPlexChangedParams *p)
{
    ExtlTab t=extl_create_table();
    bool ret;
    
    extl_table_sets_o(t, "reg", (Obj*)p->reg);
    extl_table_sets_s(t, "mode", mode2str(p->mode));
    extl_table_sets_b(t, "sw", p->sw);
    extl_table_sets_o(t, "sub", (Obj*)p->sub);
    
    extl_protect(NULL);
    ret=extl_call(fn, "t", NULL, t);
    extl_unprotect(NULL);
    
    extl_unref_table(t);
    
    return ret;
}


void mplex_call_changed_hook(WMPlex *mplex, WHook *hook, 
                             int mode, bool sw, WRegion *reg)
{
    WMPlexChangedParams p;
    
    p.reg=mplex;
    p.mode=mode;
    p.sw=sw;
    p.sub=reg;

    hook_call_p(hook, &p, (WHookMarshallExtl*)mrsh_chg);
}


/*}}} */


/*{{{ Save/load */


static void save_node(WMPlex *mplex, ExtlTab subs, int *n, 
                      WStacking *node, bool unnumbered)
{
    ExtlTab st, g;
    
    st=region_get_configuration(node->reg);
    
    if(st!=extl_table_none()){
        if(mplex->mx_current!=NULL && node==mplex->mx_current->st)
            extl_table_sets_b(st, "switchto", TRUE);
        extl_table_sets_s(st, "sizepolicy", 
                          sizepolicy2string(node->szplcy));
        extl_table_sets_i(st, "level", node->level);
        g=extl_table_from_rectangle(&REGION_GEOM(node->reg));
        extl_table_sets_t(st, "geom", g);
        extl_unref_table(g);
        if(STACKING_IS_HIDDEN(node))
            extl_table_sets_b(st, "hidden", TRUE);
        if(STACKING_IS_PSEUDOMODAL(node))
            extl_table_sets_b(st, "pseudomodal", TRUE);
        if(unnumbered)
            extl_table_sets_b(st, "unnumbered", TRUE);
        
        extl_table_seti_t(subs, ++(*n), st);
        extl_unref_table(st);
    }
}


ExtlTab mplex_get_configuration(WMPlex *mplex)
{
    ExtlTab tab, subs, stdisptab;
    WMPlexIterTmp tmp;
    WLListIterTmp ltmp;
    WLListNode *lnode;
    WStacking *node;
    int n=0;
    
    tab=region_get_base_configuration((WRegion*)mplex);
    
    subs=extl_create_table();
    extl_table_sets_t(tab, "managed", subs);
    
    /* First the numbered/mutually exclusive nodes */
    FOR_ALL_NODES_ON_LLIST(lnode, mplex->mx_list, ltmp){
        save_node(mplex, subs, &n, lnode->st, FALSE);
    }

    FOR_ALL_NODES_IN_MPLEX(mplex, node, tmp){
        if(node->lnode==NULL)
            save_node(mplex, subs, &n, node, TRUE);
    }

    extl_unref_table(subs);
    
    /*stdisptab=mplex_do_get_stdisp_extl(mplex, TRUE);
    if(stdisptab!=extl_table_none()){
        extl_table_sets_t(tab, "stdisp", stdisptab);
        extl_unref_table(stdisptab);
    }*/
    
    return tab;
}


void mplex_load_contents(WMPlex *mplex, ExtlTab tab)
{
    ExtlTab substab, subtab;
    int n, i;

    /*if(extl_table_gets_t(tab, "stdisp", &subtab)){
        mplex_set_stdisp_extl(mplex, subtab);
        extl_unref_table(subtab);
    }*/
    
    if(extl_table_gets_t(tab, "managed", &substab) ||
       extl_table_gets_t(tab, "subs", &substab)){
        n=extl_table_get_n(substab);
        for(i=1; i<=n; i++){
            if(extl_table_geti_t(substab, i, &subtab)){
                WMPlexAttachParams par=MPLEXATTACHPARAMS_INIT;
                WFitParams fp;
                WPHolder *ph;
                
                get_params(mplex, subtab, 0, &par);
                mplex_attach_fp(mplex, &par, &fp);
                
                par.flags|=MPLEX_ATTACH_INDEX;
                par.index=LLIST_INDEX_LAST;
                
                ph=(WPHolder*)create_mplexpholder(mplex, NULL, &par);
                
                if(ph!=NULL){
                    region_attach_load_helper((WRegion*)mplex, (WWindow*)mplex, &fp,
                                              (WRegionDoAttachFn*)mplex_do_attach_final,
                                              (void*)ph, subtab, &ph);
                                              
                    if(ph!=NULL)
                        destroy_obj((Obj*)ph);
                }
                
                extl_unref_table(subtab);
            }
        }
        extl_unref_table(substab);
    }
}


WRegion *mplex_load(WWindow *par, const WFitParams *fp, ExtlTab tab)
{
    WMPlex *mplex=create_mplex(par, fp);
    if(mplex!=NULL)
        mplex_load_contents(mplex, tab);
    return (WRegion*)mplex;
}


/*}}}*/


/*{{{ Dynfuntab and class info */


static DynFunTab mplex_dynfuntab[]={
    {region_do_set_focus, 
     mplex_do_set_focus},
    
    {region_managed_remove, 
     mplex_managed_remove},
    
    {region_managed_rqgeom,
     mplex_managed_rqgeom},
    
    {(DynFun*)region_managed_prepare_focus,
     (DynFun*)mplex_managed_prepare_focus},
    
    {(DynFun*)region_handle_drop, 
     (DynFun*)mplex_handle_drop},
    
    {region_map, mplex_map},
    {region_unmap, mplex_unmap},
    
    {(DynFun*)region_prepare_manage,
     (DynFun*)mplex_prepare_manage},
    
    {(DynFun*)region_current,
     (DynFun*)mplex_current},
    
    {(DynFun*)region_rescue_clientwins,
     (DynFun*)mplex_rescue_clientwins},
    
    {(DynFun*)region_get_configuration,
     (DynFun*)mplex_get_configuration},
    
    {mplex_managed_geom, 
     mplex_managed_geom_default},
    
    {(DynFun*)region_fitrep,
     (DynFun*)mplex_fitrep},
    
    {region_child_removed,
     mplex_child_removed},
    
    {(DynFun*)region_managed_get_pholder,
     (DynFun*)mplex_managed_get_pholder},

    {(DynFun*)region_get_rescue_pholder_for,
     (DynFun*)mplex_get_rescue_pholder_for},

    {(DynFun*)region_navi_first,
     (DynFun*)mplex_navi_first},
    
    {(DynFun*)region_navi_next,
     (DynFun*)mplex_navi_next},
    
    {(DynFun*)region_managed_rqorder,
     (DynFun*)mplex_managed_rqorder},
    
    END_DYNFUNTAB
};


EXTL_EXPORT
IMPLCLASS(WMPlex, WWindow, mplex_deinit, mplex_dynfuntab);


/*}}}*/

