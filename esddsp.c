/* Evil evil evil hack to get x11amp to cooperate with esd
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
   gcc -O2 -pipe -c -fPIC esddsp.c 
   gcc -shared -Wl,-soname -Wl,libesddsp.so.0 -o libesddsp.so.0.0.0 esddsp.o -lesd -lm -lc

   then set LD_PRELOAD=/path/to/libesddsp.so.0.0.0 before launching x11amp, etc.
 */

#include <string.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/soundcard.h>

#include <esd.h>

extern int __open (const char *pathname, int flags, mode_t mode);
extern int __ioctl (int fd, int request, ...);

extern ssize_t write (int fd, const void *buf, size_t count);
extern int fsync (int fd);

static int sndfd;

int open (const char *pathname, int flags, mode_t mode)
{
    if (!strcmp (pathname, "/dev/dsp")) {
	/* printf( "hijacking /dev/dsp, and taking it to esd...\n" ); */
	return (sndfd = esd_open_sound (NULL));
    }
    else
	return __open (pathname, flags, mode);
}

int ioctl (int fd, int request, ...)
{
  va_list args;
  void *argp;
  static esd_format_t fmt = ESD_STREAM | ESD_PLAY;
  static int speed;
  int proto = ESD_PROTO_STREAM_PLAY;
  char buf[ESD_NAME_MAX];

  va_start (args, request);
  argp = va_arg (args, void *);
  va_end (args);

  if (fd != sndfd)
    return __ioctl (fd, request, argp);
  else if (sndfd != -1)
    {
      int *arg = (int *) argp;

      /* printf( "hijacking /dev/dsp ioctl, and sending it to esd (%d:%d - %p)\n", 
	      fd, request, argp ); */
      switch (request)
	{
        case SNDCTL_DSP_SETFMT:
	  fmt |= (*arg == 16) ? ESD_BITS16 : ESD_BITS8;
	  return 0;

	case SNDCTL_DSP_STEREO:
	  fmt |= (*arg) ? ESD_STEREO : ESD_MONO;
	  return 0;

	case SNDCTL_DSP_SPEED:
	  speed = *arg;
	  return 0;

	case SNDCTL_DSP_GETBLKSIZE:
	  *arg = ESD_BUF_SIZE;

	  strncpy (buf, "amp", ESD_NAME_MAX);

          if (write (sndfd, &proto, sizeof (proto)) != sizeof (proto))
	    return -1;
          if (write (sndfd, &fmt, sizeof (fmt)) != sizeof (fmt))
	    return -1;
          if (write (sndfd, &speed, sizeof (speed)) != sizeof (speed))
	    return -1;
	  if (write (sndfd, buf, ESD_NAME_MAX) != ESD_NAME_MAX)
	    return -1;

	  fsync (sndfd);
	  return 0;

	default:
	  return 0;
	}
    }

  return 0;
}
