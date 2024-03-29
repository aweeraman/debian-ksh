/***********************************************************************
*                                                                      *
*               This software is part of the ast package               *
*          Copyright (c) 1997-2012 AT&T Intellectual Property          *
*          Copyright (c) 2020-2023 Contributors to ksh 93u+m           *
*                      and is licensed under the                       *
*                 Eclipse Public License, Version 2.0                  *
*                                                                      *
*                A copy of the License is available at                 *
*      https://www.eclipse.org/org/documents/epl-2.0/EPL-2.0.html      *
*         (with md5 checksum 84283fa8859daf213bdda5a9f8d1be1d)         *
*                                                                      *
*                 Glenn Fowler <gsf@research.att.com>                  *
*                  Martijn Dekker <martijn@inlv.org>                   *
*                                                                      *
***********************************************************************/
/*
 * Glenn Fowler
 * AT&T Research
 */

#include "dlllib.h"

/*
 * return plugin version for dll
 * 0 if there is none
 * path!=0 enables library level diagnostics
 */

extern unsigned long
dllversion(void* dll, const char* path)
{
	Dll_plugin_version_f	pvf;

	if (pvf = (Dll_plugin_version_f)dlllook(dll, "plugin_version"))
		return (*pvf)();
	if (path)
	{
		state.error = 1;
		sfsprintf(state.errorbuf, sizeof(state.errorbuf), "plugin_version() not found");
		errorf("dll", NULL, 1, "dllversion: %s: %s", path, state.errorbuf);
	}
	return 0;
}

/*
 * check if dll on path has plugin version >= ver
 * 1 returned on success, 0 on failure
 * path!=0 enables library level diagnostics
 * cur!=0 gets actual version
 */

extern int
dllcheck(void* dll, const char* path, unsigned long ver, unsigned long* cur)
{
	unsigned long		v;

	state.error = 0;
	if (ver || cur)
	{
		v = dllversion(dll, path);
		if (cur)
			*cur = v;
	}
	if (!ver)
		return 1;
	if (!v)
		return 0;
	if (v < ver)
	{
		if (path)
		{
			state.error = 1;
			sfsprintf(state.errorbuf, sizeof(state.errorbuf), "plugin version %lu older than caller %lu", v, ver);
			errorf("dll", NULL, 1, "dllcheck: %s: %s", path, state.errorbuf);
		}
		return 0;
	}
	errorf("dll", NULL, -1, "dllversion: %s: %lu >= %lu", path, v, ver);
	return 1;
}
