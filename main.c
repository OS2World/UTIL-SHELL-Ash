/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Kenneth Almquist.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
static char const copyright[] =
"@(#) Copyright (c) 1991, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)main.c	8.6 (Berkeley) 5/28/95";
#endif
static const char rcsid[] =
	"$Id: main.c,v 1.9.2.3 1998/11/03 15:57:35 cracauer Exp $";
#endif /* not lint */

#include <stdio.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <locale.h>
#ifdef __EMX__
#include <sys/param.h>
#include <stdlib.h>
#endif


#include "shell.h"
#include "main.h"
#include "mail.h"
#include "options.h"
#include "output.h"
#include "parser.h"
#include "nodes.h"
#include "expand.h"
#include "eval.h"
#include "jobs.h"
#include "input.h"
#include "trap.h"
#include "var.h"
#include "show.h"
#include "memalloc.h"
#include "error.h"
#include "init.h"
#include "mystring.h"
#include "exec.h"
#include "cd.h"

#define PROFILE 0

int rootpid;
int rootshell;
extern int errno;
#if PROFILE
short profile_buf[16384];
extern int etext();
#endif

STATIC void read_profile __P((char *));
STATIC char *find_dot_file __P((char *));

#ifdef __EMX__
char *etc_profile = NULL;
char *home_profile = NULL;

void
STATIC prepare_profiles()
        {
        char *etc_dir;
        char *home_dir;

        etc_dir = getenv("INIT");
        if (!etc_dir) {
                etc_dir = getenv("ETC");
                if (!etc_dir)
                        etc_dir = "/etc";
        }
        home_dir = getenv("HOME");
        if (!home_dir)
                home_dir = ".";
        if (etc_profile)
                ckfree(etc_profile);
        if (home_profile)
                ckfree(home_profile);
        etc_profile = ckmalloc(MAXPATHLEN);
        home_profile = ckmalloc(MAXPATHLEN);
        _makepath(etc_profile, NULL, etc_dir, "profile", NULL);
        _makepath(home_profile, NULL, home_dir, "profile", NULL);
}

#endif /* __EMX__ */

/*
 * Main routine.  We initialize things, parse the arguments, execute
 * profiles if we're a login shell, and then call cmdloop to execute
 * commands.  The setjmp call sets up the location to jump to when an
 * exception occurs.  When an exception occurs the variable "state"
 * is used to figure out how far we had gotten.
 */

int
main(argc, argv)
	int argc;
	char **argv;
{
	struct jmploc jmploc;
	struct stackmark smark;
	volatile int state;
	char *shinit;

#if PROFILE
	monitor(4, etext, profile_buf, sizeof profile_buf, 50);
#endif
#ifndef NO_LOCALE
	(void) setlocale(LC_ALL, "");
#endif
	state = 0;
	if (setjmp(jmploc.loc)) {
		/*
		 * When a shell procedure is executed, we raise the
		 * exception EXSHELLPROC to clean up before executing
		 * the shell procedure.
		 */
		switch (exception) {
		case EXSHELLPROC:
			rootpid = getpid();
			rootshell = 1;
			minusc = NULL;
			state = 3;
			break;

		case EXEXEC:
			exitstatus = exerrno;
			break;

		case EXERROR:
			exitstatus = 2;
			break;

		default:
			break;
		}

		if (exception != EXSHELLPROC) {
		    if (state == 0 || iflag == 0 || ! rootshell)
			    exitshell(exitstatus);
		}
		reset();
		if (exception == EXINT
#if ATTY
		 && (! attyset() || equal(termval(), "emacs"))
#endif
		 ) {
			out2c('\n');
			flushout(&errout);
		}
		popstackmark(&smark);
		FORCEINTON;				/* enable interrupts */
		if (state == 1)
			goto state1;
		else if (state == 2)
			goto state2;
		else if (state == 3)
			goto state3;
		else
			goto state4;
	}
	handler = &jmploc;
#ifdef DEBUG
	opentrace();
	trputs("Shell args:  ");  trargs(argv);
#endif
	rootpid = getpid();
	rootshell = 1;
	init();
	setstackmark(&smark);
	procargs(argc, argv);
	if (getpwd() == NULL && iflag)
		out2str("sh: cannot determine working directory\n");
	if (argv[0] && argv[0][0] == '-') {
#ifdef __EMX__
                prepare_profiles();
#endif
		state = 1;
#ifndef __EMX__
		read_profile("/etc/profile");
#else
		read_profile(etc_profile);
#endif
state1:
		state = 2;
#ifndef __EMX__
		if (privileged == 0)
			read_profile(".profile");
		else
			read_profile("/etc/suid_profile");
#else
		read_profile(home_profile);
#endif
	}
state2:
	state = 3;
	if (!privileged && iflag) {
		if ((shinit = lookupvar("ENV")) != NULL && *shinit != '\0') {
			state = 3;
			read_profile(shinit);
		}
	}
state3:
	state = 4;
	if (minusc) {
		evalstring(minusc);
	}
	if (sflag || minusc == NULL) {
state4:	/* XXX ??? - why isn't this before the "if" statement */
		cmdloop(1);
	}
#if PROFILE
	monitor(0);
#endif
	exitshell(exitstatus);
	/*NOTREACHED*/
	return 0;
}


