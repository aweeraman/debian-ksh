/***********************************************************************
 *                                                                      *
 *               This software is part of the ast package               *
 *          Copyright (c) 1985-2011 AT&T Intellectual Property          *
 *                      and is licensed under the                       *
 *                 Eclipse Public License, Version 1.0                  *
 *                    by AT&T Intellectual Property                     *
 *                                                                      *
 *                A copy of the License is available at                 *
 *          http://www.eclipse.org/org/documents/epl-v10.html           *
 *         (with md5 checksum b35adb5213ca9657e911e9befb180842)         *
 *                                                                      *
 *              Information and Software Systems Research               *
 *                            AT&T Research                             *
 *                           Florham Park NJ                            *
 *                                                                      *
 *               Glenn Fowler <glenn.s.fowler@gmail.com>                *
 *                    David Korn <dgkorn@gmail.com>                     *
 *                     Phong Vo <phongvo@gmail.com>                     *
 *                                                                      *
 ***********************************************************************/
/*
 * Glenn Fowler
 * AT&T Research
 *
 * -last clock_t,time_t interface definitions plus
 *
 *      <ast.h>
 *      <time.h>
 *      <sys/time.h>
 *      <sys/times.h>
 */
#ifndef _TIMES_H
#define _TIMES_H 1

#include "ast.h"

#undef _TIMES_H
#include <sys/time.h>
#include <sys/times.h>
#include <time.h>
#ifndef _TIMES_H
#define _TIMES_H
#endif

extern int touch(const char *, time_t, time_t, int);

#endif  // _TIMES_H
