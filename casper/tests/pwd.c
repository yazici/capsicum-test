/*-
 * Copyright (c) 2013 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed by Pawel Jakub Dawidek under sponsorship from
 * the FreeBSD Foundation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>

#include <sys/capsicum.h>

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <libcasper.h>

#include "cap_pwd/cap_pwd.h"
#include "local.h"
#include "test.h"

#define	UID_ROOT	0

#define SAVED_USER_COUNT 8
static int userids[SAVED_USER_COUNT];
static char *usernames[SAVED_USER_COUNT];

static void save_users()
{
	int ii;
	struct passwd *pwd;

	setpwent();
	for (ii = 0; ii < SAVED_USER_COUNT; ii++) {
		pwd = getpwent();
		userids[ii] = pwd->pw_uid;
		usernames[ii] = strdup(pwd->pw_name);
	}
	endpwent();
}
static void free_users()
{
	int ii;
	for (ii = 0; ii < SAVED_USER_COUNT; ii++) {
		free(usernames[ii]);
		usernames[ii] = NULL;
	}
}

#define	GETPWENT0	0x0001
#define	GETPWENT1	0x0002
#define	GETPWENT2	0x0004
#define	GETPWENT	(GETPWENT0 | GETPWENT1 | GETPWENT2)
#define	GETPWENT_R0	0x0008
#define	GETPWENT_R1	0x0010
#define	GETPWENT_R2	0x0020
#define	GETPWENT_R	(GETPWENT_R0 | GETPWENT_R1 | GETPWENT_R2)
#define	GETPWNAM	0x0040
#define	GETPWNAM_R	0x0080
#define	GETPWUID	0x0100
#define	GETPWUID_R	0x0200

static bool
passwd_compare(const struct passwd *pwd0, const struct passwd *pwd1)
{

	if (pwd0 == NULL && pwd1 == NULL)
		return (true);
	if (pwd0 == NULL || pwd1 == NULL)
		return (false);

	if (strcmp(pwd0->pw_name, pwd1->pw_name) != 0)
		return (false);

	if (pwd0->pw_passwd != NULL || pwd1->pw_passwd != NULL) {
		if (pwd0->pw_passwd == NULL || pwd1->pw_passwd == NULL)
			return (false);
		if (strcmp(pwd0->pw_passwd, pwd1->pw_passwd) != 0)
			return (false);
	}

	if (pwd0->pw_uid != pwd1->pw_uid)
		return (false);

	if (pwd0->pw_gid != pwd1->pw_gid)
		return (false);

#ifdef HAVE_PASSWD_PW_CHANGE
	if (pwd0->pw_change != pwd1->pw_change)
		return (false);
#endif

#ifdef HAVE_PASSWD_PW_CLASS
	if (pwd0->pw_class != NULL || pwd1->pw_class != NULL) {
		if (pwd0->pw_class == NULL || pwd1->pw_class == NULL)
			return (false);
		if (strcmp(pwd0->pw_class, pwd1->pw_class) != 0)
			return (false);
	}
#endif

	if (pwd0->pw_gecos != NULL || pwd1->pw_gecos != NULL) {
		if (pwd0->pw_gecos == NULL || pwd1->pw_gecos == NULL)
			return (false);
		if (strcmp(pwd0->pw_gecos, pwd1->pw_gecos) != 0)
			return (false);
	}

	if (pwd0->pw_dir != NULL || pwd1->pw_dir != NULL) {
		if (pwd0->pw_dir == NULL || pwd1->pw_dir == NULL)
			return (false);
		if (strcmp(pwd0->pw_dir, pwd1->pw_dir) != 0)
			return (false);
	}

	if (pwd0->pw_shell != NULL || pwd1->pw_shell != NULL) {
		if (pwd0->pw_shell == NULL || pwd1->pw_shell == NULL)
			return (false);
		if (strcmp(pwd0->pw_shell, pwd1->pw_shell) != 0)
			return (false);
	}

#ifdef HAVE_PASSWD_PW_EXPIRE
	if (pwd0->pw_expire != pwd1->pw_expire)
		return (false);
#endif

#ifdef HAVE_PASSWD_PW_FIELDS
	if (pwd0->pw_fields != pwd1->pw_fields)
		return (false);
#endif

	return (true);
}

static unsigned int
runtest_cmds(cap_channel_t *cappwd)
{
	char bufs[1024], bufc[1024];
	unsigned int result;
	struct passwd *pwds, *pwdc;
	struct passwd sts, stc;

	result = 0;

	setpwent();
	cap_setpwent(cappwd);

	pwds = getpwent();
	pwdc = cap_getpwent(cappwd);
	if (passwd_compare(pwds, pwdc)) {
		result |= GETPWENT0;
		pwds = getpwent();
		pwdc = cap_getpwent(cappwd);
		if (passwd_compare(pwds, pwdc))
			result |= GETPWENT1;
	}

	getpwent_r(&sts, bufs, sizeof(bufs), &pwds);
	cap_getpwent_r(cappwd, &stc, bufc, sizeof(bufc), &pwdc);
	if (passwd_compare(pwds, pwdc)) {
		result |= GETPWENT_R0;
		getpwent_r(&sts, bufs, sizeof(bufs), &pwds);
		cap_getpwent_r(cappwd, &stc, bufc, sizeof(bufc), &pwdc);
		if (passwd_compare(pwds, pwdc))
			result |= GETPWENT_R1;
	}

	setpwent();
	cap_setpwent(cappwd);

	getpwent_r(&sts, bufs, sizeof(bufs), &pwds);
	cap_getpwent_r(cappwd, &stc, bufc, sizeof(bufc), &pwdc);
	if (passwd_compare(pwds, pwdc))
		result |= GETPWENT_R2;

	pwds = getpwent();
	pwdc = cap_getpwent(cappwd);
	if (passwd_compare(pwds, pwdc))
		result |= GETPWENT2;

	pwds = getpwnam(usernames[0]);
	pwdc = cap_getpwnam(cappwd, usernames[0]);
	if (passwd_compare(pwds, pwdc)) {
		pwds = getpwnam(usernames[3]);
		pwdc = cap_getpwnam(cappwd, usernames[3]);
		if (passwd_compare(pwds, pwdc))
			result |= GETPWNAM;
	}

	getpwnam_r(usernames[0], &sts, bufs, sizeof(bufs), &pwds);
	cap_getpwnam_r(cappwd, usernames[0], &stc, bufc, sizeof(bufc), &pwdc);
	if (passwd_compare(pwds, pwdc)) {
		getpwnam_r(usernames[3], &sts, bufs, sizeof(bufs), &pwds);
		cap_getpwnam_r(cappwd, usernames[3], &stc, bufc, sizeof(bufc),
		    &pwdc);
		if (passwd_compare(pwds, pwdc))
			result |= GETPWNAM_R;
	}

	pwds = getpwuid(UID_ROOT);
	pwdc = cap_getpwuid(cappwd, UID_ROOT);
	if (passwd_compare(pwds, pwdc)) {
		pwds = getpwuid(userids[3]);
		pwdc = cap_getpwuid(cappwd, userids[3]);
		if (passwd_compare(pwds, pwdc))
			result |= GETPWUID;
	}

	getpwuid_r(UID_ROOT, &sts, bufs, sizeof(bufs), &pwds);
	cap_getpwuid_r(cappwd, UID_ROOT, &stc, bufc, sizeof(bufc), &pwdc);
	if (passwd_compare(pwds, pwdc)) {
		getpwuid_r(userids[3], &sts, bufs, sizeof(bufs), &pwds);
		cap_getpwuid_r(cappwd, userids[3], &stc, bufc, sizeof(bufc),
		    &pwdc);
		if (passwd_compare(pwds, pwdc))
			result |= GETPWUID_R;
	}

	return (result);
}

static void
test_cmds(cap_channel_t *origcappwd)
{
	cap_channel_t *cappwd;
	const char *cmds[7], *fields[10], *names[6];
	uid_t uids[5];

	fields[0] = "pw_name";
	fields[1] = "pw_passwd";
	fields[2] = "pw_uid";
	fields[3] = "pw_gid";
	fields[4] = "pw_change";
	fields[5] = "pw_class";
	fields[6] = "pw_gecos";
	fields[7] = "pw_dir";
	fields[8] = "pw_shell";
	fields[9] = "pw_expire";

	names[0] = usernames[0];
	names[1] = usernames[1];
	names[2] = usernames[2];
	names[3] = usernames[3];
	names[4] = usernames[4];
	names[5] = usernames[6];

	uids[0] = userids[0];
	uids[1] = userids[1];
	uids[2] = userids[2];
	uids[3] = userids[3];
	uids[4] = userids[5];

	/*
	 * Allow:
	 * cmds: setpwent, getpwent, getpwent_r, getpwnam, getpwnam_r,
	 *       getpwuid, getpwuid_r
	 * users:
	 *     names: entries 0,1,2,3,4,6
	 *     uids:
	 */
	cap_setpwent(origcappwd);
	cappwd = cap_clone(origcappwd);
	CHECK(cappwd != NULL);

	cmds[0] = "setpwent";
	cmds[1] = "getpwent";
	cmds[2] = "getpwent_r";
	cmds[3] = "getpwnam";
	cmds[4] = "getpwnam_r";
	cmds[5] = "getpwuid";
	cmds[6] = "getpwuid_r";
	CHECK(cap_pwd_limit_cmds(cappwd, cmds, 7) == 0);
	CHECK(cap_pwd_limit_fields(cappwd, fields, 10) == 0);
	CHECK(cap_pwd_limit_users(cappwd, names, 6, NULL, 0) == 0);

	CHECK(runtest_cmds(cappwd) == (GETPWENT | GETPWENT_R |
	    GETPWNAM | GETPWNAM_R | GETPWUID | GETPWUID_R));

	cap_close(cappwd);

	/*
	 * Allow:
	 * cmds: setpwent, getpwent, getpwent_r, getpwnam, getpwnam_r,
	 *       getpwuid, getpwuid_r
	 * users:
	 *     names:
	 *     uids: entries 0, 1, 2, 3, 5
	 */
	cap_setpwent(origcappwd);
	cappwd = cap_clone(origcappwd);
	CHECK(cappwd != NULL);

	cmds[0] = "setpwent";
	cmds[1] = "getpwent";
	cmds[2] = "getpwent_r";
	cmds[3] = "getpwnam";
	cmds[4] = "getpwnam_r";
	cmds[5] = "getpwuid";
	cmds[6] = "getpwuid_r";
	CHECK(cap_pwd_limit_cmds(cappwd, cmds, 7) == 0);
	CHECK(cap_pwd_limit_fields(cappwd, fields, 10) == 0);
	CHECK(cap_pwd_limit_users(cappwd, NULL, 0, uids, 5) == 0);

	CHECK(runtest_cmds(cappwd) == (GETPWENT | GETPWENT_R |
	    GETPWNAM | GETPWNAM_R | GETPWUID | GETPWUID_R));

	cap_close(cappwd);

	/*
	 * Allow:
	 * cmds: getpwent, getpwent_r, getpwnam, getpwnam_r,
	 *       getpwuid, getpwuid_r
	 * users:
	 *     names: entries 0,1,2,3,4,6
	 *     uids:
	 * Disallow:
	 * cmds: setpwent
	 * users:
	 */
	cap_setpwent(origcappwd);
	cappwd = cap_clone(origcappwd);
	CHECK(cappwd != NULL);

	cap_setpwent(cappwd);

	cmds[0] = "getpwent";
	cmds[1] = "getpwent_r";
	cmds[2] = "getpwnam";
	cmds[3] = "getpwnam_r";
	cmds[4] = "getpwuid";
	cmds[5] = "getpwuid_r";
	CHECK(cap_pwd_limit_cmds(cappwd, cmds, 6) == 0);
	cmds[0] = "setpwent";
	cmds[1] = "getpwent";
	cmds[2] = "getpwent_r";
	cmds[3] = "getpwnam";
	cmds[4] = "getpwnam_r";
	cmds[5] = "getpwuid";
	cmds[6] = "getpwuid_r";
	CHECK(cap_pwd_limit_cmds(cappwd, cmds, 7) == -1 && errno == ENOTCAPABLE);
	cmds[0] = "setpwent";
	CHECK(cap_pwd_limit_cmds(cappwd, cmds, 1) == -1 && errno == ENOTCAPABLE);
	CHECK(cap_pwd_limit_fields(cappwd, fields, 10) == 0);
	CHECK(cap_pwd_limit_users(cappwd, names, 6, NULL, 0) == 0);

	CHECK(runtest_cmds(cappwd) == (GETPWENT0 | GETPWENT1 | GETPWENT_R0 |
	    GETPWENT_R1 | GETPWNAM | GETPWNAM_R | GETPWUID | GETPWUID_R));

	cap_close(cappwd);

	/*
	 * Allow:
	 * cmds: getpwent, getpwent_r, getpwnam, getpwnam_r,
	 *       getpwuid, getpwuid_r
	 * users:
	 *     names:
	 *     uids: entries 0, 1, 2, 3, 5
	 * Disallow:
	 * cmds: setpwent
	 * users:
	 */
	cap_setpwent(origcappwd);
	cappwd = cap_clone(origcappwd);
	CHECK(cappwd != NULL);

	cap_setpwent(cappwd);

	cmds[0] = "getpwent";
	cmds[1] = "getpwent_r";
	cmds[2] = "getpwnam";
	cmds[3] = "getpwnam_r";
	cmds[4] = "getpwuid";
	cmds[5] = "getpwuid_r";
	CHECK(cap_pwd_limit_cmds(cappwd, cmds, 6) == 0);
	cmds[0] = "setpwent";
	cmds[1] = "getpwent";
	cmds[2] = "getpwent_r";
	cmds[3] = "getpwnam";
	cmds[4] = "getpwnam_r";
	cmds[5] = "getpwuid";
	cmds[6] = "getpwuid_r";
	CHECK(cap_pwd_limit_cmds(cappwd, cmds, 7) == -1 && errno == ENOTCAPABLE);
	cmds[0] = "setpwent";
	CHECK(cap_pwd_limit_cmds(cappwd, cmds, 1) == -1 && errno == ENOTCAPABLE);
	CHECK(cap_pwd_limit_fields(cappwd, fields, 10) == 0);
	CHECK(cap_pwd_limit_users(cappwd, NULL, 0, uids, 5) == 0);

	CHECK(runtest_cmds(cappwd) == (GETPWENT0 | GETPWENT1 | GETPWENT_R0 |
	    GETPWENT_R1 | GETPWNAM | GETPWNAM_R | GETPWUID | GETPWUID_R));

	cap_close(cappwd);

	/*
	 * Allow:
	 * cmds: setpwent, getpwent_r, getpwnam, getpwnam_r,
	 *       getpwuid, getpwuid_r
	 * users:
	 *     names: entries 0,1,2,3,4,6
	 *     uids:
	 * Disallow:
	 * cmds: getpwent
	 * users:
	 */
	cap_setpwent(origcappwd);
	cappwd = cap_clone(origcappwd);
	CHECK(cappwd != NULL);

	cmds[0] = "setpwent";
	cmds[1] = "getpwent_r";
	cmds[2] = "getpwnam";
	cmds[3] = "getpwnam_r";
	cmds[4] = "getpwuid";
	cmds[5] = "getpwuid_r";
	CHECK(cap_pwd_limit_cmds(cappwd, cmds, 6) == 0);
	cmds[0] = "setpwent";
	cmds[1] = "getpwent";
	cmds[2] = "getpwent_r";
	cmds[3] = "getpwnam";
	cmds[4] = "getpwnam_r";
	cmds[5] = "getpwuid";
	cmds[6] = "getpwuid_r";
	CHECK(cap_pwd_limit_cmds(cappwd, cmds, 7) == -1 && errno == ENOTCAPABLE);
	cmds[0] = "getpwent";
	CHECK(cap_pwd_limit_cmds(cappwd, cmds, 1) == -1 && errno == ENOTCAPABLE);
	CHECK(cap_pwd_limit_fields(cappwd, fields, 10) == 0);
	CHECK(cap_pwd_limit_users(cappwd, names, 6, NULL, 0) == 0);

	CHECK(runtest_cmds(cappwd) == (GETPWENT_R2 |
	    GETPWNAM | GETPWNAM_R | GETPWUID | GETPWUID_R));

	cap_close(cappwd);

	/*
	 * Allow:
	 * cmds: setpwent, getpwent_r, getpwnam, getpwnam_r,
	 *       getpwuid, getpwuid_r
	 * users:
	 *     names:
	 *     uids: entries 0, 1, 2, 3, 5
	 * Disallow:
	 * cmds: getpwent
	 * users:
	 */
	cap_setpwent(origcappwd);
	cappwd = cap_clone(origcappwd);
	CHECK(cappwd != NULL);

	cmds[0] = "setpwent";
	cmds[1] = "getpwent_r";
	cmds[2] = "getpwnam";
	cmds[3] = "getpwnam_r";
	cmds[4] = "getpwuid";
	cmds[5] = "getpwuid_r";
	CHECK(cap_pwd_limit_cmds(cappwd, cmds, 6) == 0);
	cmds[0] = "setpwent";
	cmds[1] = "getpwent";
	cmds[2] = "getpwent_r";
	cmds[3] = "getpwnam";
	cmds[4] = "getpwnam_r";
	cmds[5] = "getpwuid";
	cmds[6] = "getpwuid_r";
	CHECK(cap_pwd_limit_cmds(cappwd, cmds, 7) == -1 && errno == ENOTCAPABLE);
	cmds[0] = "getpwent";
	CHECK(cap_pwd_limit_cmds(cappwd, cmds, 1) == -1 && errno == ENOTCAPABLE);
	CHECK(cap_pwd_limit_fields(cappwd, fields, 10) == 0);
	CHECK(cap_pwd_limit_users(cappwd, NULL, 0, uids, 5) == 0);

	CHECK(runtest_cmds(cappwd) == (GETPWENT_R2 |
	    GETPWNAM | GETPWNAM_R | GETPWUID | GETPWUID_R));

	cap_close(cappwd);

	/*
	 * Allow:
	 * cmds: setpwent, getpwent, getpwnam, getpwnam_r,
	 *       getpwuid, getpwuid_r
	 * users:
	 *     names: entries 0,1,2,3,4,6
	 *     uids:
	 * Disallow:
	 * cmds: getpwent_r
	 * users:
	 */
	cap_setpwent(origcappwd);
	cappwd = cap_clone(origcappwd);
	CHECK(cappwd != NULL);

	cmds[0] = "setpwent";
	cmds[1] = "getpwent";
	cmds[2] = "getpwnam";
	cmds[3] = "getpwnam_r";
	cmds[4] = "getpwuid";
	cmds[5] = "getpwuid_r";
	CHECK(cap_pwd_limit_cmds(cappwd, cmds, 6) == 0);
	cmds[0] = "setpwent";
	cmds[1] = "getpwent";
	cmds[2] = "getpwent_r";
	cmds[3] = "getpwnam";
	cmds[4] = "getpwnam_r";
	cmds[5] = "getpwuid";
	cmds[6] = "getpwuid_r";
	CHECK(cap_pwd_limit_cmds(cappwd, cmds, 7) == -1 && errno == ENOTCAPABLE);
	cmds[0] = "getpwent_r";
	CHECK(cap_pwd_limit_cmds(cappwd, cmds, 1) == -1 && errno == ENOTCAPABLE);
	CHECK(cap_pwd_limit_fields(cappwd, fields, 10) == 0);
	CHECK(cap_pwd_limit_users(cappwd, names, 6, NULL, 0) == 0);

	CHECK(runtest_cmds(cappwd) == (GETPWENT0 | GETPWENT1 |
	    GETPWNAM | GETPWNAM_R | GETPWUID | GETPWUID_R));

	cap_close(cappwd);

	/*
	 * Allow:
	 * cmds: setpwent, getpwent, getpwnam, getpwnam_r,
	 *       getpwuid, getpwuid_r
	 * users:
	 *     names:
	 *     uids: entries 0, 1, 2, 3, 5
	 * Disallow:
	 * cmds: getpwent_r
	 * users:
	 */
	cap_setpwent(origcappwd);
	cappwd = cap_clone(origcappwd);
	CHECK(cappwd != NULL);

	cmds[0] = "setpwent";
	cmds[1] = "getpwent";
	cmds[2] = "getpwnam";
	cmds[3] = "getpwnam_r";
	cmds[4] = "getpwuid";
	cmds[5] = "getpwuid_r";
	CHECK(cap_pwd_limit_cmds(cappwd, cmds, 6) == 0);
	cmds[0] = "setpwent";
	cmds[1] = "getpwent";
	cmds[2] = "getpwent_r";
	cmds[3] = "getpwnam";
	cmds[4] = "getpwnam_r";
	cmds[5] = "getpwuid";
	cmds[6] = "getpwuid_r";
	CHECK(cap_pwd_limit_cmds(cappwd, cmds, 7) == -1 && errno == ENOTCAPABLE);
	cmds[0] = "getpwent_r";
	CHECK(cap_pwd_limit_cmds(cappwd, cmds, 1) == -1 && errno == ENOTCAPABLE);
	CHECK(cap_pwd_limit_fields(cappwd, fields, 10) == 0);
	CHECK(cap_pwd_limit_users(cappwd, NULL, 0, uids, 5) == 0);

	CHECK(runtest_cmds(cappwd) == (GETPWENT0 | GETPWENT1 |
	    GETPWNAM | GETPWNAM_R | GETPWUID | GETPWUID_R));

	cap_close(cappwd);

	/*
	 * Allow:
	 * cmds: setpwent, getpwent, getpwent_r, getpwnam_r,
	 *       getpwuid, getpwuid_r
	 * users:
	 *     names: entries 0,1,2,3,4,6
	 *     uids:
	 * Disallow:
	 * cmds: getpwnam
	 * users:
	 */
	cap_setpwent(origcappwd);
	cappwd = cap_clone(origcappwd);
	CHECK(cappwd != NULL);

	cmds[0] = "setpwent";
	cmds[1] = "getpwent";
	cmds[2] = "getpwent_r";
	cmds[3] = "getpwnam_r";
	cmds[4] = "getpwuid";
	cmds[5] = "getpwuid_r";
	CHECK(cap_pwd_limit_cmds(cappwd, cmds, 6) == 0);
	cmds[0] = "setpwent";
	cmds[1] = "getpwent";
	cmds[2] = "getpwent_r";
	cmds[3] = "getpwnam";
	cmds[4] = "getpwnam_r";
	cmds[5] = "getpwuid";
	cmds[6] = "getpwuid_r";
	CHECK(cap_pwd_limit_cmds(cappwd, cmds, 7) == -1 && errno == ENOTCAPABLE);
	cmds[0] = "getpwnam";
	CHECK(cap_pwd_limit_cmds(cappwd, cmds, 1) == -1 && errno == ENOTCAPABLE);
	CHECK(cap_pwd_limit_fields(cappwd, fields, 10) == 0);
	CHECK(cap_pwd_limit_users(cappwd, names, 6, NULL, 0) == 0);

	CHECK(runtest_cmds(cappwd) == (GETPWENT | GETPWENT_R |
	    GETPWNAM_R | GETPWUID | GETPWUID_R));

	cap_close(cappwd);

	/*
	 * Allow:
	 * cmds: setpwent, getpwent, getpwent_r, getpwnam_r,
	 *       getpwuid, getpwuid_r
	 * users:
	 *     names:
	 *     uids: entries 0, 1, 2, 3, 5
	 * Disallow:
	 * cmds: getpwnam
	 * users:
	 */
	cap_setpwent(origcappwd);
	cappwd = cap_clone(origcappwd);
	CHECK(cappwd != NULL);

	cmds[0] = "setpwent";
	cmds[1] = "getpwent";
	cmds[2] = "getpwent_r";
	cmds[3] = "getpwnam_r";
	cmds[4] = "getpwuid";
	cmds[5] = "getpwuid_r";
	CHECK(cap_pwd_limit_cmds(cappwd, cmds, 6) == 0);
	cmds[0] = "setpwent";
	cmds[1] = "getpwent";
	cmds[2] = "getpwent_r";
	cmds[3] = "getpwnam";
	cmds[4] = "getpwnam_r";
	cmds[5] = "getpwuid";
	cmds[6] = "getpwuid_r";
	CHECK(cap_pwd_limit_cmds(cappwd, cmds, 7) == -1 && errno == ENOTCAPABLE);
	cmds[0] = "getpwnam";
	CHECK(cap_pwd_limit_cmds(cappwd, cmds, 1) == -1 && errno == ENOTCAPABLE);
	CHECK(cap_pwd_limit_fields(cappwd, fields, 10) == 0);
	CHECK(cap_pwd_limit_users(cappwd, NULL, 0, uids, 5) == 0);

	CHECK(runtest_cmds(cappwd) == (GETPWENT | GETPWENT_R |
	    GETPWNAM_R | GETPWUID | GETPWUID_R));

	cap_close(cappwd);

	/*
	 * Allow:
	 * cmds: setpwent, getpwent, getpwent_r, getpwnam,
	 *       getpwuid, getpwuid_r
	 * users:
	 *     names: entries 0,1,2,3,4,6
	 *     uids:
	 * Disallow:
	 * cmds: getpwnam_r
	 * users:
	 */
	cap_setpwent(origcappwd);
	cappwd = cap_clone(origcappwd);
	CHECK(cappwd != NULL);

	cmds[0] = "setpwent";
	cmds[1] = "getpwent";
	cmds[2] = "getpwent_r";
	cmds[3] = "getpwnam";
	cmds[4] = "getpwuid";
	cmds[5] = "getpwuid_r";
	CHECK(cap_pwd_limit_cmds(cappwd, cmds, 6) == 0);
	cmds[0] = "setpwent";
	cmds[1] = "getpwent";
	cmds[2] = "getpwent_r";
	cmds[3] = "getpwnam";
	cmds[4] = "getpwnam_r";
	cmds[5] = "getpwuid";
	cmds[6] = "getpwuid_r";
	CHECK(cap_pwd_limit_cmds(cappwd, cmds, 7) == -1 && errno == ENOTCAPABLE);
	cmds[0] = "getpwnam_r";
	CHECK(cap_pwd_limit_cmds(cappwd, cmds, 1) == -1 && errno == ENOTCAPABLE);
	CHECK(cap_pwd_limit_fields(cappwd, fields, 10) == 0);
	CHECK(cap_pwd_limit_users(cappwd, names, 6, NULL, 0) == 0);

	CHECK(runtest_cmds(cappwd) == (GETPWENT | GETPWENT_R |
	    GETPWNAM | GETPWUID | GETPWUID_R));

	cap_close(cappwd);

	/*
	 * Allow:
	 * cmds: setpwent, getpwent, getpwent_r, getpwnam,
	 *       getpwuid, getpwuid_r
	 * users:
	 *     names:
	 *     uids: entries 0, 1, 2, 3, 5
	 * Disallow:
	 * cmds: getpwnam_r
	 * users:
	 */
	cap_setpwent(origcappwd);
	cappwd = cap_clone(origcappwd);
	CHECK(cappwd != NULL);

	cmds[0] = "setpwent";
	cmds[1] = "getpwent";
	cmds[2] = "getpwent_r";
	cmds[3] = "getpwnam";
	cmds[4] = "getpwuid";
	cmds[5] = "getpwuid_r";
	CHECK(cap_pwd_limit_cmds(cappwd, cmds, 6) == 0);
	cmds[0] = "setpwent";
	cmds[1] = "getpwent";
	cmds[2] = "getpwent_r";
	cmds[3] = "getpwnam";
	cmds[4] = "getpwnam_r";
	cmds[5] = "getpwuid";
	cmds[6] = "getpwuid_r";
	CHECK(cap_pwd_limit_cmds(cappwd, cmds, 7) == -1 && errno == ENOTCAPABLE);
	cmds[0] = "getpwnam_r";
	CHECK(cap_pwd_limit_cmds(cappwd, cmds, 1) == -1 && errno == ENOTCAPABLE);
	CHECK(cap_pwd_limit_fields(cappwd, fields, 10) == 0);
	CHECK(cap_pwd_limit_users(cappwd, NULL, 0, uids, 5) == 0);

	CHECK(runtest_cmds(cappwd) == (GETPWENT | GETPWENT_R |
	    GETPWNAM | GETPWUID | GETPWUID_R));

	cap_close(cappwd);

	/*
	 * Allow:
	 * cmds: setpwent, getpwent, getpwent_r, getpwnam, getpwnam_r,
	 *       getpwuid_r
	 * users:
	 *     names: entries 0,1,2,3,4,6
	 *     uids:
	 * Disallow:
	 * cmds: getpwuid
	 * users:
	 */
	cap_setpwent(origcappwd);
	cappwd = cap_clone(origcappwd);
	CHECK(cappwd != NULL);

	cmds[0] = "setpwent";
	cmds[1] = "getpwent";
	cmds[2] = "getpwent_r";
	cmds[3] = "getpwnam";
	cmds[4] = "getpwnam_r";
	cmds[5] = "getpwuid_r";
	CHECK(cap_pwd_limit_cmds(cappwd, cmds, 6) == 0);
	cmds[0] = "setpwent";
	cmds[1] = "getpwent";
	cmds[2] = "getpwent_r";
	cmds[3] = "getpwnam";
	cmds[4] = "getpwnam_r";
	cmds[5] = "getpwuid";
	cmds[6] = "getpwuid_r";
	CHECK(cap_pwd_limit_cmds(cappwd, cmds, 7) == -1 && errno == ENOTCAPABLE);
	cmds[0] = "getpwuid";
	CHECK(cap_pwd_limit_cmds(cappwd, cmds, 1) == -1 && errno == ENOTCAPABLE);
	CHECK(cap_pwd_limit_fields(cappwd, fields, 10) == 0);
	CHECK(cap_pwd_limit_users(cappwd, names, 6, NULL, 0) == 0);

	CHECK(runtest_cmds(cappwd) == (GETPWENT | GETPWENT_R |
	    GETPWNAM | GETPWNAM_R | GETPWUID_R));

	cap_close(cappwd);

	/*
	 * Allow:
	 * cmds: setpwent, getpwent, getpwent_r, getpwnam, getpwnam_r,
	 *       getpwuid_r
	 * users:
	 *     names:
	 *     uids: entries 0, 1, 2, 3, 5
	 * Disallow:
	 * cmds: getpwuid
	 * users:
	 */
	cap_setpwent(origcappwd);
	cappwd = cap_clone(origcappwd);
	CHECK(cappwd != NULL);

	cmds[0] = "setpwent";
	cmds[1] = "getpwent";
	cmds[2] = "getpwent_r";
	cmds[3] = "getpwnam";
	cmds[4] = "getpwnam_r";
	cmds[5] = "getpwuid_r";
	CHECK(cap_pwd_limit_cmds(cappwd, cmds, 6) == 0);
	cmds[0] = "setpwent";
	cmds[1] = "getpwent";
	cmds[2] = "getpwent_r";
	cmds[3] = "getpwnam";
	cmds[4] = "getpwnam_r";
	cmds[5] = "getpwuid";
	cmds[6] = "getpwuid_r";
	CHECK(cap_pwd_limit_cmds(cappwd, cmds, 7) == -1 && errno == ENOTCAPABLE);
	cmds[0] = "getpwuid";
	CHECK(cap_pwd_limit_cmds(cappwd, cmds, 1) == -1 && errno == ENOTCAPABLE);
	CHECK(cap_pwd_limit_fields(cappwd, fields, 10) == 0);
	CHECK(cap_pwd_limit_users(cappwd, NULL, 0, uids, 5) == 0);

	CHECK(runtest_cmds(cappwd) == (GETPWENT | GETPWENT_R |
	    GETPWNAM | GETPWNAM_R | GETPWUID_R));

	cap_close(cappwd);

	/*
	 * Allow:
	 * cmds: setpwent, getpwent, getpwent_r, getpwnam, getpwnam_r,
	 *       getpwuid
	 * users:
	 *     names: entries 0,1,2,3,4,6
	 *     uids:
	 * Disallow:
	 * cmds: getpwuid_r
	 * users:
	 */
	cap_setpwent(origcappwd);
	cappwd = cap_clone(origcappwd);
	CHECK(cappwd != NULL);

	cmds[0] = "setpwent";
	cmds[1] = "getpwent";
	cmds[2] = "getpwent_r";
	cmds[3] = "getpwnam";
	cmds[4] = "getpwnam_r";
	cmds[5] = "getpwuid";
	CHECK(cap_pwd_limit_cmds(cappwd, cmds, 6) == 0);
	cmds[0] = "setpwent";
	cmds[1] = "getpwent";
	cmds[2] = "getpwent_r";
	cmds[3] = "getpwnam";
	cmds[4] = "getpwnam_r";
	cmds[5] = "getpwuid";
	cmds[6] = "getpwuid_r";
	CHECK(cap_pwd_limit_cmds(cappwd, cmds, 7) == -1 && errno == ENOTCAPABLE);
	cmds[0] = "getpwuid_r";
	CHECK(cap_pwd_limit_cmds(cappwd, cmds, 1) == -1 && errno == ENOTCAPABLE);
	CHECK(cap_pwd_limit_fields(cappwd, fields, 10) == 0);
	CHECK(cap_pwd_limit_users(cappwd, names, 6, NULL, 0) == 0);

	CHECK(runtest_cmds(cappwd) == (GETPWENT | GETPWENT_R |
	    GETPWNAM | GETPWNAM_R | GETPWUID));

	cap_close(cappwd);

	/*
	 * Allow:
	 * cmds: setpwent, getpwent, getpwent_r, getpwnam, getpwnam_r,
	 *       getpwuid
	 * users:
	 *     names:
	 *     uids: entries 0, 1, 2, 3, 5
	 * Disallow:
	 * cmds: getpwuid_r
	 * users:
	 */
	cap_setpwent(origcappwd);
	cappwd = cap_clone(origcappwd);
	CHECK(cappwd != NULL);

	cmds[0] = "setpwent";
	cmds[1] = "getpwent";
	cmds[2] = "getpwent_r";
	cmds[3] = "getpwnam";
	cmds[4] = "getpwnam_r";
	cmds[5] = "getpwuid";
	CHECK(cap_pwd_limit_cmds(cappwd, cmds, 6) == 0);
	cmds[0] = "setpwent";
	cmds[1] = "getpwent";
	cmds[2] = "getpwent_r";
	cmds[3] = "getpwnam";
	cmds[4] = "getpwnam_r";
	cmds[5] = "getpwuid";
	cmds[6] = "getpwuid_r";
	CHECK(cap_pwd_limit_cmds(cappwd, cmds, 7) == -1 && errno == ENOTCAPABLE);
	cmds[0] = "getpwuid_r";
	CHECK(cap_pwd_limit_cmds(cappwd, cmds, 1) == -1 && errno == ENOTCAPABLE);
	CHECK(cap_pwd_limit_fields(cappwd, fields, 10) == 0);
	CHECK(cap_pwd_limit_users(cappwd, NULL, 0, uids, 5) == 0);

	CHECK(runtest_cmds(cappwd) == (GETPWENT | GETPWENT_R |
	    GETPWNAM | GETPWNAM_R | GETPWUID));

	cap_close(cappwd);
}

