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

/*	Method to profile space usage.
**
**	Written by Kiem-Phong Vo, kpv@research.att.com, 03/23/94.
*/

#define PFHASH(pf)	((pf)->data.data.hash)
#define PFVM(pf)	((pf)->data.data.vm)
#define PFFILE(pf)	((pf)->data.data.fm.file)
#define PFLINE(pf)	((pf)->line)
#define PFNAME(pf)	((pf)->data.f)
#define PFNALLOC(pf)	((pf)->data.data.nalloc)
#define PFALLOC(pf)	((pf)->data.data.alloc)
#define PFNFREE(pf)	((pf)->data.data.nfree)
#define PFFREE(pf)	((pf)->data.data.free)
#define PFREGION(pf)	((pf)->data.data.region)
#define PFMAX(pf)	((pf)->data.data.fm.max)

typedef struct _pfdata_s	Pfdata_t;
struct _pfdata_s
{	Vmulong_t	hash;	/* hash value			*/
	union
	{ char*		file;	/* file name			*/
	  Vmulong_t	max;	/* max busy space for region	*/
	} fm;
	Vmalloc_t*	vm;	/* region alloc from 		*/
	Pfobj_t*	region;	/* pointer to region record	*/
	Vmulong_t	nalloc;	/* number of alloc calls	*/
	Vmulong_t	alloc;	/* amount allocated		*/
	Vmulong_t	nfree;	/* number of free calls		*/
	Vmulong_t	free;	/* amount freed			*/
};
struct _pfobj_s
{	Pfobj_t*	next;	/* next in linked list	*/
	int		line;	/* line #, 0 for name holder	*/
	union
	{
	Pfdata_t	data;
	char		f[1];	/* actual file name		*/
	} data;
};

static Pfobj_t**	Pftable;	/* hash table		*/
#define PFTABLE		1019		/* table size		*/
static Vmalloc_t*	Vmpf;		/* heap for our own use	*/

static Pfobj_t* pfsearch(Vmalloc_t*	vm,	/* region allocating from			*/
			 char*		file,	/* the file issuing the allocation request	*/
			 int		line)	/* line number					*/
{
	Pfobj_t	*pf, *last;
	Vmulong_t	h;
	int		n;
	char	*cp;

	if(!Vmpf && !(Vmpf = vmopen(Vmdcheap,Vmpool,0)) )
		return NULL;

	/* make hash table; PFTABLE'th slot hold regions' records */
	if(!Pftable)
	{	if(!(Pftable = (Pfobj_t**)vmalloc(Vmheap,(PFTABLE+1)*sizeof(Pfobj_t*))) )
			return NULL;
		for(n = PFTABLE; n >= 0; --n)
			Pftable[n] = NULL;
	}

	/* see if it's there with a combined hash value of vm,file,line */
	h = line + (((Vmulong_t)vm)>>4);
	for(cp = file; *cp; ++cp)
		h += (h<<7) + ((*cp)&0377) + 987654321L;
	n = (int)(h%PFTABLE);
	for(last = NULL, pf = Pftable[n]; pf; last = pf, pf = pf->next)
		if(PFLINE(pf) == line && PFVM(pf) == vm && strcmp(PFFILE(pf),file) == 0)
			break;

	/* insert if not there yet */
	if(!pf)
	{	Pfobj_t*	fn;
		Pfobj_t*	pfvm;
		Vmulong_t	hn;

		/* first get/construct the file name slot */
		hn = 0;
		for(cp = file; *cp; ++cp)
			hn += (hn<<7) + ((*cp)&0377) + 987654321L;
		n = (int)(hn%PFTABLE);
		for(fn = Pftable[n]; fn; fn = fn->next)
			if(PFLINE(fn) < 0 && strcmp(PFNAME(fn),file) == 0)
				break;
		if(!fn)
		{	size_t	s;
			s = sizeof(Pfobj_t) - sizeof(Pfdata_t) + strlen(file) + 1;
			if(!(fn = (Pfobj_t*)vmalloc(Vmheap,s)) )
				return NULL;
			fn->next = Pftable[n];
			Pftable[n] = fn;
			PFLINE(fn) = -1;
			strcpy(PFNAME(fn),file);
		}

		/* get region record; note that these are ordered by vm */
		last = NULL;
		for(pfvm = Pftable[PFTABLE]; pfvm; last = pfvm, pfvm = pfvm->next)
			if(vm >= PFVM(pfvm))
				break;
		if(!pfvm || PFVM(pfvm) > vm)
		{	if(!(pfvm = (Pfobj_t*)vmalloc(Vmpf,sizeof(Pfobj_t))) )
				return NULL;
			if(last)
			{	pfvm->next = last->next;
				last->next = pfvm;
			}
			else
			{	pfvm->next = Pftable[PFTABLE];
				Pftable[PFTABLE] = pfvm;
			}
			PFNALLOC(pfvm) = PFALLOC(pfvm) = 0;
			PFNFREE(pfvm) = PFFREE(pfvm) = 0;
			PFMAX(pfvm) = 0;
			PFVM(pfvm) = vm;
			PFLINE(pfvm) = 0;
		}

		if(!(pf = (Pfobj_t*)vmalloc(Vmpf,sizeof(Pfobj_t))) )
			return NULL;
		n = (int)(h%PFTABLE);
		pf->next = Pftable[n];
		Pftable[n] = pf;
		PFLINE(pf) = line;
		PFFILE(pf) = PFNAME(fn);
		PFREGION(pf) = pfvm;
		PFVM(pf) = vm;
		PFNALLOC(pf) = 0;
		PFALLOC(pf) = 0;
		PFNFREE(pf) = 0;
		PFFREE(pf) = 0;
		PFHASH(pf) = h;
	}
	else if(last)	/* do a move-to-front */
	{	last->next = pf->next;
		pf->next = Pftable[n];
		Pftable[n] = pf;
	}

	return pf;
}

