/*
 * ion/menu/mkmenu.h
 *
 * Copyright (c) Tuomo Valkonen 1999-2004. 
 *
 * Ion is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 */

#ifndef ION_MENU_MKMENU_H
#define ION_MENU_MKMENU_H

#include <ioncore/common.h>
#include <ioncore/extl.h>
#include <ioncore/mplex.h>
#include <ioncore/window.h>
#include "menu.h"

extern WMenu *menu_menu(WMPlex *mplex, ExtlFn handler, ExtlTab tab, 
                        bool big_mode);
extern WMenu *menu_pmenu(WWindow *where, ExtlFn handler, ExtlTab tab);

#endif /* ION_MENU_MKMENU_H */