/*
 * Read and execute commands.  "Top" is nonzero for the top level command
 * loop; it turns on prompting if the shell is interactive.
 */

void
cmdloop(top)
	int top;
{
	union node *n;
	struct stackmark smark;
	int inter;
	int numeof = 0;

	TRACE(("cmdloop(%d) called\n", top));
	setstackmark(&smark);
	for (;;) {
		if (pendingsigs)
			dotrap();
		inter = 0;
		if (iflag && top) {
			inter++;
			showjobs(1);
#ifndef NO_MAIL
			chkmail(0);
#endif
			flushout(&output);
		}
		n = parsecmd(inter);
		/* showtree(n); DEBUG */
		if (n == NEOF) {
			if (!top || numeof >= 50)
				break;
			if (!stoppedjobs()) {
				if (!Iflag)
					break;
				out2str("\nUse \"exit\" to leave shell.\n");
			}
			numeof++;
		} else if (n != NULL && nflag == 0) {
			job_warning = (job_warning == 2) ? 1 : 0;
			numeof = 0;
			evaltree(n, 0);
		}
		popstackmark(&smark);
		if (evalskip == SKIPFILE) {
			evalskip = 0;
			break;
		}
	}
	popstackmark(&smark);		/* unnecessary */
}



/*
 * Read /etc/profile or .profile.  Return on error.
 */

STATIC void
read_profile(name)
	char *name;
	{
	int fd;

	INTOFF;
	if ((fd = open(name, O_RDONLY)) >= 0)
		setinputfd(fd, 1);
	INTON;
	if (fd < 0)
		return;
	cmdloop(0);
	popfile();
}



/*
 * Read a file containing shell functions.
 */

void
readcmdfile(name)
	char *name;
{
	int fd;

	INTOFF;
	if ((fd = open(name, O_RDONLY)) >= 0)
		setinputfd(fd, 1);
	else
		error("Can't open %s", name);
	INTON;
	cmdloop(0);
	popfile();
}



/*
 * Take commands from a file.  To be compatable we should do a path
 * search for the file, which is necessary to find sub-commands.
 */


STATIC char *
find_dot_file(basename)
	char *basename;
{
	static char localname[FILENAME_MAX+1];
	char *fullname;
	char *path = pathval();
	struct stat statb;

	/* don't try this for absolute or relative paths */
	if( strchr(basename, '/'))
		return basename;

	while ((fullname = padvance(&path, basename)) != NULL) {
		strcpy(localname, fullname);
		stunalloc(fullname);
		if ((stat(fullname, &statb) == 0) && S_ISREG(statb.st_mode))
			return localname;
	}
	return basename;
}

int
dotcmd(argc, argv)
	int argc;
	char **argv;
{
	struct strlist *sp;
	exitstatus = 0;

	for (sp = cmdenviron; sp ; sp = sp->next)
		setvareq(savestr(sp->text), VSTRFIXED|VTEXTFIXED);

	if (argc >= 2) {		/* That's what SVR2 does */
		char *fullname = find_dot_file(argv[1]);

		setinputfile(fullname, 1);
		commandname = fullname;
		cmdloop(0);
		popfile();
	}
	return exitstatus;
}


int
exitcmd(argc, argv)
	int argc;
	char **argv;
{
	extern int oexitstatus;

	if (stoppedjobs())
		return 0;
	if (argc > 1)
		exitstatus = number(argv[1]);
	else
		exitstatus = oexitstatus;
	exitshell(exitstatus);
	/*NOTREACHED*/
	return 0;
}


#ifdef notdef
/*
 * Should never be called.
 */

void
exit(exitstatus) {
	_exit(exitstatus);
}
#endif
