/*
 * ion/panews/splitext.c
 *
 * Copyright (c) Tuomo Valkonen 1999-2004. 
 *
 * Ion is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 */

#include <string.h>
#include <limits.h>
#include <libtu/objp.h>
#include <libtu/minmax.h>
#include <ioncore/common.h>
#include <ioncore/global.h>
#include <ioncore/rootwin.h>
#include <ioncore/xwindow.h>
#include <ioncore/window.h>
#include <ioncore/region-iter.h>
#include <mod_ionws/split.h>
#include "splitfloat.h"
#include "panehandle.h"


#define GEOM(X) (((WSplit*)(X))->geom)


/*{{{�Init/deinit */


static void splitfloat_set_borderlines(WSplitFloat *split)
{
    int dir=split->ssplit.dir;
    
    split->tlpwin->bline=(dir==SPLIT_HORIZONTAL
                          ? GR_BORDERLINE_RIGHT
                          : GR_BORDERLINE_BOTTOM);

    split->brpwin->bline=(dir==SPLIT_HORIZONTAL
                          ? GR_BORDERLINE_LEFT
                          : GR_BORDERLINE_TOP);
}


bool splitfloat_init(WSplitFloat *split, const WRectangle *geom, 
                     WPaneWS *ws, int dir)
{
    WWindow *par=REGION_PARENT(ws);
    WFitParams fp;
    
    assert(par!=NULL);

    fp.g=*geom;
    fp.mode=REGION_FIT_EXACT;
    split->tlpwin=create_panehandle(par, &fp);
    if(split->tlpwin==NULL)
        return FALSE;

    fp.g=*geom;
    fp.mode=REGION_FIT_EXACT;
    split->brpwin=create_panehandle(par, &fp);
    if(split->brpwin==NULL){
        destroy_obj((Obj*)split->tlpwin);
        return FALSE;
    }
    
    if(!splitsplit_init(&(split->ssplit), geom, dir)){
        destroy_obj((Obj*)split->brpwin);
        destroy_obj((Obj*)split->tlpwin);
        return FALSE;
    }

    split->tlpwin->splitfloat=split;
    split->brpwin->splitfloat=split;
    /*ionws_managed_add((WIonWS*)ws, (WRegion*)split->tlpwin);
    ionws_managed_add((WIonWS*)ws, (WRegion*)split->brpwin);*/
    
    splitfloat_set_borderlines(split);

    if(REGION_IS_MAPPED(ws)){
        region_map((WRegion*)(split->tlpwin));
        region_map((WRegion*)(split->brpwin));
    }
    
    return TRUE;
}


WSplitFloat *create_splitfloat(const WRectangle *geom, WPaneWS *ws, int dir)
{
    CREATEOBJ_IMPL(WSplitFloat, splitfloat, (p, geom, ws, dir));
}


void splitfloat_deinit(WSplitFloat *split)
{
    if(split->tlpwin!=NULL){
        WPaneHandle *tmp=split->tlpwin;
        split->tlpwin=NULL;
        tmp->splitfloat=NULL;
        destroy_obj((Obj*)tmp);
    }

    if(split->brpwin!=NULL){
        WPaneHandle *tmp=split->brpwin;
        split->brpwin=NULL;
        tmp->splitfloat=NULL;
        destroy_obj((Obj*)tmp);
    }
    
    splitsplit_deinit(&(split->ssplit));
}


/*}}}*/


/*{{{ X window handling */


static void stack_stacking_reg(WRegion *reg, 
                               Window *bottomret, Window *topret)
{
    Window b=None, t=None;
    
    if(reg!=NULL){
        region_stacking(reg, &b, &t);
        if(*bottomret==None)
            *bottomret=b;
        if(t!=None)
            *topret=t;
    }
}


static void stack_stacking_split(WSplit *split,
                                 Window *bottomret, Window *topret)
{
    Window b=None, t=None;
    
    if(split!=NULL){
        split_stacking(split, &b, &t);
        if(*bottomret==None)
            *bottomret=b;
        if(t!=None)
            *topret=t;
    }
}