static void pfclose(Vmalloc_t* vm)
{
	int		n;
	Pfobj_t	*pf, *next, *last;

	/* free all records related to region vm */
	for(n = PFTABLE; n >= 0; --n)
	{	for(last = NULL, pf = Pftable[n]; pf; )
		{	next = pf->next;

			if(PFLINE(pf) >= 0 && PFVM(pf) == vm)
			{	if(last)
					last->next = next;
				else	Pftable[n] = next;
				vmfree(Vmpf,pf);
			}
			else	last = pf;

			pf = next;
		}
	}
}

static void pfsetinfo(Vmalloc_t* vm, Vmuchar_t* data, size_t size, char* file, int line)
{
	Pfobj_t*	pf;
	Vmulong_t	s;

	/* let vmclose knows that there are records for region vm */
	_Vmpfclose = pfclose;

	if(!file || line <= 0)
	{	file = "";
		line = 0;
	}

	if((pf = pfsearch(vm,file,line)) )
	{	PFALLOC(pf) += (Vmulong_t)size;
		PFNALLOC(pf) += 1;
	}
	PFOBJ(data) = pf;
	PFSIZE(data) = size;

	if(pf)
	{	/* update region statistics */
		pf = PFREGION(pf);
		PFALLOC(pf) += (Vmulong_t)size;
		PFNALLOC(pf) += 1;
		if((s = PFALLOC(pf) - PFFREE(pf)) > PFMAX(pf) )
			PFMAX(pf) = s;
	}
}

/* sort by file names and line numbers */
static Pfobj_t* pfsort(Pfobj_t* pf)
{
	Pfobj_t	*one, *two, *next;
	int		cmp;

	if(!pf->next)
		return pf;

	/* partition to two equal size lists */
	one = two = NULL;
	while(pf)
	{	next = pf->next;
		pf->next = one;
		one = pf;

		if((pf = next) )
		{	next = pf->next;
			pf->next = two;
			two = pf;
			pf = next;
		}
	}

	/* sort and merge the lists */
	one = pfsort(one);
	two = pfsort(two);
	for(pf = next = NULL;; )
	{	/* make sure that the "<>" file comes first */	
		if(PFLINE(one) == 0 && PFLINE(two) == 0)
			cmp = PFVM(one) > PFVM(two) ? 1 : -1;
		else if(PFLINE(one) == 0)
			cmp = -1;
		else if(PFLINE(two) == 0)
			cmp = 1;
		else if((cmp = strcmp(PFFILE(one),PFFILE(two))) == 0)
		{	cmp = PFLINE(one) - PFLINE(two);
			if(cmp == 0)
				cmp = PFVM(one) > PFVM(two) ? 1 : -1;
		}

		if(cmp < 0)
		{	if(!pf)
				pf = one;
			else	next->next = one;
			next = one;
			if(!(one = one->next) )
			{	if(two)
					next->next = two;
				return pf;
			}
		}
		else
		{	if(!pf)
				pf = two;
			else	next->next = two;
			next = two;
			if(!(two = two->next) )
			{	if(one)
					next->next = one;
				return pf;
			}
		}
	}
}