#ifdef HAVE_PASSWD_PW_FIELDS
#define	PW_NAME		_PWF_NAME
#define	PW_PASSWD	_PWF_PASSWD
#define	PW_UID		_PWF_UID
#define	PW_GID		_PWF_GID
#define	PW_CHANGE	_PWF_CHANGE
#define	PW_CLASS	_PWF_CLASS
#define	PW_GECOS	_PWF_GECOS
#define	PW_DIR		_PWF_DIR
#define	PW_SHELL	_PWF_SHELL
#define	PW_EXPIRE	_PWF_EXPIRE

static unsigned int
passwd_fields(const struct passwd *pwd)
{
	unsigned int result;

	result = 0;

	if (pwd->pw_name != NULL && pwd->pw_name[0] != '\0')
		result |= PW_NAME;
//	else
//		printf("No pw_name\n");

	if (pwd->pw_passwd != NULL && pwd->pw_passwd[0] != '\0')
		result |= PW_PASSWD;
	else if ((pwd->pw_fields & _PWF_PASSWD) != 0)
		result |= PW_PASSWD;
//	else
//		printf("No pw_passwd\n");

	if (pwd->pw_uid != (uid_t)-1)
		result |= PW_UID;
//	else
//		printf("No pw_uid\n");

	if (pwd->pw_gid != (gid_t)-1)
		result |= PW_GID;
//	else
//		printf("No pw_gid\n");

	if (pwd->pw_change != 0 || (pwd->pw_fields & _PWF_CHANGE) != 0)
		result |= PW_CHANGE;
//	else
//		printf("No pw_change\n");

	if (pwd->pw_class != NULL && pwd->pw_class[0] != '\0')
		result |= PW_CLASS;
	else if ((pwd->pw_fields & _PWF_CLASS) != 0)
		result |= PW_CLASS;
//	else
//		printf("No pw_class\n");

	if (pwd->pw_gecos != NULL && pwd->pw_gecos[0] != '\0')
		result |= PW_GECOS;
	else if ((pwd->pw_fields & _PWF_GECOS) != 0)
		result |= PW_GECOS;
//	else
//		printf("No pw_gecos\n");

	if (pwd->pw_dir != NULL && pwd->pw_dir[0] != '\0')
		result |= PW_DIR;
	else if ((pwd->pw_fields & _PWF_DIR) != 0)
		result |= PW_DIR;
//	else
//		printf("No pw_dir\n");

	if (pwd->pw_shell != NULL && pwd->pw_shell[0] != '\0')
		result |= PW_SHELL;
	else if ((pwd->pw_fields & _PWF_SHELL) != 0)
		result |= PW_SHELL;
//	else
//		printf("No pw_shell\n");

	if (pwd->pw_expire != 0 || (pwd->pw_fields & _PWF_EXPIRE) != 0)
		result |= PW_EXPIRE;
//	else
//		printf("No pw_expire\n");

if (false && pwd->pw_fields != (int)result) {
printf("fields=0x%x != result=0x%x\n", (const unsigned int)pwd->pw_fields, result);
printf("           fields result\n");
printf("PW_NAME    %d      %d\n", (pwd->pw_fields & PW_NAME) != 0, (result & PW_NAME) != 0);
printf("PW_PASSWD  %d      %d\n", (pwd->pw_fields & PW_PASSWD) != 0, (result & PW_PASSWD) != 0);
printf("PW_UID     %d      %d\n", (pwd->pw_fields & PW_UID) != 0, (result & PW_UID) != 0);
printf("PW_GID     %d      %d\n", (pwd->pw_fields & PW_GID) != 0, (result & PW_GID) != 0);
printf("PW_CHANGE  %d      %d\n", (pwd->pw_fields & PW_CHANGE) != 0, (result & PW_CHANGE) != 0);
printf("PW_CLASS   %d      %d\n", (pwd->pw_fields & PW_CLASS) != 0, (result & PW_CLASS) != 0);
printf("PW_GECOS   %d      %d\n", (pwd->pw_fields & PW_GECOS) != 0, (result & PW_GECOS) != 0);
printf("PW_DIR     %d      %d\n", (pwd->pw_fields & PW_DIR) != 0, (result & PW_DIR) != 0);
printf("PW_SHELL   %d      %d\n", (pwd->pw_fields & PW_SHELL) != 0, (result & PW_SHELL) != 0);
printf("PW_EXPIRE  %d      %d\n", (pwd->pw_fields & PW_EXPIRE) != 0, (result & PW_EXPIRE) != 0);
}

//printf("result=0x%x\n", result);
	return (result);
}
#else