static void splitfloat_stacking(WSplitFloat *split, 
                                Window *bottomret, Window *topret)
{
    *bottomret=None;
    *topret=None;
    
    if(split->ssplit.current!=SPLIT_CURRENT_TL){
        stack_stacking_reg((WRegion*)split->tlpwin, bottomret, topret);
        stack_stacking_split(split->ssplit.tl, bottomret, topret);
        stack_stacking_reg((WRegion*)split->brpwin, bottomret, topret);
        stack_stacking_split(split->ssplit.br, bottomret, topret);
    }else{
        stack_stacking_reg((WRegion*)split->brpwin, bottomret, topret);
        stack_stacking_split(split->ssplit.br, bottomret, topret);
        stack_stacking_reg((WRegion*)split->tlpwin, bottomret, topret);
        stack_stacking_split(split->ssplit.tl, bottomret, topret);
    }
}


static void stack_restack_reg(WRegion *reg, Window *other, int *mode)
{
    Window b=None, t=None;
    
    if(reg!=NULL){
        region_restack(reg, *other, *mode);
        region_stacking(reg, &b, &t);
        if(t!=None){
            *other=t;
            *mode=Above;
        }
    }
}


static void stack_restack_split(WSplit *split, Window *other, int *mode)
{
    Window b=None, t=None;
    
    if(split!=NULL){
        split_restack(split, *other, *mode);
        split_stacking(split, &b, &t);
        if(t!=None){
            *other=t;
            *mode=Above;
        }
    }
}



static void splitfloat_restack(WSplitFloat *split, Window other, int mode)
{
    if(split->ssplit.current!=SPLIT_CURRENT_TL){
        stack_restack_reg((WRegion*)split->tlpwin, &other, &mode);
        stack_restack_split(split->ssplit.tl, &other, &mode);
        stack_restack_reg((WRegion*)split->brpwin, &other, &mode);
        stack_restack_split(split->ssplit.br, &other, &mode);
    }else{
        stack_restack_reg((WRegion*)split->brpwin, &other, &mode);
        stack_restack_split(split->ssplit.br, &other, &mode);
        stack_restack_reg((WRegion*)split->tlpwin, &other, &mode);
        stack_restack_split(split->ssplit.tl, &other, &mode);
    }
}


static void splitfloat_map(WSplitFloat *split)
{
    region_map((WRegion*)(split->tlpwin));
    region_map((WRegion*)(split->brpwin));
    splitinner_forall((WSplitInner*)split, split_map);
}


static void splitfloat_unmap(WSplitFloat *split)
{
    region_unmap((WRegion*)(split->tlpwin));
    region_unmap((WRegion*)(split->brpwin));
    splitinner_forall((WSplitInner*)split, split_unmap);
}


static void reparentreg(WRegion *reg, WWindow *target)
{
    WRectangle g=REGION_GEOM(reg);
    region_reparent(reg, target, &g, REGION_FIT_EXACT);
}


static void splitfloat_reparent(WSplitFloat *split, WWindow *target)
{
    if(split->ssplit.current!=SPLIT_CURRENT_TL){
        reparentreg((WRegion*)split->tlpwin, target);
        split_reparent(split->ssplit.tl, target);
        reparentreg((WRegion*)split->brpwin, target);
        split_reparent(split->ssplit.br, target);
    }else{
        reparentreg((WRegion*)split->brpwin, target);
        split_reparent(split->ssplit.br, target);
        reparentreg((WRegion*)split->tlpwin, target);
        split_reparent(split->ssplit.tl, target);
    }
}


/*}}}*/


/*{{{ Geometry */


void splitfloat_tl_pwin_to_cnt(WSplitFloat *split, WRectangle *g)
{
    if(split->ssplit.dir==SPLIT_HORIZONTAL)
        g->w=maxof(1, g->w-split->tlpwin->bdw.right);
    else
        g->h=maxof(1, g->h-split->tlpwin->bdw.bottom);
}


void splitfloat_br_pwin_to_cnt(WSplitFloat *split, WRectangle *g)
{
    if(split->ssplit.dir==SPLIT_HORIZONTAL){
        int delta=split->tlpwin->bdw.left;
        g->w=maxof(1, g->w-delta);
        g->x+=delta;
    }else{
        int delta=split->tlpwin->bdw.top;
        g->h=maxof(1, g->h-delta);
        g->y+=delta;
    }
}


