/*
 * ion/ioncore/property.h
 *
 * Copyright (c) Tuomo Valkonen 1999-2004. 
 *
 * Ion is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 */

#ifndef ION_IONCORE_PROPERTY_H
#define ION_IONCORE_PROPERTY_H

#include <X11/Xatom.h>

#include "common.h"

extern ulong get_property(Display *dpy, Window win, Atom atom, Atom type,
                          ulong n32expected, bool more, uchar **p);
extern char *get_string_property(Window win, Atom a, int *nret);
extern void set_string_property(Window win, Atom a, const char *value);
extern bool get_integer_property(Window win, Atom a, int *vret);
extern void set_integer_property(Window win, Atom a, int value);
extern bool get_win_state(Window win, int *state);
extern void set_win_state(Window win, int state);
extern char **get_text_property(Window win, Atom a, int *nret);
extern void set_text_property(Window win, Atom a, const char *str);

#endif /* ION_IONCORE_PROPERTY_H */

