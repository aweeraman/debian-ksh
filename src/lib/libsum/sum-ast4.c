/***********************************************************************
*                                                                      *
*               This software is part of the ast package               *
*          Copyright (c) 1996-2011 AT&T Intellectual Property          *
*          Copyright (c) 2020-2022 Contributors to ksh 93u+m           *
*                      and is licensed under the                       *
*                 Eclipse Public License, Version 2.0                  *
*                                                                      *
*                A copy of the License is available at                 *
*      https://www.eclipse.org/org/documents/epl-2.0/EPL-2.0.html      *
*         (with md5 checksum 84283fa8859daf213bdda5a9f8d1be1d)         *
*                                                                      *
*                 Glenn Fowler <gsf@research.att.com>                  *
*                  Martijn Dekker <martijn@inlv.org>                   *
*            Johnothan King <johnothanking@protonmail.com>             *
*                                                                      *
***********************************************************************/

/*
 * AST4
 */

#define ast4_description \
	"The \bAST\b 128 bit PRNG hash generated by catenating 4 separate 32 \
	bit PNRG hashes. The block count is not printed."
#define ast4_options	0
#define ast4_match	"ast4|32x4|tw"
#define ast4_done	long_done
#define ast4_scale	0

typedef struct Ast4_sum_s
{
	uint32_t	sum0;
	uint32_t	sum1;
	uint32_t	sum2;
	uint32_t	sum3;
} Ast4_sum_t;

typedef struct Ast4_s
{
	_SUM_PUBLIC_
	_SUM_PRIVATE_
	Ast4_sum_t	cur;
	Ast4_sum_t	tot;
	unsigned char	buf[sizeof(Ast4_sum_t)];
} Ast4_t;

static int
ast4_init(Sum_t* p)
{
	register Ast4_t*	a = (Ast4_t*)p;

	a->tot.sum0 ^= a->cur.sum0;
	a->cur.sum0 = 0;
	a->tot.sum1 ^= a->cur.sum1;
	a->cur.sum1 = 0;
	a->tot.sum2 ^= a->cur.sum2;
	a->cur.sum2 = 0;
	a->tot.sum3 ^= a->cur.sum3;
	a->cur.sum3 = 0;
	return 0;
}

static Sum_t*
ast4_open(const Method_t* method, const char* name)
{
	Ast4_t*	p;

	if (p = newof(0, Ast4_t, 1, 0))
	{
		p->method = (Method_t*)method;
		p->name = name;
	}
	return (Sum_t*)p;
}

static int
ast4_block(Sum_t* p, const void* s, size_t n)
{
	register Ast4_sum_t*	a = &((Ast4_t*)p)->cur;
	register unsigned char*	b = (unsigned char*)s;
	register unsigned char*	e = b + n;
	register int		c;

	while (b < e)
	{
		c = *b++;
		a->sum0 = a->sum0 * 0x63c63cd9 + 0x9c39c33d + c;
		a->sum1 = a->sum1 * 0x00000011 + 0x00017cfb + c;
		a->sum2 = a->sum2 * 0x12345679 + 0x3ade68b1 + c;
		a->sum3 = a->sum3 * 0xf1eac01d + 0xcafe10af + c;
	}
	return 0;
}

static int
ast4_print(Sum_t* p, Sfio_t* sp, int flags, size_t scale)
{
	register Ast4_sum_t*	a;

	a = (flags & SUM_TOTAL) ? &((Ast4_t*)p)->tot : &((Ast4_t*)p)->cur;
	sfprintf(sp, "%06..64u%06..64u%06..64u%06..64u", a->sum0, a->sum1, a->sum2, a->sum3);
	return 0;
}

static int
ast4_data(Sum_t* p, Sumdata_t* data)
{
	data->size = sizeof(((Ast4_t*)p)->cur);
	data->num = 0;
#if _ast_intswap
	swapmem(_ast_intswap, data->buf = ((Ast4_t*)p)->buf, &((Ast4_t*)p)->cur, sizeof(((Ast4_t*)p)->cur));
#else
	data->buf = &((Ast4_t*)p)->cur;
#endif
	return 0;
}
