/***********************************************************************
*                                                                      *
*               This software is part of the ast package               *
*          Copyright (c) 1985-2012 AT&T Intellectual Property          *
*          Copyright (c) 2020-2023 Contributors to ksh 93u+m           *
*                      and is licensed under the                       *
*                 Eclipse Public License, Version 2.0                  *
*                                                                      *
*                A copy of the License is available at                 *
*      https://www.eclipse.org/org/documents/epl-2.0/EPL-2.0.html      *
*         (with md5 checksum 84283fa8859daf213bdda5a9f8d1be1d)         *
*                                                                      *
*                 Glenn Fowler <gsf@research.att.com>                  *
*                  David Korn <dgk@research.att.com>                   *
*                   Phong Vo <kpv@research.att.com>                    *
*                  Martijn Dekker <martijn@inlv.org>                   *
*                                                                      *
***********************************************************************/

#include	"vmhdr.h"

/*	Opening a new region of allocation.
**	Note that because of possible exotic memory types,
**	all region data must be stored within the space given
**	by the discipline.
**
**	Written by Kiem-Phong Vo, kpv@research.att.com, 01/16/94.
*/

/* this structure lives in the top data segment of the region */
typedef struct _vminit_s
{	union
	{ Vmdata_t	vd;		/* root of usable data space  	*/
	  Vmuchar_t	a[ROUND(sizeof(Vmdata_t),ALIGN)];
	} vd;
	union
	{ Vmalloc_t	vm;		/* embedded region if needed	*/
	  Vmuchar_t	a[ROUND(sizeof(Vmalloc_t),ALIGN)];
	} vm;
	union
	{ Seg_t		seg;		/* space for segment		*/
	  Vmuchar_t	a[ROUND(sizeof(Seg_t),ALIGN)];
	} seg;
	Block_t		block[16];	/* space for a few blocks	*/
} Vminit_t;

Vmalloc_t* vmopen(Vmdisc_t*	disc,	/* discipline to get segments	*/
		  Vmethod_t*	meth,	/* method to manage space	*/
		  int		mode)	/* type of region		*/
{
	Vmalloc_t	*vm, *vmp, vmproto;
	Vmdata_t	*vd;
	Vminit_t	*init;
	size_t		algn, size, incr;
	Block_t		*bp, *np;
	Seg_t		*seg;
	Vmuchar_t	*addr;
	int		rv, mt;

	if(!meth || !disc || !disc->memoryf )
		return NULL;

	GETPAGESIZE(_Vmpagesize);

	mode = (mode&VM_FLAGS) | meth->meth; /* start with user-settable flags */

	vmp = &vmproto; /* avoid memory allocation here! */
	memset(vmp, 0, sizeof(Vmalloc_t));
	memcpy(&vmp->meth, meth, sizeof(Vmethod_t));
	mt = vmp->meth.meth;
	vmp->meth.meth = 0;
	vmp->disc = disc;

	size = 0;

	if(disc->exceptf)
	{	addr = NULL;
		if((rv = (*disc->exceptf)(vmp,VM_OPEN,&addr,disc)) < 0)
			return NULL;
		else if(rv == 0 )
		{	if(addr) /* vm itself is in memory from disc->memoryf */
				mode |= VM_MEMORYF;
		}
		else if(rv > 0) /* the data section is being restored */
		{	if(!(init = (Vminit_t*)addr) )
				return NULL;
			size = -1; /* to tell that addr was not from disc->memoryf */
			vd = &init->vd.vd; /**/ASSERT(VLONG(vd)%ALIGN == 0);
			goto done;
		}
	}

	/* make sure vd->incr is properly rounded and get initial memory */
	incr = disc->round <= 0 ? _Vmpagesize : disc->round;
	incr = MULTIPLE(incr,ALIGN);
	size = ROUND(sizeof(Vminit_t),incr); /* get initial memory */
	if(!(addr = (Vmuchar_t*)(*disc->memoryf)(vmp, NULL, 0, size, disc)) )
		return NULL;
	memset(addr, 0, size);

	/* initialize region data */
	algn = (size_t)(VLONG(addr)%ALIGN);
	init = (Vminit_t*)(addr + (algn ? ALIGN-algn : 0)); /**/ASSERT(VLONG(init)%ALIGN == 0);
	vd = &init->vd.vd; /**/ASSERT(VLONG(vd)%ALIGN == 0);
	vd->mode = mode | meth->meth;
	vd->incr = incr;
	vd->pool = 0;
	vd->free = vd->wild = NULL;

	if(vd->mode&(VM_MTBEST|VM_MTDEBUG|VM_MTPROFILE))
	{	int	k;
		vd->root = NULL;
		for(k = S_TINY-1; k >= 0; --k)
			TINY(vd)[k] = NULL;
		for(k = S_CACHE; k >= 0; --k)
			CACHE(vd)[k] = NULL;
	}

	vd->seg = &init->seg.seg; /**/ ASSERT(VLONG(vd->seg)%ALIGN == 0);
	seg = vd->seg;
	seg->next = NULL;
	seg->vmdt = vd;
	seg->addr = addr;
	seg->extent = size;
	seg->baddr = addr + size;
	seg->size = size; /* Note: this size is unusually large to mark seg as
			   the root segment and can be freed only at closing */
	seg->free = NULL;

	/* make a data block out of the remainder */
	bp = SEGBLOCK(seg);
	SEG(bp) = seg;
	size = ((seg->baddr - (Vmuchar_t*)bp)/ALIGN) * ALIGN; /**/ ASSERT(size > 0);
	SIZE(bp) = size - 2*sizeof(Head_t); /**/ASSERT(SIZE(bp) > 0 && (SIZE(bp)%ALIGN) == 0);
	SELF(bp) = bp;
	/**/ ASSERT(SIZE(bp)%ALIGN == 0);
	/**/ ASSERT(VLONG(bp)%ALIGN == 0);

	/* make a fake header for next block in case of noncontiguous segments */
	np = NEXT(bp);
	SEG(np) = seg;
	SIZE(np) = BUSY|PFREE;

	if(vd->mode&(VM_MTLAST|VM_MTPOOL))
		seg->free = bp;
	else	vd->wild = bp;

	vmp->meth.meth = mt;

done:	/* now make the region handle */
	if(vd->mode&VM_MEMORYF)
		vm = &init->vm.vm;
	else if(!(vm = vmalloc(Vmheap, sizeof(Vmalloc_t))) )
	{	if(size > 0)
			(void)(*disc->memoryf)(vmp, addr, size, 0, disc);
		return NULL;
	}
	memcpy(vm, vmp, sizeof(Vmalloc_t));
	vm->data = vd;

	if(disc->exceptf) /* signaling that vmopen succeeded */
		(void)(*disc->exceptf)(vm, VM_ENDOPEN, NULL, disc);

	/* add to the linked list of regions */
	_vmlock(NULL, 1);
	vm->next = Vmheap->next; Vmheap->next = vm;
	_vmlock(NULL, 0);

	return vm;
}
