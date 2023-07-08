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

static char*	Version = "\n@(#)$Id: Vmalloc (AT&T Labs - Research) 2011-08-08 $\0\n";


/*	Private code used in the vmalloc library
**
**	Written by Kiem-Phong Vo, kpv@research.att.com, 01/16/94.
*/

/* Get more memory for a region */
static Block_t* _vmextend(Vmalloc_t*	vm,		/* region to increase in size	*/
			  size_t		size,		/* desired amount of space	*/
			  Vmsearch_f		searchf)	/* tree search function		*/
{
	size_t	s;
	Seg_t*	seg;
	Block_t	*bp, *tp, *np;
	Vmuchar_t*	addr = (Vmuchar_t*)Version; /* shut compiler warning */
	Vmdata_t*	vd = vm->data;

	GETPAGESIZE(_Vmpagesize);

	if(vd->incr <= 0) /* this is just _Vmheap on the first call */
		vd->incr = _Vmpagesize*sizeof(void*);

	/* Get slightly more for administrative data */
	s = size + sizeof(Seg_t) + sizeof(Block_t) + sizeof(Head_t) + 2*ALIGN;
	if(s <= size)	/* size was too large and we have wrapped around */
		return NULL;
	if((size = ROUND(s,vd->incr)) < s)
		return NULL;

	/* increase the rounding factor to reduce # of future extensions */
	if(size > 2*vd->incr && vm->disc->round < vd->incr)
		vd->incr *= 2;

	if(!(seg = vd->seg) ) /* there is no current segment */
		addr = NULL;
	else /* see if we can extend the current segment */
	{	addr = (Vmuchar_t*)(*vm->disc->memoryf)(vm,seg->addr,seg->extent,
					  		seg->extent+size,vm->disc);
		if(addr == (Vmuchar_t*)seg->addr)
			addr += seg->extent; /* seg successfully extended */
		else	seg = NULL; /* a new segment was created */
	}

	if(!addr) /* create a new segment */
	{	if(!(addr = (Vmuchar_t*)(*vm->disc->memoryf)(vm, NULL, 0, size, vm->disc)) )
		{	if(vm->disc->exceptf) /* announce that no more memory is available */
			{
				CLRLOCK(vm, 0);
				(void)(*vm->disc->exceptf)(vm, VM_NOMEM, (void*)size, vm->disc);
				SETLOCK(vm, 0);
			}
			return NULL;
		}
	}

	if(seg)
	{	/* extending current segment */
		bp = BLOCK(seg->baddr);

		if(vd->mode&(VM_MTBEST|VM_MTDEBUG|VM_MTPROFILE) )
		{	/**/ ASSERT((SIZE(bp)&~BITS) == 0);
			/**/ ASSERT(SEG(bp) == seg);

			if(!ISPFREE(SIZE(bp)) )
				SIZE(bp) = size - sizeof(Head_t);
			else
			{	/**/ ASSERT(searchf);
				bp = LAST(bp);
				if(bp == vd->wild)
					vd->wild = NULL;
				else	REMOVE(vd,bp,INDEX(SIZE(bp)),tp,(*searchf));
				SIZE(bp) += size;
			}
		}
		else
		{	if(seg->free)
			{	bp = seg->free;
				seg->free = NULL;
				SIZE(bp) += size;
			}
			else
			{	SEG(bp) = seg;
				SIZE(bp) = size - sizeof(Head_t);
			}
		}

		seg->size += size;
		seg->extent += size;
		seg->baddr += size;
	}
	else
	{	/* creating a new segment */
		Seg_t	*sp, *lastsp;

		if((s = (size_t)(VLONG(addr)%ALIGN)) != 0)
			addr += ALIGN-s;

		seg = (Seg_t*)addr;
		seg->vmdt = vd;
		seg->addr = addr - (s ? ALIGN-s : 0);
		seg->extent = size;
		seg->baddr = addr + size - (s ? 2*ALIGN : 0);
		seg->free = NULL;
		bp = SEGBLOCK(seg);
		SEG(bp) = seg;
		SIZE(bp) = seg->baddr - (Vmuchar_t*)bp - 2*sizeof(Head_t);

		/* NOTE: for Vmbest, Vmdebug and Vmprofile the region's segment list
		   is reversely ordered by addresses. This is so that we can easily
		   check for the wild block.
		*/
		lastsp = NULL;
		sp = vd->seg;
		if(vd->mode&(VM_MTBEST|VM_MTDEBUG|VM_MTPROFILE))
			for(; sp; lastsp = sp, sp = sp->next)
				if(seg->addr > sp->addr)
					break;
		seg->next = sp;
		if(lastsp)
			lastsp->next = seg;
		else	vd->seg = seg;

		seg->size = SIZE(bp);
	}

	/* make a fake header for possible segmented memory */
	tp = NEXT(bp);
	SEG(tp) = seg;
	SIZE(tp) = BUSY;

	/* see if the wild block is still wild */
	if((tp = vd->wild) && (seg = SEG(tp)) != vd->seg)
	{	np = NEXT(tp);
		CLRPFREE(SIZE(np));
		if(vd->mode&(VM_MTBEST|VM_MTDEBUG|VM_MTPROFILE) )
		{	SIZE(tp) |= BUSY|JUNK;
			LINK(tp) = CACHE(vd)[C_INDEX(SIZE(tp))];
			CACHE(vd)[C_INDEX(SIZE(tp))] = tp;
		}
		else	seg->free = tp;

		vd->wild = NULL;
	}

	return bp;
}

