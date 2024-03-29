/*
 * ion/de/init.h
 *
 * Copyright (c) Tuomo Valkonen 1999-2004. 
 *
 * Ion is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 */

#ifndef ION_DE_INIT_H
#define ION_DE_INIT_H

#include <ioncore/common.h>
#include <ioncore/gr.h>
#include "brush.h"

extern void de_get_border_val(uint *val, ExtlTab tab, const char *what);
extern void de_get_border_style(uint *ret, ExtlTab tab);
extern void de_get_border(DEBorder *border, ExtlTab tab);

extern bool de_get_colour(WRootWin *rootwin, DEColour *ret, 
                          ExtlTab tab, const char *what, 
                          DEColour substitute);
extern void de_get_colour_group(WRootWin *rootwin, DEColourGroup *cg, 
                                ExtlTab tab);
extern void de_get_extra_cgrps(WRootWin *rootwin, DEStyle *style, 
                               ExtlTab tab);

extern void de_get_text_align(int *alignret, ExtlTab tab);

extern void de_get_transparent_background(uint *mode, ExtlTab tab);

extern void de_get_nonfont(WRootWin *rw, DEStyle *style, ExtlTab tab);

extern bool de_do_define_style(WRootWin *rootwin, const char *name, 
                               ExtlTab tab);

#endif /* ION_DE_INIT_H */