#define	PW_NAME	0x0001
#define	PW_PASSWD	0x0002
#define	PW_UID		0x0004
#define	PW_GID		0x0008
#define	PW_CHANGE	0x0010
#define	PW_CLASS	0x0020
#define	PW_GECOS	0x0040
#define	PW_DIR		0x0080
#define	PW_SHELL	0x0100
#define	PW_EXPIRE	0x0200

static unsigned int
passwd_fields(const struct passwd *pwd)
{
	return 0;
}

#endif

static bool
runtest_fields(cap_channel_t *cappwd, unsigned int expected)
{
	char buf[1024];
	struct passwd *pwd;
	struct passwd st;

//printf("expected=0x%x\n", expected);
	cap_setpwent(cappwd);
	pwd = cap_getpwent(cappwd);
	if ((passwd_fields(pwd) & ~expected) != 0)
		return (false);

	cap_setpwent(cappwd);
	cap_getpwent_r(cappwd, &st, buf, sizeof(buf), &pwd);
	if ((passwd_fields(pwd) & ~expected) != 0)
		return (false);

	pwd = cap_getpwnam(cappwd, usernames[0]);
	if ((passwd_fields(pwd) & ~expected) != 0)
		return (false);

	cap_getpwnam_r(cappwd, usernames[0], &st, buf, sizeof(buf), &pwd);
	if ((passwd_fields(pwd) & ~expected) != 0)
		return (false);

	pwd = cap_getpwuid(cappwd, UID_ROOT);
	if ((passwd_fields(pwd) & ~expected) != 0)
		return (false);

	cap_getpwuid_r(cappwd, UID_ROOT, &st, buf, sizeof(buf), &pwd);
	if ((passwd_fields(pwd) & ~expected) != 0)
		return (false);

	return (true);
}

