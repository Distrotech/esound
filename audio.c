#include "esd.h"

/*******************************************************************/
/* globals */
esd_format_t esd_audio_format = ESD_BITS16 | ESD_STEREO;
int esd_audio_rate = 44100;

/* the audio device, /dev/dsp, file descriptor */
static int esd_audio_fd = -1;

/*******************************************************************/
/* returns audio_fd for use by main prog - platform dependent */

#if defined(DRIVER_OSS)
#  include "audio_oss.c"
#elif defined(DRIVER_AIX)
#  include "audio_aix.c"
#elif defined(DRIVER_IRIX)
#  include "audio_irix.c"
#elif defined(DRIVER_HPUX)
#  include "audio_hpux.c"
#elif defined(DRIVER_SOLARIS)
#  include "audio_solaris.c"
#else
#  define DRIVER_NONE
#  include "audio_none.c"
#endif

/*******************************************************************/
/* close the audio device */
#ifndef DRIVER_IRIX
void esd_audio_close()
{
    close( esd_audio_fd );
    return;
}
#endif
/*******************************************************************/
/* make the sound device quiet for a while */
void esd_audio_pause()
{
    /* per oss specs */
#ifdef DRIVER_OSS	
    ioctl( esd_audio_fd, SNDCTL_DSP_POST, 0 );
#endif    
    return;
}

#ifndef DRIVER_IRIX
/*******************************************************************/
/* dump a buffer to the sound device */
int esd_audio_write( void *buffer, int buf_size )
{
    return write( esd_audio_fd, buffer, buf_size );
}

/*******************************************************************/
/* read a chunk from the sound device */
int esd_audio_read( void *buffer, int buf_size )
{
    return read( esd_audio_fd, buffer, buf_size );
}

/*******************************************************************/
/* flush the audio buffer */
void esd_audio_flush()
{
    fsync( esd_audio_fd );
    return;
}
#endif