void splitfloat_tl_cnt_to_pwin(WSplitFloat *split, WRectangle *g)
{
    if(split->ssplit.dir==SPLIT_HORIZONTAL)
        g->w=maxof(1, g->w+split->tlpwin->bdw.right);
    else
        g->h=maxof(1, g->h+split->tlpwin->bdw.bottom);
}


void splitfloat_br_cnt_to_pwin(WSplitFloat *split, WRectangle *g)
{
    if(split->ssplit.dir==SPLIT_HORIZONTAL){
        int delta=split->tlpwin->bdw.left;
        g->w=maxof(1, g->w+delta);
        g->x-=delta;
    }else{
        int delta=split->tlpwin->bdw.top;
        g->h=maxof(1, g->h+delta);
        g->y-=delta;
    }
}

    
static int infadd(int x, int y)
{
    return ((x==INT_MAX || y==INT_MAX) ? INT_MAX : (x+y));
}


static int splitfloat_get_handle(WSplitFloat *split, int dir, 
                                  WSplit *other)
{
    assert(other==split->ssplit.tl || other==split->ssplit.br);
    
    if(dir!=split->ssplit.dir)
        return 0;

    if(dir==SPLIT_VERTICAL){
        if(other==split->ssplit.tl)
            return split->tlpwin->bdw.right;
        else if(other==split->ssplit.br)
            return split->tlpwin->bdw.left;
    }else{
        if(other==split->ssplit.tl)
            return split->tlpwin->bdw.bottom;
        else if(other==split->ssplit.br)
            return split->tlpwin->bdw.top;
    }
    
    return 0;
}


static int splitfloat_get_max(WSplitFloat *split, int dir, WSplit *other)
{
    return infadd((dir==SPLIT_VERTICAL ? other->max_h : other->max_w),
                  splitfloat_get_handle(split, dir, other));
}


static int splitfloat_get_min(WSplitFloat *split, int dir, WSplit *other)
{
    return ((dir==SPLIT_VERTICAL ? other->min_h : other->min_w)
            +splitfloat_get_handle(split, dir, other));
}


static void splitfloat_update_bounds(WSplitFloat *split, bool recursive)
{
    WSplit *tl=split->ssplit.tl, *br=split->ssplit.br;
    WSplit *node=(WSplit*)split;
    int tl_max_w, br_max_w, tl_max_h, br_max_h;
    int tl_min_w, br_min_w, tl_min_h, br_min_h;
    
    if(recursive){
        split_update_bounds(tl, recursive);
        split_update_bounds(br, recursive);
    }

    tl_max_w=splitfloat_get_max(split, SPLIT_HORIZONTAL, tl);
    br_max_w=splitfloat_get_max(split, SPLIT_HORIZONTAL, br);
    tl_max_h=splitfloat_get_max(split, SPLIT_VERTICAL, tl);
    br_max_h=splitfloat_get_max(split, SPLIT_VERTICAL, br);
    tl_min_w=splitfloat_get_min(split, SPLIT_HORIZONTAL, tl);
    br_min_w=splitfloat_get_min(split, SPLIT_HORIZONTAL, br);
    tl_min_h=splitfloat_get_min(split, SPLIT_VERTICAL, tl);
    br_min_h=splitfloat_get_min(split, SPLIT_VERTICAL, br);
    
    if(split->ssplit.dir==SPLIT_HORIZONTAL){
        node->max_w=infadd(tl_max_w, br_max_w);
        node->min_w=minof(tl_min_w, br_min_w);
        node->unused_w=0;
        node->min_h=maxof(tl_min_h, br_min_h);
        node->max_h=maxof(minof(tl_max_h, br_max_h), node->min_h);
        node->unused_h=minof(tl->unused_h, br->unused_h);
    }else{
        node->max_h=infadd(tl_max_h, br_max_h);
        node->min_h=minof(tl_min_h, br_min_h);
        node->unused_h=0;
        node->min_w=maxof(tl_min_w, br_min_w);
        node->max_w=maxof(minof(tl_max_w, br_max_w), node->min_w);
        node->unused_w=minof(tl->unused_w, br->unused_w);
    }
}


