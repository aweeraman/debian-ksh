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
#include	"sfhdr.h"

/*	Close a stream. A file stream is synced before closing.
**
**	Written by Kiem-Phong Vo
*/

int sfclose(Sfio_t* f)
{
	int		local, ex, rv;
	void*		data = NULL;

	if(!f)
		return -1;

	GETLOCAL(f,local);

	if(!(f->mode&SF_INIT) &&
	   SFMODE(f,local) != (f->mode&SF_RDWR) &&
	   SFMODE(f,local) != (f->mode&(SF_READ|SF_SYNCED)) &&
	   _sfmode(f,SF_SYNCED,local) < 0)
		return -1;

	/* closing a stack of streams */
	while(f->push)
	{	Sfio_t*	pop;

		if(!(pop = (*_Sfstack)(f,NULL)) )
			return -1;

		if(sfclose(pop) < 0)
		{	(*_Sfstack)(f,pop);
			return -1;
		}
	}

	rv = 0;
	if(f->disc == _Sfudisc)	/* closing the ungetc stream */
		f->disc = NULL;
	else if(f->file >= 0)	/* sync file pointer */
	{	f->bits |= SF_ENDING;
		rv = sfsync(f);
	}

	SFLOCK(f,0);

	/* raise discipline exceptions */
	if(f->disc && (ex = SFRAISE(f,local ? SF_NEW : SF_CLOSING,NULL)) != 0)
		return ex;

	if(!local && f->pool)
	{	/* remove from pool */
		if(f->pool == &_Sfpool)
		{	int	n;

			for(n = 0; n < _Sfpool.n_sf; ++n)
			{	if(_Sfpool.sf[n] != f)
					continue;
				/* found it */
				_Sfpool.n_sf -= 1;
				for(; n < _Sfpool.n_sf; ++n)
					_Sfpool.sf[n] = _Sfpool.sf[n+1];
				break;
			}
		}
		else
		{	f->mode &= ~SF_LOCK;	/**/ASSERT(_Sfpmove);
			if((*_Sfpmove)(f,-1) < 0)
			{	SFOPEN(f,0);
				return -1;
			}
			f->mode |= SF_LOCK;
		}
		f->pool = NULL;
	}

	if(f->data && (!local || (f->flags&SF_STRING) || (f->bits&SF_MMAP) ) )
	{	/* free buffer */
#ifdef MAP_TYPE
		if(f->bits&SF_MMAP)
			SFMUNMAP(f,f->data,f->endb-f->data);
		else
#endif
		if(f->flags&SF_MALLOC)
			data = f->data;

		f->data = NULL;
		f->size = -1;
	}

	/* zap the file descriptor */
	if(_Sfnotify)
		(*_Sfnotify)(f, SF_CLOSING, (void*)((long)f->file));
	if(f->file >= 0 && !(f->flags&SF_STRING))
	{	while(close(f->file) < 0 )
		{	if(errno == EINTR)
				errno = 0;
			else
			{	rv = -1;
				break;
			}
		}
	}
	f->file = -1;

	SFKILL(f);
	f->flags &= SF_STATIC;
	f->here = 0;
	f->extent = -1;
	f->endb = f->endr = f->endw = f->next = f->data;

	/* zap any associated auxiliary buffer */
	if(f->rsrv)
	{	free(f->rsrv);
		f->rsrv = NULL;
	}

	/* delete any associated sfpopen-data */
	if(f->proc)
		rv = _sfpclose(f);

	if(!local)
	{	if(f->disc && (ex = SFRAISE(f,SF_FINAL,NULL)) != 0 )
		{	rv = ex;
			goto done;
		}

		if(!(f->flags&SF_STATIC) )
			free(f);
		else
		{	f->disc = NULL;
			f->stdio = NULL;
			f->mode = SF_AVAIL;
		}
	}

done:
	if(data)
		free(data);
	return rv;
}