static char* pfsummary(char* buf, Vmulong_t na, Vmulong_t sa,
			Vmulong_t nf, Vmulong_t sf, Vmulong_t max, Vmulong_t size)
{
	buf = (*_Vmstrcpy)(buf,"n_alloc", '=');
	buf = (*_Vmstrcpy)(buf, (*_Vmitoa)(na,-1), ':');
	buf = (*_Vmstrcpy)(buf,"n_free", '=');
	buf = (*_Vmstrcpy)(buf, (*_Vmitoa)(nf,-1), ':');
	buf = (*_Vmstrcpy)(buf,"s_alloc", '=');
	buf = (*_Vmstrcpy)(buf, (*_Vmitoa)(sa,-1), ':');
	buf = (*_Vmstrcpy)(buf,"s_free", '=');
	buf = (*_Vmstrcpy)(buf, (*_Vmitoa)(sf,-1), ':');
	if(max > 0)
	{	buf = (*_Vmstrcpy)(buf,"max_busy", '=');
		buf = (*_Vmstrcpy)(buf, (*_Vmitoa)(max,-1), ':');
		buf = (*_Vmstrcpy)(buf,"extent", '=');
		buf = (*_Vmstrcpy)(buf, (*_Vmitoa)(size,-1), ':');
	}
	*buf++ = '\n';

	return buf;
}

