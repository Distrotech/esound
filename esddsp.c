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

/* To build and use:
   gcc -shared -O2 -pipe -fPIC esddsp.c -o libesddsp.so -lesd -lm

   set LD_PRELOAD=/path/to/libesddsp.so
   launch x11amp, etc.

   For frequently used programs, you can use a wrapper script:

   --- cut here ---
   #!/bin/sh
   export LD_PRELOAD=/path/to/libesddsp.so
   exec /path/to/my/app.real $*
   --- cut here ---

   Just rename "app" to "app.real" and drop in the above script as "app"
 */

#include <string.h>
#include <sys/types.h>
#include <sys/soundcard.h>

#include <esd.h>

extern int __open (const char *pathname, int flags, mode_t mode);
extern int __ioctl (int fd, int request, ...);

extern ssize_t write (int fd, const void *buf, size_t count);
extern int fsync (int fd);

static int sndfd = -1;

static int settings, done;

int open (const char *pathname, int flags, mode_t mode)
{
  int ret = __open (pathname, flags, mode);

  if (ret == -1 && !strcmp (pathname, "/dev/dsp"))
    {
      settings = done = 0;

#ifdef DSP_DEBUG
      printf ("hijacking /dev/dsp open, and taking it to esd...\n");
#endif

      return (sndfd = esd_open_sound (NULL));
    }
  else
    return ret;
}

int ioctl (int fd, int request, void *argp)
{
  static esd_format_t fmt = ESD_STREAM | ESD_PLAY | ESD_MONO;
  static int speed;

  if (fd != sndfd)
    return __ioctl (fd, request, argp);
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
	  fmt |= (*arg & 0x30) ? ESD_BITS16 : ESD_BITS8; settings |= 1;
	  break;

	case SNDCTL_DSP_SPEED:
	  speed = *arg; settings |= 2;
	  break;

	case SNDCTL_DSP_STEREO:
	  fmt &= ~ESD_MONO, fmt |= (*arg) ? ESD_STEREO : ESD_MONO;
	  break;

	case SNDCTL_DSP_GETBLKSIZE:
	  *arg = ESD_BUF_SIZE;
	  break;

	case SNDCTL_DSP_GETFMTS:
	  *arg = 0x3f;
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
	  char buf[ESD_NAME_MAX] = "dsp";

	  done = 1;

          if (write (sndfd, &proto, sizeof (proto)) != sizeof (proto))
	    return -1;
          if (write (sndfd, &fmt, sizeof (fmt)) != sizeof (fmt))
	    return -1;
          if (write (sndfd, &speed, sizeof (speed)) != sizeof (speed))
	    return -1;
	  if (write (sndfd, buf, ESD_NAME_MAX) != ESD_NAME_MAX)
	    return -1;

	  fsync (sndfd);

  	  fmt = ESD_STREAM | ESD_PLAY | ESD_MONO;
	  speed = 0;
	}

      return 0;
    }

  return 0;
}