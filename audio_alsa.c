/* Advanced Linux Sounds Architecture support for EsounD
   7-19-98: Nick Lopez( kimo_sabe@usa.net ) - it starts!
*/

#if defined(DRIVER_NEWALSA)
#  include <sys/asoundlib.h>
#else
#  include <sys/soundlib.h>
#endif

void *handle;

/* so that EsounD can use other cards besides the first */
#ifndef ALSACARD
#  define ALSACARD 0
#endif
#ifndef ALSADEVICE
#  define ALSADEVICE 0
#endif

#define ARCH_esd_audio_open
int esd_audio_open()
{
  snd_pcm_format_t format;
  snd_pcm_playback_params_t params;
  int ret, mode = SND_PCM_OPEN_PLAYBACK;
  char buf[256];
  
    /* if recording, set for full duplex mode */
    if ( (esd_audio_format & ESD_MASK_FUNC) == ESD_RECORD )
        mode = SND_PCM_OPEN_DUPLEX;
  
  if ( ret = snd_pcm_open( &handle, ALSACARD, ALSADEVICE, mode ) < 0) {
    perror( "snd_pcm_open" );
    fprintf( stderr, "open failed: %s\n", snd_strerror( ret ) );
    esd_audio_close();
    esd_audio_fd = -1;
    return ( -1 );
  }

    /* set the sound driver audio format for playback */
    format.format = ( (esd_audio_format & ESD_MASK_BITS) == ESD_BITS16 )
        ? /* 16 bit */ SND_PCM_SFMT_S16_LE : /* 8 bit */ SND_PCM_SFMT_U8;
    format.rate = esd_audio_rate;
    format.channels = ( ( esd_audio_format & ESD_MASK_CHAN) == ESD_STEREO ) 
                        ? 2 : 1;
    if ( ( ret = snd_pcm_playback_format( handle, &format ) ) < 0 ) {
      fprintf( stderr, "set format failed: %s\n", snd_strerror( ret ) );
      esd_audio_close();
      esd_audio_fd = -1;
      return ( -1 );
    }
    
    params.fragment_size = 4*1024;
    params.fragments_max = 2;
    params.fragments_room = 1;
    ret = snd_pcm_playback_params( handle, &params );
    if ( ret ) {
      printf( "error: %s: in snd_pcm_playback_params\n", snd_strerror(ret) );
    }
    
    ret = snd_pcm_block_mode( handle, 1 );
    if ( ret )
      printf( "error: %s: in snd_pcm_block_mode\n", snd_strerror(ret));

   if ( format.rate != esd_audio_rate || format.channels != 2 || format.format != SND_PCM_SFMT_S16_LE )
     printf("set format didn't work.");

  return ( esd_audio_fd = snd_pcm_file_descriptor( handle ) );  /* no descriptor for ALSAlib */
}

#define ARCH_esd_audio_close
void esd_audio_close()
{
  snd_pcm_close( handle );
}

#define ARCH_esd_audio_pause
void esd_audio_pause()
{
  snd_pcm_drain_playback( handle );
}

#define ARCH_esd_audio_read
int esd_audio_read( void *buffer, int buf_size )
{
  return (snd_pcm_read( handle, buffer, buf_size ));
}

int writes;
#define ARCH_esd_audio_write
int esd_audio_write( void *buffer, int buf_size )
{
  int i=0;
  i = snd_pcm_write( handle, buffer, buf_size);
  writes += i;
  return (i);
}

#define ARCH_esd_audio_flush
void esd_audio_flush()
{
  fsync( esd_audio_fd );
  /*snd_pcm_flush_playback( handle );*/
}
