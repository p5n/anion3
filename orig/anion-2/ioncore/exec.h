/*
 * ion/ioncore/exec.h
 *
 * Copyright (c) Tuomo Valkonen 1999-2004. 
 *
 * Ion is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 */

#ifndef ION_IONCORE_EXEC_H
#define ION_IONCORE_EXEC_H

#include "common.h"
#include "rootwin.h"

extern void do_exec(const char *cmd);
extern bool exec_on_rootwin(WRootWin *rootwin, const char *cmd);
extern bool exec(const char *cmd);
extern void restart_other_wm(const char *cmd);
extern void restart_wm();
extern void exit_wm();
extern void setup_environ(int scr);

#endif /* ION_IONCORE_EXEC_H */