/* print profile data */
int vmprofile(Vmalloc_t* vm, int fd)
{
	Pfobj_t	*pf, *list, *next, *last;
	int		n;
	Vmulong_t	nalloc, alloc, nfree, free;
	Seg_t	*seg;
	char		buf[1024], *bufp, *endbuf;
#define INITBUF()	(bufp = buf, endbuf = buf+sizeof(buf)-128)
#define CHKBUF()	(bufp >= endbuf ? (write(fd,buf,bufp-buf), bufp=buf) : bufp)
#define FLSBUF()	(bufp > buf ? write(fd,buf,bufp-buf) : 0)

	if(fd < 0)
		return -1;

	/* initialize functions from vmtrace.c that we use below */
	if((n = vmtrace(-1)) >= 0)
		vmtrace(n);

	alloc = free = nalloc = nfree = 0;
	list = NULL;
	for(n = PFTABLE-1; n >= 0; --n)
	{	for(pf = Pftable[n], last = NULL; pf; )
		{	next = pf->next;

			if(PFLINE(pf) < 0  || (vm && vm != PFVM(pf)) )
			{	last = pf;
				goto next_pf;
			}

			/* remove from hash table */
			if(last)
				last->next = next;
			else	Pftable[n] = next;

			/* put on output list */
			pf->next = list;
			list = pf;
			nalloc += PFNALLOC(pf);
			alloc += PFALLOC(pf);
			nfree += PFNFREE(pf);
			free += PFFREE(pf);

		next_pf:
			pf = next;
		}
	}

	INITBUF();
	bufp = (*_Vmstrcpy)(bufp,"ALLOCATION USAGE SUMMARY", ':');
	bufp = pfsummary(bufp,nalloc,alloc,nfree,free,0,0);

	/* print regions' summary data */
	for(pf = Pftable[PFTABLE]; pf; pf = pf->next)
	{	if(vm && PFVM(pf) != vm)
			continue;
		alloc = 0;
		for(seg = PFVM(pf)->data->seg; seg; seg = seg->next)
			alloc += (Vmulong_t)seg->extent;
		bufp = (*_Vmstrcpy)(bufp,"region", '=');
		bufp = (*_Vmstrcpy)(bufp, (*_Vmitoa)(VLONG(PFVM(pf)),0), ':');
		bufp = pfsummary(bufp,PFNALLOC(pf),PFALLOC(pf),
				 PFNFREE(pf),PFFREE(pf),PFMAX(pf),alloc);
	}

	/* sort then output detailed profile */
	list = pfsort(list);
	for(pf = list; pf; )
	{	/* compute summary for file */
		alloc = free = nalloc = nfree = 0;
		for(last = pf; last; last = last->next)
		{	if(strcmp(PFFILE(last),PFFILE(pf)) != 0)
				break;
			nalloc += PFNALLOC(pf);
			alloc += PFALLOC(last);
			nfree += PFNFREE(last);
			free += PFFREE(last);
		}
		CHKBUF();
		bufp = (*_Vmstrcpy)(bufp,"file",'=');
		bufp = (*_Vmstrcpy)(bufp,PFFILE(pf)[0] ? PFFILE(pf) : "<>" ,':');
		bufp = pfsummary(bufp,nalloc,alloc,nfree,free,0,0);

		while(pf != last)	/* detailed data */
		{	CHKBUF();
			bufp = (*_Vmstrcpy)(bufp,"\tline",'=');
			bufp = (*_Vmstrcpy)(bufp, (*_Vmitoa)(PFLINE(pf),-1), ':');
			bufp = (*_Vmstrcpy)(bufp, "region", '=');
			bufp = (*_Vmstrcpy)(bufp, (*_Vmitoa)(VLONG(PFVM(pf)),0), ':');
			bufp = pfsummary(bufp,PFNALLOC(pf),PFALLOC(pf),
					 PFNFREE(pf),PFFREE(pf),0,0);

			/* reinsert into hash table */
			next = pf->next;
			n = (int)(PFHASH(pf)%PFTABLE);
			pf->next = Pftable[n];
			Pftable[n] = pf;
			pf = next;
		}
	}

	FLSBUF();
	return 0;
}

static void* pfalloc(Vmalloc_t* vm, size_t size, int local)
{
	size_t	s;
	void	*data;
	char	*file;
	int		line;
	void	*func;
	Vmdata_t	*vd = vm->data;

	VMFLF(vm,file,line,func);

	SETLOCK(vm, local);

	s = ROUND(size,ALIGN) + PF_EXTRA;
	if((data = KPVALLOC(vm,s,(*(Vmbest->allocf))) ) )
	{	pfsetinfo(vm,(Vmuchar_t*)data,size,file,line);

		if(!local && (vd->mode&VM_TRACE) && _Vmtrace)
		{	vm->file = file; vm->line = line; vm->func = func;
			(*_Vmtrace)(vm,NULL,(Vmuchar_t*)data,size,0);
		}
	}

	CLRLOCK(vm, local);

	return data;
}

static int pffree(Vmalloc_t* vm, void* data, int local)
{
	Pfobj_t	*pf;
	size_t	s;
	char	*file;
	int		line, rv;
	void	*func;
	Vmdata_t	*vd = vm->data;

	VMFLF(vm,file,line,func);

	if(!data)
		return 0;

	SETLOCK(vm,local);

	/**/ASSERT(KPVADDR(vm, data, Vmbest->addrf) == 0 );
	pf = PFOBJ(data);
	s = PFSIZE(data);
	if(pf)
	{	PFNFREE(pf) += 1;
		PFFREE(pf) += (Vmulong_t)s;
		pf = PFREGION(pf);
		PFNFREE(pf) += 1;
		PFFREE(pf) += (Vmulong_t)s;
	}

	if(!local && (vd->mode&VM_TRACE) && _Vmtrace)
	{	vm->file = file; vm->line = line; vm->func = func;
		(*_Vmtrace)(vm,(Vmuchar_t*)data,NULL,s,0);
	}

	rv = KPVFREE((vm), data, (*Vmbest->freef));

        CLRLOCK(vm, local);

	return rv;
}

