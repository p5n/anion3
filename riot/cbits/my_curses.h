#ifndef MY_CURSES_H
#define MY_CURSES_H

/* arch-tag: d71507ca-a4d9-46ec-ae82-d0f9d6ab1a0f */

#if 0
#ifndef CONFIG_INCLUDED
#define CONFIG_INCLUDED
#undef PACKAGE_NAME
#undef PACKAGE_STRING
#undef PACKAGE_TARNAME
#undef PACKAGE_VERSION
#if __GLASGOW_HASKELL__ >= 603
# include "ghcconfig.h"
#else
# include "config.h"
#endif
#endif
#endif

#if HAVE_NCURSESW_NCURSES_H 
#include <ncursesw/ncurses.h>
#elif HAVE_NCURSES_H
#include <ncurses.h>
#else
#include <curses.h>
#endif


#include <signal.h>

#if defined(initscr)
#undef initscr
#endif

#if defined(cbreak)
#undef cbreak
#endif

#if defined(clrtoeol)
#undef clrtoeol
#endif

#if defined(touchwin)
#undef touchwin
#endif

#if defined(beep)
#undef beep
#endif

#if defined(flash)
#undef flash
#endif

#if defined(wattr_set)
#undef wattr_set
#endif

#if defined(wattr_get)
#undef wattr_get
#endif


#endif
