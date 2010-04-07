/*
 * ion/ioncore/pointer.c
 *
 * Copyright (c) Tuomo Valkonen 1999-2004. 
 *
 * Ion is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 */

#include "common.h"
#include "pointer.h"
#include "cursor.h"
#include "event.h"
#include "global.h"
#include "focus.h"
#include "regbind.h"
#include "grab.h"


/*{{{ Variables */


static uint p_button=0, p_state=0;
static int p_x=-1, p_y=-1;
static int p_orig_x=-1, p_orig_y=-1;
static bool p_motion=FALSE;
static int p_clickcnt=0;
static Time p_time=0;
static int p_area=0;
static enum{ST_NO, ST_INIT, ST_HELD} p_grabstate=ST_NO;

static WButtonHandler *p_motion_end_handler=NULL;
static WMotionHandler *p_motion_handler=NULL;
static WMotionHandler *p_motion_begin_handler=NULL;
static GrabHandler *p_key_handler=NULL;
static GrabKilledHandler *p_killed_handler=NULL;

static WWatch p_regwatch=WWATCH_INIT, p_subregwatch=WWATCH_INIT;

#define p_reg ((WRegion*)p_regwatch.obj)
#define p_subreg ((WRegion*)p_subregwatch.obj)


/*}}}*/


/*{{{ Handler setup */


bool p_set_drag_handlers(WRegion *reg,
                         WMotionHandler *begin,
                         WMotionHandler *motion,
                         WButtonHandler *end,
                         GrabHandler *keypress,
                         GrabKilledHandler *killed)
{
    if(pointer_grab_region()==NULL || p_motion)
        return FALSE;
    
    /* A motion handler set at this point may not set a begin handler */
    if(p_grabstate!=ST_HELD && begin!=NULL)
        return FALSE;
    
    if(p_reg!=reg){
        setup_watch(&p_regwatch, (WObj*)reg, NULL);
        reset_watch(&p_subregwatch);
    }
    
    p_motion_begin_handler=begin;
    p_motion_handler=motion;
    p_motion_end_handler=end;
    p_key_handler=keypress;
    p_killed_handler=killed;
    p_motion=TRUE;
    
    return TRUE;
}


/*}}}*/


/*{{{ Misc. */


bool coords_in_rect(const WRectangle *g, int x, int y)
{
    return (x>=g->x && x<g->x+g->w && y>=g->y && y<g->y+g->h);
}


static bool time_in_threshold(Time time)
{
    Time t;
    
    if(time<p_time)
        t=p_time-time;
    else
        t=time-p_time;
    
    return t<wglobal.dblclick_delay;
}


static bool motion_in_threshold(int x, int y)
{
    return (x>p_x-CF_DRAG_TRESHOLD && x<p_x+CF_DRAG_TRESHOLD &&
            y>p_y-CF_DRAG_TRESHOLD && y<p_y+CF_DRAG_TRESHOLD);
}


WRegion *pointer_grab_region()
{
    if(p_grabstate==ST_NO)
        return NULL;
    return p_reg;
}


/*}}}*/


/*{{{ Call handlers */


static XEvent *p_curr_event=NULL;


XEvent *p_current_event()
{
    return p_curr_event;
}


static void call_button(WBinding *binding, XButtonEvent *ev)
{
    WButtonHandler *fn;

    if(binding==NULL)
        return;

    p_curr_event=(XEvent*)ev;
    extl_call(binding->func, "oo", NULL, p_reg, p_subreg);
    p_curr_event=NULL;
}


static void call_motion(XMotionEvent *ev, int dx, int dy)
{
    if(p_motion_handler!=NULL && p_reg!=NULL){
        p_curr_event=(XEvent*)ev;
        p_motion_handler(p_reg, ev, dx, dy);
        p_curr_event=NULL;
    }
}


static void call_motion_end(XButtonEvent *ev)
{
    if(p_motion_end_handler!=NULL && p_reg!=NULL){
        p_curr_event=(XEvent*)ev;
        p_motion_end_handler(p_reg, ev);
        p_curr_event=NULL;
    }
}


static void call_motion_begin(WBinding *binding, XMotionEvent *ev,
                              int dx, int dy)
{
    WMotionHandler *fn;
    
    if(binding==NULL)
        return;

    p_curr_event=(XEvent*)ev;
    
    extl_call(binding->func, "oo", NULL, p_reg, p_subreg);
    
    if(p_motion_begin_handler!=NULL && p_reg!=NULL)
        p_motion_begin_handler(p_reg, ev, dx, dy);
    
    p_motion_begin_handler=NULL;
    
    p_curr_event=NULL;
}