void splitfloat_update_handles(WSplitFloat *split, const WRectangle *tlg_,
                               const WRectangle *brg_)
{
    WRectangle tlg=*tlg_, brg=*brg_;
    
    if(split->ssplit.dir==SPLIT_HORIZONTAL){
        tlg.w=split->tlpwin->bdw.right;
        tlg.x=tlg_->x+tlg_->w-tlg.w;
        brg.w=split->brpwin->bdw.left;
    }else{
        tlg.h=split->tlpwin->bdw.bottom;
        tlg.y=tlg_->y+tlg_->h-tlg.h;
        brg.h=split->brpwin->bdw.top;
    }
    
    region_fit((WRegion*)split->tlpwin, &tlg, REGION_FIT_EXACT);
    region_fit((WRegion*)split->brpwin, &brg, REGION_FIT_EXACT);
}


static void bound(int *what, int min, int max)
{
    if(*what<min)
        *what=min;
    else if(*what>max)
        *what=max;
}


static void adjust_sizes(int *tls_, int *brs_, int nsize, 
                         int tlmin, int brmin, int tlmax, int brmax,
                         int primn)
{
    int tls=maxof(0, *tls_);
    int brs=maxof(0, *brs_);
    nsize=maxof(1, nsize);
    
    if(primn==PRIMN_TL){
        tls=maxof(1, nsize-brs);
        bound(&tls, tlmin, tlmax);
        brs=nsize-tls;
        bound(&brs, brmin, brmax);
        tls=nsize-brs;
        bound(&tls, tlmin, tlmax);
    }else if(primn==PRIMN_BR){
        brs=maxof(1, nsize-tls);
        bound(&brs, brmin, brmax);
        tls=nsize-brs;
        bound(&tls, tlmin, tlmax);
        brs=nsize-tls;
        bound(&brs, brmin, brmax);
    }else{ /* && PRIMN_ANY */
        tls=tls*nsize/maxof(2, tls+brs);
        bound(&tls, tlmin, tlmax);
        brs=nsize-tls;
        bound(&brs, brmin, brmax);
        tls=nsize-brs;
        bound(&tls, tlmin, tlmax);
    }
    
    *tls_=tls;
    *brs_=brs;
}


static void adjust_size(int *sz, int dir, WSplitFloat *f, WSplit *s)
{
    int mi=splitfloat_get_min(f, dir, s);
    int ma=splitfloat_get_max(f, dir, s);
    *sz=maxof(mi, minof(*sz, ma));
}