/* Truncate a segment if possible */
static ssize_t _vmtruncate(Vmalloc_t*	vm,	/* containing region		*/
			   Seg_t*	seg,	/* the one to be truncated	*/
			   size_t	size,	/* amount of free space		*/
			   int		exact)
{
	void*	caddr;
	Seg_t*	last;
	Vmdata_t*	vd = vm->data;
	Vmemory_f	memoryf = vm->disc->memoryf;

	caddr = seg->addr;

	if(size < seg->size)
	{	ssize_t	less;

		if(exact)
			less = size;
		else /* keep truncated amount to discipline requirements */
		{	if((less = vm->disc->round) <= 0)
				less = _Vmpagesize;
			less = (size/less)*less;
			less = (less/vd->incr)*vd->incr;
			if(less > 0 && (ssize_t)size > less && (size-less) < sizeof(Block_t) )
				less = less <= (ssize_t)vd->incr ? 0 : less - vd->incr;
		}

		if(less <= 0 ||
		   (*memoryf)(vm,caddr,seg->extent,seg->extent-less,vm->disc) != caddr)
			return 0;

		seg->extent -= less;
		seg->size -= less;
		seg->baddr -= less;
		SEG(BLOCK(seg->baddr)) = seg;
		SIZE(BLOCK(seg->baddr)) = BUSY;

		return less;
	}
	else
	{	/* unlink segment from region */
		if(seg == vd->seg)
		{	vd->seg = seg->next;
			last = NULL;
		}
		else
		{	for(last = vd->seg; last->next != seg; last = last->next)
				;
			last->next = seg->next;
		}

		/* now delete it */
		if((*memoryf)(vm,caddr,seg->extent,0,vm->disc) == caddr)
			return size;

		/* space reduction failed, reinsert segment */
		if(last)
		{	seg->next = last->next;
			last->next = seg;
		}
		else
		{	seg->next = vd->seg;
			vd->seg = seg;
		}
		return 0;
	}
}

int _vmlock(Vmalloc_t* vm, int locking)
{
	if(!vm) /* some sort of global locking */
	{	if(!locking) /* turn off lock */
			asolock(&_Vmlock, 1, ASO_UNLOCK);
		else	asolock(&_Vmlock, 1, ASO_SPINLOCK);
	}
	else if(vm->data->mode&VM_SHARE)
	{	if(!locking) /* turning off the lock */
			asolock(&vm->data->lock, 1, ASO_UNLOCK);
		else	asolock(&vm->data->lock, 1, ASO_SPINLOCK);
	}
	else
	{	if(!locking)
			vm->data->lock = 0;
		else	vm->data->lock = 1;
	}
	return 0;
}


/* Externally visible names but local to library */
Vmextern_t	_Vmextern =
{	_vmextend,						/* _Vmextend	*/
	_vmtruncate,						/* _Vmtruncate	*/
	0,							/* _Vmpagesize	*/
	NULL,							/* _Vmstrcpy	*/
	NULL,							/* _Vmitoa	*/
	NULL,							/* _Vmtrace	*/
	NULL							/* _Vmpfclose	*/
};
