#ifndef ESD_H
#define ESD_H

/* sound includes */
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>

/* socket stuff */
#include <sys/socket.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/types.h>

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
    ESD_PROTO_SAMPLE_STOP,   /* stop a looping sample when done */
    ESD_PROTO_SAMPLE_KILL,  /* stop the looping sample immed. */

    ESD_PROTO_MAX           /* for bounds checking */
};

/************************/
/* ricdude's EsounD api */

/* the properties of a sound buffer are logically or'd */

/* bits of stream/sample data */
#define ESD_MASK_BITS	( 0x0003 )
#define ESD_BITS8 	( 0x0000 )
#define ESD_BITS16	( 0x0001 )

/* endianness of data */
#define ESD_MASK_END	( 0x000C )
#define ESD_ENDSM	( 0x0000 )
#define ESD_ENDBIG	( 0x0008 )
/* TODO: maybe ESD_ENDSAME/DIFF ? only the server needs this info */

/* how many interleaved channels of data */
#define ESD_MASK_CHAN	( 0x00F0 )
#define ESD_MONO	( 0x0010 )
#define ESD_STEREO	( 0x0020 )

/* whether it's a stream or a sample */
#define ESD_MASK_MODE	( 0x0F00 )
#define ESD_STREAM	( 0x0000 )
#define ESD_SAMPLE	( 0x0100 )

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
/* rate, format = (bits | endian | channels | stream | func) */
int esd_open_sound();

/* TODO: only used by esdlock, esdunlock - hide it in there */
/* sends authorization sequence, creating ~/.esd_auth if needed */
int esd_send_auth( int esd );

/* open a socket for playing, monitoring, or recording as a stream */
/* the *_fallback functions try to open /dev/dsp if there's no EsounD */
int esd_play_stream( esd_format_t format, int rate );
int esd_play_stream_fallback( esd_format_t format, int rate );
int esd_monitor_stream( esd_format_t format, int rate );
/* int esd_monitor_stream_fallback( esd_format_t format, int rate ); */
int esd_record_stream( esd_format_t format, int rate );
int esd_record_stream_fallback( esd_format_t format, int rate );

/* cache a sample in the server returns sample id, < 0 = error */
int esd_sample_cache( int esd, esd_format_t format, int rate, long length );
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

/*******************************************************************/
/* sound daemon data structures */

typedef struct esd_client {
    struct esd_client *next; 	/* it's a list, eh? link 'em */

    esd_proto_t request;	/* current request for this client */
    int fd;			/* the clients protocol stream */
    struct sockaddr_in source;	/* data maintained about source */

} esd_client_t;

typedef struct esd_player {
    struct esd_player *next;	/* point to next player in list */
    void *parent;		/* the client or sample that spawned player */

    esd_format_t format;	/* magic int with the format info */
    int rate;			/* sample rate */

    int source_id;		/* either a stream fd or sample id */
    octet *data_buffer;		/* buffer to hold sound data */
    int buffer_length;		/* total buffer length */
    int actual_length;		/* actual length of data in buffer */

    /* time_t last_read; */	/* timeout for streams, not used */
    int last_pos;		/* track read position for samples */

} esd_player_t;

/* TODO?: typedef esd_player_t esd_recorder_t, and monitor? */

typedef struct esd_sample {
    struct esd_sample *next;	/* point to next sample in list */
    struct esd_client *parent;	/* the client that spawned sample */

    esd_format_t format;	/* magic int with the format info */
    int rate;			/* sample rate */

    int sample_id;		/* the sample's id number */
    octet *data_buffer;		/* buffer to hold sound data */
    int sample_length;		/* total buffer length */

    int ref_count;		/* track players for clean deletion */
    int erase_when_done;	/* track uncache requests */
} esd_sample_t;

/* length of the audio buffer size */
#define ESD_BUF_SIZE (4 * 1024)

/* length of the authorization key, octets */
#define ESD_KEY_LEN (16)

/* default port for the EsounD server */
#define ESD_DEFAULT_PORT (5001)

#endif /* #ifndef ESD_H */