static void splitfloat_do_resize(WSplitFloat *split, const WRectangle *ng, 
                                 int hprimn, int vprimn, bool transpose)
{
    WRectangle tlg=GEOM(split->ssplit.tl);
    WRectangle brg=GEOM(split->ssplit.br);
    WRectangle ntlg=*ng, nbrg=*ng;
    WRectangle *og=&((WSplit*)split)->geom;
    int dir=split->ssplit.dir;
    bool adjust=TRUE;
    
    splitfloat_tl_cnt_to_pwin(split, &tlg);
    splitfloat_br_cnt_to_pwin(split, &brg);

    if(transpose){
        if(dir==SPLIT_VERTICAL){
            dir=SPLIT_HORIZONTAL;
            split->tlpwin->bline=GR_BORDERLINE_RIGHT;
            split->brpwin->bline=GR_BORDERLINE_LEFT;
        }else{
            dir=SPLIT_VERTICAL;
            split->tlpwin->bline=GR_BORDERLINE_BOTTOM;
            split->brpwin->bline=GR_BORDERLINE_TOP;
        }
        split->ssplit.dir=dir;
    }

    if(dir==SPLIT_VERTICAL){
        if(ng->h<=tlg.h+brg.h){
            if(transpose){
                ntlg.h=minof(tlg.w, ng->h*2/3);
                nbrg.h=minof(brg.w, ng->h*2/3);
                adjust_size(&ntlg.h, dir, split, split->ssplit.tl);
                adjust_size(&nbrg.h, dir, split, split->ssplit.br);
                adjust=(ng->h>ntlg.h+nbrg.h);
            }else{
                ntlg.h=minof(ng->h, tlg.h);
                nbrg.h=minof(ng->h, brg.h);
                adjust=FALSE;
            }
        }else{
            ntlg.h=tlg.h;
            nbrg.h=brg.h;
        }

        if(adjust){
            adjust_sizes(&ntlg.h, &nbrg.h, ng->h,
                         splitfloat_get_min(split, dir, split->ssplit.tl),
                         splitfloat_get_min(split, dir, split->ssplit.br),
                         splitfloat_get_max(split, dir, split->ssplit.tl),
                         splitfloat_get_max(split, dir, split->ssplit.br),
                         vprimn);
        }
        
        nbrg.y=ng->y+ng->h-nbrg.h;
    }else{
        if(ng->w<=tlg.w+brg.w){
            if(transpose){
                ntlg.w=minof(tlg.h, ng->w*2/3);
                nbrg.w=minof(brg.h, ng->w*2/3);
                adjust_size(&ntlg.w, dir, split, split->ssplit.tl);
                adjust_size(&nbrg.w, dir, split, split->ssplit.br);
                adjust=(ng->w>ntlg.w+nbrg.w);
            }else{
                ntlg.w=minof(ng->w, tlg.w);
                nbrg.w=minof(ng->w, brg.w);
                adjust=FALSE;
            }
        }else{
            ntlg.w=tlg.w;
            nbrg.w=brg.w;
        }
        
        if(adjust){
            adjust_sizes(&ntlg.w, &nbrg.w, ng->w,
                         splitfloat_get_min(split, dir, split->ssplit.tl),
                         splitfloat_get_min(split, dir, split->ssplit.br),
                         splitfloat_get_max(split, dir, split->ssplit.tl),
                         splitfloat_get_max(split, dir, split->ssplit.br),
                         hprimn);
        }
        
        nbrg.x=ng->x+ng->w-nbrg.w;
    }

    GEOM(split)=*ng;

    splitfloat_update_handles(split, &ntlg, &nbrg);

    splitfloat_tl_pwin_to_cnt(split, &ntlg);
    split_do_resize(split->ssplit.tl, &ntlg, hprimn, vprimn, transpose);
    splitfloat_br_pwin_to_cnt(split, &nbrg);
    split_do_resize(split->ssplit.br, &nbrg, hprimn, vprimn, transpose);
}


static void calc_amount(int *amount, int *oamount,
                        int rs, WSplitSplit *p, int omax,
                        const WRectangle *ng, const WRectangle *og)
{
    *oamount=0;
    
    if(rs>=0){
        if(p->dir==SPLIT_VERTICAL)
            *amount=maxof(0, minof(rs, GEOM(p).h-ng->h));
        else
            *amount=maxof(0, minof(rs, GEOM(p).w-ng->w));
    }else{
        if(p->dir==SPLIT_VERTICAL){
            int overlap=maxof(0, og->h-(GEOM(p).h-ng->h));
            *amount=-minof(-rs, overlap);
            *oamount=maxof(0, minof(*amount-rs, omax-og->h));
            *amount-=*oamount;
        }else{
            int overlap=maxof(0, og->w-(GEOM(p).w-ng->w));
            *amount=-minof(-rs, overlap);
            *oamount=maxof(0, minof(*amount-rs, omax-og->w));
            *amount-=*oamount;
        }
    }
}


