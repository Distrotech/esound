#ifndef ESD_H
#define ESD_H

/* sound includes */
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>

/* for math functions */
#include <math.h>

/* generic stuff */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*************************************/
/* what can we do to/with the EsounD */
enum esd_proto { 
    ESD_PROTO_INVALID,      /* hold the zero value */

    /* pseudo "security" functionality */
    ESD_PROTO_LOCK,	    /* disable "foreign" client connections */
    ESD_PROTO_UNLOCK,	    /* enable "foreign" client connections */

    /* stream functionality: play, record, monitor */
    ESD_PROTO_STREAM_PLAY,  /* play all following data as a stream */
    ESD_PROTO_STREAM_REC,   /* record data from card as a stream */
    ESD_PROTO_STREAM_MON,   /* send mixed buffer output as a stream */

    /* sample functionality: cache, free, play, loop, EOL, kill */
    ESD_PROTO_SAMPLE_CACHE, /* cache a sample in the server */
    ESD_PROTO_SAMPLE_FREE,  /* release a sample in the server */
    ESD_PROTO_SAMPLE_PLAY,  /* play a cached sample */
    ESD_PROTO_SAMPLE_LOOP,  /* loop a cached sample, til eoloop */
    ESD_PROTO_SAMPLE_STOP,  /* stop a looping sample when done */
    ESD_PROTO_SAMPLE_KILL,  /* stop the looping sample immed. */

    /* free and reclaim /dev/dsp functionality */
    ESD_PROTO_STANDBY,	    /* release /dev/dsp and ignore all data */
    ESD_PROTO_RESUME,	    /* reclaim /dev/dsp and play sounds again */

    /* move these to a more logical place when we're ready to break the protocol */
    ESD_PROTO_SAMPLE_GETID, /* get the ID for an already-cached sample */
    ESD_PROTO_STREAM_FILT,  /* filter mixed buffer output as a stream */

    ESD_PROTO_MAX           /* for bounds checking */
};

/************************/
/* ricdude's EsounD api */

/* the properties of a sound buffer are logically or'd */

/* bits of stream/sample data */
#define ESD_MASK_BITS	( 0x000F )
#define ESD_BITS8 	( 0x0000 )
#define ESD_BITS16	( 0x0001 )

/* how many interleaved channels of data */
#define ESD_MASK_CHAN	( 0x00F0 )
#define ESD_MONO	( 0x0010 )
#define ESD_STEREO	( 0x0020 )

/* whether it's a stream or a sample */
#define ESD_MASK_MODE	( 0x0F00 )
#define ESD_STREAM	( 0x0000 )
#define ESD_SAMPLE	( 0x0100 )
#define ESD_ADPCM	( 0x0200 )	/* TODO: anyone up for this? =P */

/* the function of the stream/sample, and common functions */
#define ESD_MASK_FUNC	( 0xF000 )
#define ESD_PLAY	( 0x1000 )
/* functions for streams only */
#define ESD_MONITOR	( 0x0000 )
#define ESD_RECORD	( 0x2000 )
/* functions for samples only */
#define ESD_STOP	( 0x0000 )
#define ESD_LOOP	( 0x2000 )

typedef int esd_format_t;
typedef int esd_proto_t;

/*******************************************************************/
/* client side API for playing sounds */

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned char octet;

/* esdlib.c - client interface functions */

/* opens channel, authenticates connection, and prefares for protos */
/* returns EsounD socket for communication, result < 0 = error */
/* server = listen socket (localhost:5001, 192.168.168.0:9999 */
/* rate, format = (bits | channels | stream | func) */
int esd_open_sound( char *host );

/* lock/unlock will disable/enable foreign clients from connecting */
int esd_lock( int esd );
int esd_unlock( int esd );

/* standby/resume will free/reclaim audio device so others may use it */
int esd_standby( int esd );
int esd_resume( int esd );

/* open a socket for playing, monitoring, or recording as a stream */
/* the *_fallback functions try to open /dev/dsp if there's no EsounD */
int esd_play_stream( esd_format_t format, int rate, char *host, char *name );
int esd_play_stream_fallback( esd_format_t format, int rate, char *host, char *name );
int esd_monitor_stream( esd_format_t format, int rate, char *host, char *name );
/* int esd_monitor_stream_fallback( esd_format_t format, int rate ); */
int esd_record_stream( esd_format_t format, int rate, char *host, char *name );
int esd_record_stream_fallback( esd_format_t format, int rate, char *host, char *name );
int esd_filter_stream( esd_format_t format, int rate, char *host, char *name );

/* cache a sample in the server returns sample id, < 0 = error */
int esd_sample_cache( int esd, esd_format_t format, int rate, int length, char *name );
int esd_confirm_sample_cache( int esd );

/* get the sample id for an already-cached sample */
int esd_sample_getid( int esd, const char *name);

/* uncache a sample in the server */
int esd_sample_free( int esd, int sample );

/* play a cached sample once */
int esd_sample_play( int esd, int sample );
/* make a cached sample loop */
int esd_sample_loop( int esd, int sample );

/* stop the looping sample at end */
int esd_sample_stop( int esd, int sample );
/* stop a playing sample immed. */
int esd_sample_kill( int esd, int sample );

/* closes fd, previously obtained by esd_open */
int esd_close( int esd );

/* audio.c - abstract the sound hardware for cross platform usage */
extern esd_format_t esd_audio_format;
extern int esd_audio_rate;

int esd_audio_open();
void esd_audio_close();
void esd_audio_pause();
int esd_audio_write( void *buffer, int buf_size );
int esd_audio_read( void *buffer, int buf_size );
void esd_audio_flush();

#ifdef __cplusplus
}
#endif

/* length of the audio buffer size */
#define ESD_BUF_SIZE (4 * 1024)

/* length of the authorization key, octets */
#define ESD_KEY_LEN (16)

/* default port for the EsounD server */
#define ESD_DEFAULT_PORT (5001)

/* default sample rate for the EsounD server */
#define ESD_DEFAULT_RATE (44100)

/* maximum length of a stream/sample name */
#define ESD_NAME_MAX (128)

/* a magic number to identify the relative endianness of a client */
#define ESD_ENDIAN_KEY \
	( ('E' << 24) + ('N' << 16) + ('D' << 8) + ('N') )

#endif /* #ifndef ESD_H */