static void
test_fields(cap_channel_t *origcappwd)
{
	cap_channel_t *cappwd;
	const char *fields[10];

	/* No limits. */

	CHECK(runtest_fields(origcappwd, PW_NAME | PW_PASSWD | PW_UID |
	    PW_GID | PW_CHANGE | PW_CLASS | PW_GECOS | PW_DIR | PW_SHELL |
	    PW_EXPIRE));

	/*
	 * Allow:
	 * fields: pw_name, pw_passwd, pw_uid, pw_gid, pw_change, pw_class,
	 *         pw_gecos, pw_dir, pw_shell, pw_expire
	 */
	cap_setpwent(origcappwd);
	cappwd = cap_clone(origcappwd);
	CHECK(cappwd != NULL);

	fields[0] = "pw_name";
	fields[1] = "pw_passwd";
	fields[2] = "pw_uid";
	fields[3] = "pw_gid";
	fields[4] = "pw_change";
	fields[5] = "pw_class";
	fields[6] = "pw_gecos";
	fields[7] = "pw_dir";
	fields[8] = "pw_shell";
	fields[9] = "pw_expire";
	CHECK(cap_pwd_limit_fields(cappwd, fields, 10) == 0);

	CHECK(runtest_fields(origcappwd, PW_NAME | PW_PASSWD | PW_UID |
	    PW_GID | PW_CHANGE | PW_CLASS | PW_GECOS | PW_DIR | PW_SHELL |
	    PW_EXPIRE));

	cap_close(cappwd);

	/*
	 * Allow:
	 * fields: pw_name, pw_passwd, pw_uid, pw_gid, pw_change
	 */
	cap_setpwent(origcappwd);
	cappwd = cap_clone(origcappwd);
	CHECK(cappwd != NULL);

	fields[0] = "pw_name";
	fields[1] = "pw_passwd";
	fields[2] = "pw_uid";
	fields[3] = "pw_gid";
	fields[4] = "pw_change";
	CHECK(cap_pwd_limit_fields(cappwd, fields, 5) == 0);
	fields[5] = "pw_class";
	CHECK(cap_pwd_limit_fields(cappwd, fields, 6) == -1 &&
	    errno == ENOTCAPABLE);
	fields[0] = "pw_class";
	CHECK(cap_pwd_limit_fields(cappwd, fields, 1) == -1 &&
	    errno == ENOTCAPABLE);

	CHECK(runtest_fields(cappwd, PW_NAME | PW_PASSWD | PW_UID |
	    PW_GID | PW_CHANGE));

	cap_close(cappwd);

	/*
	 * Allow:
	 * fields: pw_class, pw_gecos, pw_dir, pw_shell, pw_expire
	 */
	cap_setpwent(origcappwd);
	cappwd = cap_clone(origcappwd);
	CHECK(cappwd != NULL);

	fields[0] = "pw_class";
	fields[1] = "pw_gecos";
	fields[2] = "pw_dir";
	fields[3] = "pw_shell";
	fields[4] = "pw_expire";
	CHECK(cap_pwd_limit_fields(cappwd, fields, 5) == 0);
	fields[5] = "pw_uid";
	CHECK(cap_pwd_limit_fields(cappwd, fields, 6) == -1 &&
	    errno == ENOTCAPABLE);
	fields[0] = "pw_uid";
	CHECK(cap_pwd_limit_fields(cappwd, fields, 1) == -1 &&
	    errno == ENOTCAPABLE);

	CHECK(runtest_fields(cappwd, PW_CLASS | PW_GECOS | PW_DIR |
	    PW_SHELL | PW_EXPIRE));

	cap_close(cappwd);

	/*
	 * Allow:
	 * fields: pw_name, pw_uid, pw_change, pw_gecos, pw_shell
	 */
	cap_setpwent(origcappwd);
	cappwd = cap_clone(origcappwd);
	CHECK(cappwd != NULL);

	fields[0] = "pw_name";
	fields[1] = "pw_uid";
	fields[2] = "pw_change";
	fields[3] = "pw_gecos";
	fields[4] = "pw_shell";
	CHECK(cap_pwd_limit_fields(cappwd, fields, 5) == 0);
	fields[5] = "pw_class";
	CHECK(cap_pwd_limit_fields(cappwd, fields, 6) == -1 &&
	    errno == ENOTCAPABLE);
	fields[0] = "pw_class";
	CHECK(cap_pwd_limit_fields(cappwd, fields, 1) == -1 &&
	    errno == ENOTCAPABLE);

	CHECK(runtest_fields(cappwd, PW_NAME | PW_UID | PW_CHANGE |
	    PW_GECOS | PW_SHELL));

	cap_close(cappwd);

	/*
	 * Allow:
	 * fields: pw_passwd, pw_gid, pw_class, pw_dir, pw_expire
	 */
	cap_setpwent(origcappwd);
	cappwd = cap_clone(origcappwd);
	CHECK(cappwd != NULL);

	fields[0] = "pw_passwd";
	fields[1] = "pw_gid";
	fields[2] = "pw_class";
	fields[3] = "pw_dir";
	fields[4] = "pw_expire";
	CHECK(cap_pwd_limit_fields(cappwd, fields, 5) == 0);
	fields[5] = "pw_uid";
	CHECK(cap_pwd_limit_fields(cappwd, fields, 6) == -1 &&
	    errno == ENOTCAPABLE);
	fields[0] = "pw_uid";
	CHECK(cap_pwd_limit_fields(cappwd, fields, 1) == -1 &&
	    errno == ENOTCAPABLE);

	CHECK(runtest_fields(cappwd, PW_PASSWD | PW_GID | PW_CLASS |
	    PW_DIR | PW_EXPIRE));

	cap_close(cappwd);

	/*
	 * Allow:
	 * fields: pw_uid, pw_class, pw_shell
	 */
	cap_setpwent(origcappwd);
	cappwd = cap_clone(origcappwd);
	CHECK(cappwd != NULL);

	fields[0] = "pw_uid";
	fields[1] = "pw_class";
	fields[2] = "pw_shell";
	CHECK(cap_pwd_limit_fields(cappwd, fields, 3) == 0);
	fields[3] = "pw_change";
	CHECK(cap_pwd_limit_fields(cappwd, fields, 4) == -1 &&
	    errno == ENOTCAPABLE);
	fields[0] = "pw_change";
	CHECK(cap_pwd_limit_fields(cappwd, fields, 1) == -1 &&
	    errno == ENOTCAPABLE);

	CHECK(runtest_fields(cappwd, PW_UID | PW_CLASS | PW_SHELL));

	cap_close(cappwd);

	/*
	 * Allow:
	 * fields: pw_change
	 */
	cap_setpwent(origcappwd);
	cappwd = cap_clone(origcappwd);
	CHECK(cappwd != NULL);

	fields[0] = "pw_change";
	CHECK(cap_pwd_limit_fields(cappwd, fields, 1) == 0);
	fields[1] = "pw_uid";
	CHECK(cap_pwd_limit_fields(cappwd, fields, 2) == -1 &&
	    errno == ENOTCAPABLE);
	fields[0] = "pw_uid";
	CHECK(cap_pwd_limit_fields(cappwd, fields, 1) == -1 &&
	    errno == ENOTCAPABLE);

	CHECK(runtest_fields(cappwd, PW_CHANGE));

	cap_close(cappwd);
}

