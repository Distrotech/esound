/* Evil evil evil hack to get OSS apps to cooperate with esd
 * Copyright (C) 1998 Manish Singh <yosh@gimp.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/* #define DSP_DEBUG */

/* This lets you run multiple instances of x11amp by setting the X11AMPNUM
   environment variable. Only works on glibc2.
 */
/* #define MULTIPLE_X11AMP */

#include "config.h"

#include <dlfcn.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>

#ifdef HAVE_MACHINE_SOUNDCARD_H
#  include <machine/soundcard.h>
#else
#  ifdef HAVE_SOUNDCARD_H
#    include <soundcard.h>
#  else
#    include <sys/soundcard.h>
#  endif
#endif

#include "esd.h"

#define REAL_LIBC RTLD_NEXT

#ifdef __FreeBSD__
typedef unsigned long request_t;
#else
typedef int request_t;
#endif

static int sndfd = -1;
static int settings = 0, done = 0;

int
open (const char *pathname, int flags, ...)
{
  static int (*func) (const char *, int, mode_t) = NULL;
  va_list args;
  mode_t mode;

  if (!func)
    func = (int (*) (const char *, int, mode_t)) dlsym (REAL_LIBC, "open");

  va_start (args, flags);
  mode = va_arg (args, mode_t);
  va_end (args);

  if (!strcmp (pathname, "/dev/dsp"))
    {
      if (!getenv ("ESPEAKER"))
	{
          int ret;

	  flags |= O_NONBLOCK;
	  if ((ret = (*func) (pathname, flags, mode)) >= 0)
	    return ret;
	}

      settings = done = 0;

#ifdef DSP_DEBUG
      printf ("hijacking /dev/dsp open, and taking it to esd...\n");
#endif

      return (sndfd = esd_open_sound (NULL));
    }
  else
    return (*func) (pathname, flags, mode);
}

int
ioctl (int fd, request_t request, ...)
{
  static int (*func) (int, request_t, void *) = NULL;
  va_list args;
  void *argp;

  static esd_format_t fmt = ESD_STREAM | ESD_PLAY | ESD_MONO;
  static int speed;

  if (!func)                                                                    
    func = (int (*) (int, request_t, void *)) dlsym (REAL_LIBC, "ioctl");             
  va_start (args, request);
  argp = va_arg (args, void *);
  va_end (args);

  if (fd != sndfd)
    return (*func) (fd, request, argp);
  else if (sndfd != -1)
    {
      int *arg = (int *) argp;

#ifdef DSP_DEBUG
      printf ("hijacking /dev/dsp ioctl, and sending it to esd "
	      "(%d : %x - %p)\n", fd, request, argp);
#endif

      switch (request)
	{
        case SNDCTL_DSP_SETFMT:
	  fmt |= (*arg & 0x30) ? ESD_BITS16 : ESD_BITS8;
	  settings |= 1;
	  break;

	case SNDCTL_DSP_SPEED:
	  speed = *arg;
	  settings |= 2;
	  break;

	case SNDCTL_DSP_STEREO:
	  fmt &= ~ESD_MONO;
	  fmt |= (*arg) ? ESD_STEREO : ESD_MONO;
	  break;

	case SNDCTL_DSP_GETBLKSIZE:
	  *arg = ESD_BUF_SIZE;
	  break;

	case SNDCTL_DSP_GETFMTS:
	  *arg = 0x38;
	  break;

	case SNDCTL_DSP_GETCAPS:
	  *arg = 0;
	  break;

	default:
#ifdef DSP_DEBUG
	  printf ("unhandled /dev/dsp ioctl (%x - %p)\n", request, argp);
#endif
	  break;
	}

      if (settings == 3 && !done)
	{
	  int proto = ESD_PROTO_STREAM_PLAY;
	  char buf[ESD_NAME_MAX];
	  strncpy (buf, /* use environment variable for alternate name */
		   (getenv ("ESDDSP_NAME") ? getenv ("ESDDSP_NAME") : "esddsp"),
		   ESD_NAME_MAX);

	  done = 1;

          if (write (sndfd, &proto, sizeof (proto)) != sizeof (proto))
	    return -1;
          if (write (sndfd, &fmt, sizeof (fmt)) != sizeof (fmt))
	    return -1;
          if (write (sndfd, &speed, sizeof (speed)) != sizeof (speed))
	    return -1;
	  if (write (sndfd, buf, ESD_NAME_MAX) != ESD_NAME_MAX)
	    return -1;

	  fmt = ESD_STREAM | ESD_PLAY | ESD_MONO;
	  speed = 0;
	}

      return 0;
    }

  return 0;
}

int
close (int fd)
{
  static int (*func) (int) = NULL;

  if (!func)
    func = (int (*) (int)) dlsym (REAL_LIBC, "close");

  if (fd == sndfd)
    sndfd = -1;

  return (*func) (fd);
}

#ifdef MULTIPLE_X11AMP

#include <socketbits.h>
#include <sys/param.h>
#include <sys/un.h>

#define ENVSET "X11AMPNUM"

int
unlink (const char *filename)
{
  static int (*func) (const char *) = NULL;
  char *num;

  if (!func)
    func = (int (*) (const char *)) dlsym (REAL_LIBC, "unlink");

  if (!strcmp (filename, "/tmp/X11Amp_CTRL") && (num = getenv (ENVSET)))
    {
      char buf[PATH_MAX] = "/tmp/X11Amp_CTRL";
      strcat (buf, num);
      return (*func) (buf); 
    }
  else
    return (*func) (filename);
}

typedef int (*sa_func_t) (int, struct sockaddr *, int);

static int
sockaddr_mangle (sa_func_t func, int fd, struct sockaddr *addr, int len)
{
  char *num;

  if (!strcmp (((struct sockaddr_un *) addr)->sun_path, "/tmp/X11Amp_CTRL")
      && (num = getenv(ENVSET)))
    {
      int ret;
      char buf[PATH_MAX] = "/tmp/X11Amp_CTRL";

      struct sockaddr_un *new_addr = malloc (len);

      strcat (buf, num);
      memcpy (new_addr, addr, len);
      strcpy (new_addr->sun_path, buf);

      ret = (*func) (fd, (struct sockaddr *) new_addr, len);

      free (new_addr);
      return ret;
    } 
  else
    return (*func) (fd, addr, len);
}

int
bind (int fd, struct sockaddr *addr, int len)
{
  static sa_func_t func = NULL;

  if (!func)
    func = (sa_func_t) dlsym (REAL_LIBC, "bind");
  return sockaddr_mangle (func, fd, addr, len);
}

int
connect (int fd, struct sockaddr *addr, int len)
{
  static sa_func_t func = NULL;

  if (!func)
    func = (sa_func_t) dlsym (REAL_LIBC, "connect");
  return sockaddr_mangle (func, fd, addr, len);
}

#endif /* MULTIPLE_X11AMP */