static void* pfresize(Vmalloc_t* vm, void* data, size_t size, int type, int local)
{
	Pfobj_t	*pf;
	size_t	s, news;
	void	*addr;
	char	*file;
	int		line;
	void	*func;
	size_t	oldsize;
	Vmdata_t	*vd = vm->data;

	if(!data)
	{	addr = pfalloc(vm, size, local);
		if(addr && (type&VM_RSZERO) )
			memset(addr, 0, size);
		return addr;
	}
	if(size == 0)
	{	(void)pffree(vm, data, local);
		return NULL;
	}

	VMFLF(vm,file,line,func);

	SETLOCK(vm, local);

	/**/ASSERT(KPVADDR(vm,data,Vmbest->addrf) == 0 );
	pf = PFOBJ(data);
	s = oldsize = PFSIZE(data);

	news = ROUND(size,ALIGN) + PF_EXTRA;
	if((addr = KPVRESIZE(vm,data,news,(type&~VM_RSZERO),Vmbest->resizef)) )
	{	if(pf)
		{	PFFREE(pf) += (Vmulong_t)s;
			PFNFREE(pf) += 1;
			pf = PFREGION(pf);
			PFFREE(pf) += (Vmulong_t)s;
			PFNFREE(pf) += 1;
			pfsetinfo(vm,(Vmuchar_t*)addr,size,file,line);
		}

		if(!local && (vd->mode&VM_TRACE) && _Vmtrace)
		{	vm->file = file; vm->line = line; vm->func = func;
			(*_Vmtrace)(vm,(Vmuchar_t*)data,(Vmuchar_t*)addr,size,0);
		}
	}
	else if(pf)	/* reset old info */
	{	PFALLOC(pf) -= (Vmulong_t)s;
		PFNALLOC(pf) -= 1;
		pf = PFREGION(pf);
		PFALLOC(pf) -= (Vmulong_t)s;
		PFNALLOC(pf) -= 1;
		file = PFFILE(pf);
		line = PFLINE(pf);
		pfsetinfo(vm,(Vmuchar_t*)data,s,file,line);
	}

	if(addr && (type&VM_RSZERO) && oldsize < size)
	{	Vmuchar_t *d = (Vmuchar_t*)addr+oldsize, *ed = (Vmuchar_t*)addr+size;
		do { *d++ = 0; } while(d < ed);
	}

	CLRLOCK(vm, local);

	return addr;
}

static long pfsize(Vmalloc_t* vm, void* addr, int local)
{
	return (*Vmbest->addrf)(vm, addr, local) != 0 ? -1L : (long)PFSIZE(addr);
}

static long pfaddr(Vmalloc_t* vm, void* addr, int local)
{
	return (*Vmbest->addrf)(vm, addr, local);
}

static int pfcompact(Vmalloc_t* vm, int local)
{
	return (*Vmbest->compactf)(vm, local);
}

static void* pfalign(Vmalloc_t* vm, size_t size, size_t align, int local)
{
	size_t	s;
	void	*data;
	char	*file;
	int		line;
	void	*func;
	Vmdata_t	*vd = vm->data;

	VMFLF(vm,file,line,func);

	SETLOCK(vm, local);

	s = (size <= TINYSIZE ? TINYSIZE : ROUND(size,ALIGN)) + PF_EXTRA;
	if((data = KPVALIGN(vm,s,align,Vmbest->alignf)) )
	{	pfsetinfo(vm,(Vmuchar_t*)data,size,file,line);

		if(!local && (vd->mode&VM_TRACE) && _Vmtrace)
		{	vm->file = file; vm->line = line; vm->func = func;
			(*_Vmtrace)(vm,NULL,(Vmuchar_t*)data,size,align);
		}
	}

	CLRLOCK(vm, local);

	return data;
}

static Vmethod_t _Vmprofile =
{
	pfalloc,
	pfresize,
	pffree,
	pfaddr,
	pfsize,
	pfcompact,
	pfalign,
	VM_MTPROFILE
};

Vmethod_t*	Vmprofile = &_Vmprofile;

#ifdef NoF
NoF(vmprofile)
#endif
