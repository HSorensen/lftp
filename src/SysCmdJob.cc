/*
 * lftp and utils
 *
 * Copyright (c) 1996-1997 by Alexander V. Lukyanov (lav@yars.free.net)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/* $Id$ */

#include <config.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>

#include "SysCmdJob.h"
#include "SignalHook.h"
#include "xmalloc.h"
#include "misc.h"

SysCmdJob::SysCmdJob(const char *c)
{
   w=0;
   cmd=xstrdup(c);
}

SysCmdJob::~SysCmdJob()
{
   Bg();
   AcceptSig(SIGTERM);

   if(w)
   {
      w->Auto();
      w=0;
   }
}

int SysCmdJob::Do()
{
   int m=STALL;

   if(w)
      return m;

   const char *shell=getenv("SHELL");
   if(!shell)
      shell="/bin/sh";

   ProcWait::Signal(false);

   pid_t pid;
   fflush(stderr);
   switch(pid=fork())
   {
   case(0): /* child */
      setpgid(0,0);
      kill(getpid(),SIGSTOP);
      SignalHook::RestoreAll();
      if(cmd)
	 execlp(shell,basename_ptr(shell),"-c",cmd,(char*)NULL);
      else
	 execlp(shell,basename_ptr(shell),(char*)NULL);
      fprintf(stderr,_("execlp(%s) failed: %s\n"),shell,strerror(errno));
      fflush(stderr);
      _exit(1);
   case(-1): /* error */
      block+=TimeOut(1000);   // wait a second and retry
      goto out;
   }
   /* parent */
   int info;
   waitpid(pid,&info,WUNTRACED); // wait until the process stops
   w=new ProcWait(pid);
   fg_data=new FgData(pid,fg);
   m=MOVED;
out:
   ProcWait::Signal(true);
   return m;
}

int SysCmdJob::AcceptSig(int sig)
{
   if(!w)
   {
      if(sig==SIGINT)
	 return WANTDIE;
      return STALL;
   }
   w->Kill(sig);
   if(sig!=SIGCONT)
      AcceptSig(SIGCONT);  // for the case of stopped process
   return MOVED;
}
