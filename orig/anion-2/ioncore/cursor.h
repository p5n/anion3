/*
 * ion/ioncore/cursor.h
 *
 * Copyright (c) Tuomo Valkonen 1999-2004. 
 *
 * Ion is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 */

#ifndef ION_IONCORE_CURSOR_H
#define ION_IONCORE_CURSOR_H

#include <X11/Xlib.h>
#include <X11/cursorfont.h>

#define CURSOR_DEFAULT     0
#define CURSOR_RESIZE     1
#define CURSOR_MOVE     2
#define CURSOR_DRAG        3
#define CURSOR_WAITKEY    4
#define N_CURSORS        5

extern void load_cursors();
extern void set_cursor(Window win, int cursor);
extern Cursor x_cursor(int cursor);

#endif /* ION_IONCORE_CURSOR_H */
