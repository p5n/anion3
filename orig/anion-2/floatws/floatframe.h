/*
 * ion/floatws/floatframe.h
 *
 * Copyright (c) Tuomo Valkonen 1999-2004. 
 *
 * Ion is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 */

#ifndef ION_FLOATWS_FLOATFRAME_H
#define ION_FLOATWS_FLOATFRAME_H

#include <ioncore/common.h>
#include <ioncore/genframe.h>
#include <ioncore/window.h>
#include <ioncore/extl.h>
#include "floatws.h"

INTROBJ(WFloatFrame);

DECLOBJ(WFloatFrame){
    WGenFrame genframe;
    int bar_w;
    
    double bar_max_width_q;
    int tab_min_w;
    bool sticky;
};


extern WFloatFrame *create_floatframe(WWindow *parent, 
                                      const WRectangle *geom);

extern void floatframe_remove_managed(WFloatFrame *frame, WRegion *reg);

extern void initial_to_floatframe_geom(WFloatWS *ws, WRectangle *geom, 
                                       int gravity);
extern void managed_to_floatframe_geom(WRootWin *rootwin, WRectangle *geom);

extern WRegion *floatframe_load(WWindow *par, const WRectangle *geom, 
                                ExtlTab tab);

extern void floatframe_p_move(WFloatFrame *frame);
extern void floatframe_toggle_shade(WFloatFrame *frame);

#endif /* ION_FLOATWS_FLOATFRAME_H */