/*}}}*/


/*{{{ handle_button_press/release/motion */


static void finish_pointer()
{
    if(p_reg!=NULL)
        window_release((WWindow*)p_reg);
    p_grabstate=ST_NO;
    reset_watch(&p_subregwatch);
}


static bool handle_key(WRegion *reg, XEvent *ev)
{
    if(p_key_handler!=NULL){
        if(p_key_handler(reg, ev)){
            finish_pointer();
            return TRUE;
        }
    }
    return FALSE;
}


static void pointer_grab_killed(WRegion *unused)
{
    if(p_reg!=NULL && p_killed_handler!=NULL)
        p_killed_handler(p_reg);
    reset_watch(&p_regwatch);
    finish_pointer();
}


bool handle_button_press(XButtonEvent *ev)
{
    WBinding *pressbind=NULL;
    WRegion *reg=NULL;
    WRegion *subreg=NULL;
    uint button, state;
    bool dblclick;
    
    state=ev->state;
    button=ev->button;
    
    reg=(WRegion*)FIND_WINDOW_T(ev->window, WWindow);
    
    if(reg==NULL)
        return FALSE;

    if(ev->subwindow!=None){
        XButtonEvent ev2=*ev;
        ev2.window=ev->subwindow;
        if(XTranslateCoordinates(wglobal.dpy, ev->window, ev2.window,
                                 ev->x, ev->y, &(ev2.x), &(ev2.y),
                                 &(ev2.subwindow))){
            if(handle_button_press(&ev2))
                return TRUE;
        }
    }

    dblclick=(p_clickcnt==1 && time_in_threshold(ev->time) && 
              p_button==button && p_state==state && reg==p_reg);
    
    p_motion=FALSE;
    p_motion_begin_handler=NULL;
    p_motion_handler=NULL;
    p_motion_end_handler=NULL;
    p_key_handler=NULL;
    p_killed_handler=NULL;
    p_grabstate=ST_INIT;
    p_button=button;
    p_state=state;
    p_orig_x=p_x=ev->x_root;
    p_orig_y=p_y=ev->y_root;
    p_time=ev->time;
    p_clickcnt=0;

    setup_watch(&p_regwatch, (WObj*)reg, NULL);
    
    subreg=NULL;
    p_area=window_press((WWindow*)reg, ev, &subreg);
    if(subreg)
        setup_watch(&p_subregwatch, (WObj*)subreg, NULL);

    if(dblclick){
        pressbind=region_lookup_binding_area(reg, ACT_BUTTONDBLCLICK, state,
                                             button, p_area);
    }
    
    if(pressbind==NULL){
        pressbind=region_lookup_binding_area(reg, ACT_BUTTONPRESS, state,
                                             button, p_area);
    }
    
    grab_establish(reg, handle_key, pointer_grab_killed, 0);
    p_grabstate=ST_HELD;
    
    call_button(pressbind, ev);
    
    return TRUE;
}


bool handle_button_release(XButtonEvent *ev)
{
    WBinding *binding=NULL;
    
    if(p_button!=ev->button)
           return FALSE;

    if(p_reg!=NULL){
        if(p_motion==FALSE){
            p_clickcnt=1;
            binding=region_lookup_binding_area(p_reg, ACT_BUTTONCLICK,
                                               p_state, p_button, p_area);
            call_button(binding, ev);
        }else{
            call_motion_end(ev);
        }
        
    }
    
    grab_remove(handle_key);
    finish_pointer();
    
    return TRUE;
}


void handle_pointer_motion(XMotionEvent *ev)
{
    int dx, dy;
    WBinding *binding=NULL;
    
    if(p_reg==NULL)
        return;
    
    if(!p_motion){
        if(motion_in_threshold(ev->x_root, ev->y_root))
            return;
        binding=region_lookup_binding_area(p_reg, ACT_BUTTONMOTION,
                                           p_state, p_button, p_area);
    }
    
    p_time=ev->time;
    dx=ev->x_root-p_x;
    dy=ev->y_root-p_y;
    p_x=ev->x_root;
    p_y=ev->y_root;    
    
    if(!p_motion){
        call_motion_begin(binding, ev, dx, dy);
        p_motion=TRUE;
    }else{
        call_motion(ev, dx, dy);
    }
}


/*}}}*/