static void splitfloat_do_rqsize(WSplitFloat *split, WSplit *node, 
                                 RootwardAmount *ha, RootwardAmount *va, 
                                 WRectangle *rg, bool tryonly)
{
    int hprimn=PRIMN_ANY, vprimn=PRIMN_ANY;
    WRectangle pg, og, ng, nog, nng;
    RootwardAmount *ca;
    WSplit *other;
    int amount=0, oamount=0, omax;
    int thisnode;
    WSplitSplit *p=&(split->ssplit);
    
    assert(!ha->any || ha->tl==0);
    assert(!va->any || va->tl==0);
    assert(p->tl==node || p->br==node);
    
    if(p->tl==node){
        other=p->br;
        thisnode=PRIMN_TL;
    }else{
        other=p->tl;
        thisnode=PRIMN_BR;
    }

    ng=GEOM(node);
    og=GEOM(other);
    
    if(thisnode==PRIMN_TL){
        splitfloat_tl_cnt_to_pwin(split, &ng);
        splitfloat_br_cnt_to_pwin(split, &og);
    }else{
        splitfloat_br_cnt_to_pwin(split, &ng);
        splitfloat_tl_cnt_to_pwin(split, &og);
    }

    ca=(p->dir==SPLIT_VERTICAL ? va : ha);
    
    omax=splitfloat_get_max(split, p->dir, other);
    
    if(thisnode==PRIMN_TL || ca->any){
        calc_amount(&amount, &oamount, ca->br, p, omax, &ng, &og);
        ca->br-=amount;
    }else/*if(thisnode==PRIMN_BR)*/{
        calc_amount(&amount, &oamount, ca->tl, p, omax, &ng, &og);
        ca->tl-=amount;
    }
    
    if(((WSplit*)p)->parent==NULL /*|| 
       (ha->tl==0 && ha->br==0 && va->tl==0 && va->br==0)*/){
        pg=((WSplit*)p)->geom;
    }else{
        splitinner_do_rqsize(((WSplit*)p)->parent, (WSplit*)p, ha, va,
                             &pg, tryonly);
    }
    
    assert(pg.w>=0 && pg.h>=0);

    nog=pg;
    nng=pg;

    if(p->dir==SPLIT_VERTICAL){
        nog.h=minof(pg.h, maxof(0, og.h+oamount));
        nng.h=minof(pg.h, maxof(0, ng.h+amount+pg.h-GEOM(p).h));
        if(thisnode==PRIMN_TL)
            nog.y=pg.y+pg.h-nog.h;
        else
            nng.y=pg.y+pg.h-nng.h;
        vprimn=thisnode;
    }else{
        nog.w=minof(pg.w, maxof(0, og.w+oamount));
        nng.w=minof(pg.w, maxof(0, ng.w+amount+pg.w-GEOM(p).w));
        if(thisnode==PRIMN_TL)
            nog.x=pg.x+pg.w-nog.w;
        else
            nng.x=pg.x+pg.w-nng.w;
        hprimn=thisnode;
    }
    
    if(!tryonly){
        GEOM(p)=pg;

        if(thisnode==PRIMN_TL){
            splitfloat_update_handles(split, &nng, &nog);
            splitfloat_br_pwin_to_cnt(split, &nog);
        }else{
            splitfloat_update_handles(split, &nog, &nng);
            splitfloat_tl_pwin_to_cnt(split, &nog);
        }
            
        /* Ent� jos 'other' on stdisp? */
        split_do_resize(other, &nog, hprimn, vprimn, FALSE);
    }
    
    *rg=nng;
    if(thisnode==PRIMN_TL)
        splitfloat_tl_pwin_to_cnt(split, rg);
    else
        splitfloat_br_pwin_to_cnt(split, rg);
}


void splitfloat_flip(WSplitFloat *split)
{
    WRectangle tlg, brg;
    
    splitsplit_flip_default(&split->ssplit);
    
    tlg=split->ssplit.tl->geom;
    brg=split->ssplit.br->geom;

    splitfloat_tl_cnt_to_pwin(split, &tlg);
    splitfloat_br_cnt_to_pwin(split, &brg);
    splitfloat_update_handles(split, &tlg, &brg);
}


/*}}}*/


/*{{{ The class */


static DynFunTab splitfloat_dynfuntab[]={
    {split_update_bounds, splitfloat_update_bounds},
    {split_do_resize, splitfloat_do_resize},
    {splitinner_do_rqsize, splitfloat_do_rqsize},
    {split_stacking, splitfloat_stacking},
    {split_restack, splitfloat_restack},
    {split_reparent, splitfloat_reparent},
    {split_map, splitfloat_map},
    {split_unmap, splitfloat_unmap},
    {splitsplit_flip, splitfloat_flip},
    END_DYNFUNTAB,
};


IMPLCLASS(WSplitFloat, WSplitSplit, splitfloat_deinit, splitfloat_dynfuntab);


/*}}}*/
