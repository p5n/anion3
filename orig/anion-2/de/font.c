/*
 * ion/de/font.c
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
#include <ioncore/objp.h>
#include "font.h"
#include "fontset.h"
#include "brush.h"


/*{{{ Load/free */


static DEFont *fonts=NULL;


DEFont *de_load_font(const char *fontname)
{
    DEFont *fnt;
    XFontSet fontset=NULL;
    XFontStruct *fontstruct=NULL;
    
    if(fontname==NULL){
        warn("Attempt to load NULL as font");
        return NULL;
    }
    
    /* There shouldn't be that many fonts... */
    for(fnt=fonts; fnt!=NULL; fnt=fnt->next){
        if(strcmp(fnt->pattern, fontname)==0){
            fnt->refcount++;
            return fnt;
        }
    }
    
    if(wglobal.use_mb){
        fontset=de_create_font_set(fontname);
        if(fontset!=NULL){
            if(XContextDependentDrawing(fontset)){
                warn("Fontset for font pattern '%s' implements context "
                     "dependent drawing, which is unsupported. Expect "
                     "clutter.", fontname);
            }
        }
    }else{
        fontstruct=XLoadQueryFont(wglobal.dpy, fontname);
    }
    
    if(fontstruct==NULL && fontset==NULL){
        if(strcmp(fontname, CF_FALLBACK_FONT_NAME)!=0){
            warn("Could not load font \"%s\", trying \"%s\"",
                 fontname, CF_FALLBACK_FONT_NAME);
            return de_load_font(CF_FALLBACK_FONT_NAME);
        }
        return NULL;
    }
    
    fnt=ALLOC(DEFont);
    
    if(fnt==NULL){
        warn_err();
        return NULL;
    }
    
    fnt->fontset=fontset;
    fnt->fontstruct=fontstruct;
    fnt->pattern=scopy(fontname);
    fnt->next=NULL;
    fnt->prev=NULL;
    fnt->refcount=1;
    
    LINK_ITEM(fonts, fnt, next, prev);
    
    return fnt;
}


bool de_load_font_for_style(DEStyle *style, const char *fontname)
{
    style->font=de_load_font(fontname);

    if(style->font==NULL)
        return FALSE;
    
    if(style->font->fontstruct!=NULL){
        XSetFont(wglobal.dpy, style->normal_gc, 
                 style->font->fontstruct->fid);
    }
    
    return TRUE;
}


void de_free_font(DEFont *font)
{
    if(--font->refcount!=0)
        return;
    
    if(font->fontset!=NULL)
        XFreeFontSet(wglobal.dpy, font->fontset);
    if(font->fontstruct!=NULL)
        XFreeFont(wglobal.dpy, font->fontstruct);
    if(font->pattern!=NULL)
        free(font->pattern);
    
    UNLINK_ITEM(fonts, font, next, prev);
    free(font);
}


/*}}}*/


/*{{{ Lengths */


void debrush_get_font_extents(DEBrush *brush, GrFontExtents *fnte)
{
    if(brush->d->font==NULL){
        DE_RESET_FONT_EXTENTS(fnte);
        return;
    }
    
    defont_get_font_extents(brush->d->font, fnte);
}


void defont_get_font_extents(DEFont *font, GrFontExtents *fnte)
{
    if(font->fontset!=NULL){
        XFontSetExtents *ext=XExtentsOfFontSet(font->fontset);
        if(ext==NULL)
            goto fail;
        fnte->max_height=ext->max_logical_extent.height;
        fnte->max_width=ext->max_logical_extent.width;
        fnte->baseline=-ext->max_logical_extent.y;
        return;
    }else if(font->fontstruct!=NULL){
        XFontStruct *fnt=font->fontstruct;
        fnte->max_height=fnt->ascent+fnt->descent;
        fnte->max_width=fnt->max_bounds.width;
        fnte->baseline=fnt->ascent;
        return;
    }
    
fail:
    DE_RESET_FONT_EXTENTS(fnte);
}


uint debrush_get_text_width(DEBrush *brush, const char *text, uint len)
{
    if(brush->d->font==NULL || text==NULL || len==0)
        return 0;
    
    return defont_get_text_width(brush->d->font, text, len);
}


uint defont_get_text_width(DEFont *font, const char *text, uint len)
{
    if(font->fontset!=NULL){
        XRectangle lext;
#ifdef CF_DE_USE_XUTF8
        if(wglobal.enc_utf8)
            Xutf8TextExtents(font->fontset, text, len, NULL, &lext);
        else
#endif
            XmbTextExtents(font->fontset, text, len, NULL, &lext);
        return lext.width;
    }else if(font->fontstruct!=NULL){
        return XTextWidth(font->fontstruct, text, len);
    }else{
        return 0;
    }
}


/*}}}*/


/*{{{ String drawing */


void debrush_do_draw_string_default(DEBrush *brush, Window win, int x, int y,
                                    const char *str, int len, bool needfill, 
                                    DEColourGroup *colours)
{
    GC gc=brush->d->normal_gc;

    if(brush->d->font==NULL)
        return;
    
    XSetForeground(wglobal.dpy, gc, colours->fg);
    
    if(!needfill){
        if(brush->d->font->fontset!=NULL){
#ifdef CF_DE_USE_XUTF8
            if(wglobal.enc_utf8)
                Xutf8DrawString(wglobal.dpy, win, brush->d->font->fontset,
                                gc, x, y, str, len);
            else
#endif
                XmbDrawString(wglobal.dpy, win, brush->d->font->fontset,
                              gc, x, y, str, len);
        }else if(brush->d->font->fontstruct!=NULL){
            XDrawString(wglobal.dpy, win, gc, x, y, str, len);
        }
    }else{
        XSetBackground(wglobal.dpy, gc, colours->bg);
        if(brush->d->font->fontset!=NULL){
#ifdef CF_DE_USE_XUTF8
            if(wglobal.enc_utf8)
                Xutf8DrawImageString(wglobal.dpy, win, brush->d->font->fontset,
                                     gc, x, y, str, len);
            else
#endif
                XmbDrawImageString(wglobal.dpy, win, brush->d->font->fontset,
                                   gc, x, y, str, len);
        }else if(brush->d->font->fontstruct!=NULL){
            XDrawImageString(wglobal.dpy, win, gc, x, y, str, len);
        }
    }
}


void debrush_do_draw_string(DEBrush *brush, Window win, int x, int y,
                            const char *str, int len, bool needfill, 
                            DEColourGroup *colours)
{
    CALL_DYN(debrush_do_draw_string, brush, (brush, win, x, y, str, len,
                                             needfill, colours));
}


void debrush_draw_string(DEBrush *brush, Window win, int x, int y,
                         const char *str, int len, bool needfill,
                         const char *attrib)
{
    DEColourGroup *cg=debrush_get_colour_group(brush, attrib);
    if(cg!=NULL)
        debrush_do_draw_string(brush, win, x, y, str, len, needfill, cg);
}


/*}}}*/