static bool
runtest_users(cap_channel_t *cappwd, const char **names, const uid_t *uids,
    size_t nusers)
{
	char buf[1024];
	struct passwd *pwd;
	struct passwd st;
	unsigned int i, got;

	cap_setpwent(cappwd);
	got = 0;
	for (;;) {
		pwd = cap_getpwent(cappwd);
		if (pwd == NULL)
			break;
		got++;
		for (i = 0; i < nusers; i++) {
			if (strcmp(names[i], pwd->pw_name) == 0 &&
			    uids[i] == pwd->pw_uid) {
				break;
			}
		}
		if (i == nusers)
			return (false);
	}
	if (got != nusers)
		return (false);

	cap_setpwent(cappwd);
	got = 0;
	for (;;) {
		cap_getpwent_r(cappwd, &st, buf, sizeof(buf), &pwd);
		if (pwd == NULL)
			break;
		got++;
		for (i = 0; i < nusers; i++) {
			if (strcmp(names[i], pwd->pw_name) == 0 &&
			    uids[i] == pwd->pw_uid) {
				break;
			}
		}
		if (i == nusers)
			return (false);
	}
	if (got != nusers)
		return (false);

	for (i = 0; i < nusers; i++) {
		pwd = cap_getpwnam(cappwd, names[i]);
		if (pwd == NULL)
			return (false);
	}

	for (i = 0; i < nusers; i++) {
		cap_getpwnam_r(cappwd, names[i], &st, buf, sizeof(buf), &pwd);
		if (pwd == NULL)
			return (false);
	}

	for (i = 0; i < nusers; i++) {
		pwd = cap_getpwuid(cappwd, uids[i]);
		if (pwd == NULL)
			return (false);
	}

	for (i = 0; i < nusers; i++) {
		cap_getpwuid_r(cappwd, uids[i], &st, buf, sizeof(buf), &pwd);
		if (pwd == NULL)
			return (false);
	}

	return (true);
}

static void
test_users(cap_channel_t *origcappwd)
{
	cap_channel_t *cappwd;
	const char *names[6];
	uid_t uids[6];

	/*
	 * Allow:
	 * users:
	 *     names: entries 0-5
	 *     uids:
	 */
	cap_setpwent(origcappwd);
	cappwd = cap_clone(origcappwd);
	CHECK(cappwd != NULL);

	names[0] = usernames[0];
	names[1] = usernames[1];
	names[2] = usernames[2];
	names[3] = usernames[3];
	names[4] = usernames[4];
	names[5] = usernames[5];
	CHECK(cap_pwd_limit_users(cappwd, names, 6, NULL, 0) == 0);
	uids[0] = userids[0];
	uids[1] = userids[1];
	uids[2] = userids[2];
	uids[3] = userids[3];
	uids[4] = userids[4];
	uids[5] = userids[5];

	CHECK(runtest_users(cappwd, names, uids, 6));

	cap_close(cappwd);

	/*
	 * Allow:
	 * users:
	 *     names: entries 2-4
	 *     uids:
	 */
	cap_setpwent(origcappwd);
	cappwd = cap_clone(origcappwd);
	CHECK(cappwd != NULL);

	names[0] = usernames[2];
	names[1] = usernames[3];
	names[2] = usernames[4];
	CHECK(cap_pwd_limit_users(cappwd, names, 3, NULL, 0) == 0);
	names[3] = usernames[5];
	CHECK(cap_pwd_limit_users(cappwd, names, 4, NULL, 0) == -1 &&
	    errno == ENOTCAPABLE);
	names[0] = usernames[5];
	CHECK(cap_pwd_limit_users(cappwd, names, 1, NULL, 0) == -1 &&
	    errno == ENOTCAPABLE);
	names[0] = usernames[2];
	uids[0] = userids[2];
	uids[1] = userids[3];
	uids[2] = userids[4];

	CHECK(runtest_users(cappwd, names, uids, 3));

	cap_close(cappwd);

	/*
	 * Allow:
	 * users:
	 *     names: entries 2,4,5
	 *     uids:
	 */
	cap_setpwent(origcappwd);
	cappwd = cap_clone(origcappwd);
	CHECK(cappwd != NULL);

	names[0] = usernames[2];
	names[1] = usernames[4];
	names[2] = usernames[5];
	CHECK(cap_pwd_limit_users(cappwd, names, 3, NULL, 0) == 0);
	names[3] = usernames[3];
	CHECK(cap_pwd_limit_users(cappwd, names, 4, NULL, 0) == -1 &&
	    errno == ENOTCAPABLE);
	names[0] = usernames[3];
	CHECK(cap_pwd_limit_users(cappwd, names, 1, NULL, 0) == -1 &&
	    errno == ENOTCAPABLE);
	names[0] = usernames[2];
	uids[0] = userids[2];
	uids[1] = userids[4];
	uids[2] = userids[5];

	CHECK(runtest_users(cappwd, names, uids, 3));

	cap_close(cappwd);

	/*
	 * Allow:
	 * users:
	 *     names:
	 *     uids: 1, 2, 3
	 */
	cap_setpwent(origcappwd);
	cappwd = cap_clone(origcappwd);
	CHECK(cappwd != NULL);

	names[0] = usernames[1];
	names[1] = usernames[2];
	names[2] = usernames[3];
	uids[0] = userids[1];
	uids[1] = userids[2];
	uids[2] = userids[3];
	CHECK(cap_pwd_limit_users(cappwd, NULL, 0, uids, 3) == 0);
	uids[3] = userids[4];
	CHECK(cap_pwd_limit_users(cappwd, NULL, 0, uids, 4) == -1 &&
	    errno == ENOTCAPABLE);
	uids[0] = userids[4];
	CHECK(cap_pwd_limit_users(cappwd, NULL, 0, uids, 1) == -1 &&
	    errno == ENOTCAPABLE);
	uids[0] = userids[1];

	CHECK(runtest_users(cappwd, names, uids, 3));

	cap_close(cappwd);

	/*
	 * Allow:
	 * users:
	 *     names:
	 *     uids: 1, 3, 4
	 */
	cap_setpwent(origcappwd);
	cappwd = cap_clone(origcappwd);
	CHECK(cappwd != NULL);

	names[0] = usernames[1];
	names[1] = usernames[3];
	names[2] = usernames[4];
	uids[0] = userids[1];
	uids[1] = userids[3];
	uids[2] = userids[4];
	CHECK(cap_pwd_limit_users(cappwd, NULL, 0, uids, 3) == 0);
	uids[3] = userids[5];
	CHECK(cap_pwd_limit_users(cappwd, NULL, 0, uids, 4) == -1 &&
	    errno == ENOTCAPABLE);
	uids[0] = userids[5];
	CHECK(cap_pwd_limit_users(cappwd, NULL, 0, uids, 1) == -1 &&
	    errno == ENOTCAPABLE);
	uids[0] = userids[1];

	CHECK(runtest_users(cappwd, names, uids, 3));

	cap_close(cappwd);

	/*
	 * Allow:
	 * users:
	 *     names: entry 4
	 *     uids:
	 */
	cap_setpwent(origcappwd);
	cappwd = cap_clone(origcappwd);
	CHECK(cappwd != NULL);

	names[0] = usernames[4];
	CHECK(cap_pwd_limit_users(cappwd, names, 1, NULL, 0) == 0);
	names[1] = usernames[3];
	CHECK(cap_pwd_limit_users(cappwd, names, 2, NULL, 0) == -1 &&
	    errno == ENOTCAPABLE);
	names[0] = usernames[3];
	CHECK(cap_pwd_limit_users(cappwd, names, 1, NULL, 0) == -1 &&
	    errno == ENOTCAPABLE);
	names[0] = usernames[4];
	uids[0] = userids[4];

	CHECK(runtest_users(cappwd, names, uids, 1));

	cap_close(cappwd);

	/*
	 * Allow:
	 * users:
	 *     names: entries 2, 5
	 *     uids:
	 */
	cap_setpwent(origcappwd);
	cappwd = cap_clone(origcappwd);
	CHECK(cappwd != NULL);

	names[0] = usernames[2];
	names[1] = usernames[5];
	CHECK(cap_pwd_limit_users(cappwd, names, 2, NULL, 0) == 0);
	names[2] = usernames[3];
	CHECK(cap_pwd_limit_users(cappwd, names, 3, NULL, 0) == -1 &&
	    errno == ENOTCAPABLE);
	names[0] = usernames[3];
	CHECK(cap_pwd_limit_users(cappwd, names, 1, NULL, 0) == -1 &&
	    errno == ENOTCAPABLE);
	names[0] = usernames[2];
	uids[0] = userids[2];
	uids[1] = userids[5];

	CHECK(runtest_users(cappwd, names, uids, 2));

	cap_close(cappwd);

	/*
	 * Allow:
	 * users:
	 *     names:
	 *     uids: 3
	 */
	cap_setpwent(origcappwd);
	cappwd = cap_clone(origcappwd);
	CHECK(cappwd != NULL);

	names[0] = usernames[3];
	uids[0] = userids[3];
	CHECK(cap_pwd_limit_users(cappwd, NULL, 0, uids, 1) == 0);
	uids[1] = userids[4];
	CHECK(cap_pwd_limit_users(cappwd, NULL, 0, uids, 2) == -1 &&
	    errno == ENOTCAPABLE);
	uids[0] = userids[4];
	CHECK(cap_pwd_limit_users(cappwd, NULL, 0, uids, 1) == -1 &&
	    errno == ENOTCAPABLE);
	uids[0] = userids[3];

	CHECK(runtest_users(cappwd, names, uids, 1));

	cap_close(cappwd);

	/*
	 * Allow:
	 * users:
	 *     names:
	 *     uids: 1, 4
	 */
	cap_setpwent(origcappwd);
	cappwd = cap_clone(origcappwd);
	CHECK(cappwd != NULL);

	names[0] = usernames[1];
	names[1] = usernames[4];
	uids[0] = userids[1];
	uids[1] = userids[4];
	CHECK(cap_pwd_limit_users(cappwd, NULL, 0, uids, 2) == 0);
	uids[2] = userids[3];
	CHECK(cap_pwd_limit_users(cappwd, NULL, 0, uids, 3) == -1 &&
	    errno == ENOTCAPABLE);
	uids[0] = userids[3];
	CHECK(cap_pwd_limit_users(cappwd, NULL, 0, uids, 1) == -1 &&
	    errno == ENOTCAPABLE);
	uids[0] = userids[1];

	CHECK(runtest_users(cappwd, names, uids, 2));

	cap_close(cappwd);
}

int
main(void)
{
	cap_channel_t *capcas, *cappwd;

	save_users();
	printf("1..188\n");

	capcas = cap_init();
	CHECKX(capcas != NULL);

	cappwd = cap_service_open(capcas, "system.pwd");
	CHECKX(cappwd != NULL);

	cap_close(capcas);

	/* No limits. */

	CHECK(runtest_cmds(cappwd) == (GETPWENT | GETPWENT_R | GETPWNAM |
	    GETPWNAM_R | GETPWUID | GETPWUID_R));

	test_cmds(cappwd);

	test_fields(cappwd);

	test_users(cappwd);

	cap_close(cappwd);

	free_users();
	exit(failures);
}