
#include "esd-server.h"

#include <arpa/inet.h>

/*******************************************************************/
/* globals */
int esd_on_standby = 0;	/* set to 1 to route all audio to /dev/null */
int esdbg_trace = 0;		/* show warm fuzzy debug messages */
int esdbg_comms = 0;		/* show protocol level debug messages */

int esd_buf_size_octets = 0; 	/* size of audio buffer in bytes */
int esd_buf_size_samples = 0; 	/* size of audio buffer in samples */
int esd_sample_size = 0;	/* size of sample in bytes */

int esd_beeps = 1;		/* whether or not to beep on startup */
int listen_socket = -1;		/* socket we're accepting connections on */

/*******************************************************************/
/* just to create the startup tones for the fun of it */
void set_audio_buffer( void *buf, esd_format_t format,
		       int magl, int magr, 
		       int freq, int speed, int length, long offset )
{
    int i;
    float sample;
    float kf = 2.0 * 3.14 * (float)freq / (float)speed;

    unsigned char *uc_buf = (unsigned char *)buf;
    signed short *ss_buf = (signed short *)buf;
    
    /* printf( "fmt=%d, ml=%d, mr=%d, freq=%d, speed=%d, len=%ld\n",
	    format, magl, magr, freq, speed, length ); */

    switch ( format & ESD_MASK_BITS )
    {
    case ESD_BITS8:
	for ( i = 0 ; i < length ; i+=2 ) {
	    sample = sin( (float)(i+offset) * kf );
	    uc_buf[i] = 127 + magl * sample;
	    uc_buf[i+1] = 127 + magr * sample;
	}
	break;
    case ESD_BITS16:	/* assume same endian */
	for ( i = 0 ; i < length ; i+=2 ) {
	    sample = sin( (float)(i+offset) * kf );
	    ss_buf[i] = magl * sample;
	    ss_buf[i+1] = magr * sample;
	}
	break;
    default:
	fprintf( stderr, 
		 "unsupported format for set_audio_buffer: 0x%08x\n", 
		 format );
	exit( 1 );
    }


    return;
}

/*******************************************************************/
/* to properly handle signals */
void clean_exit(int signum) {
    /* just clean up as best we can and terminate from here */
    esd_client_t * client = esd_clients_list;
    
    fprintf( stderr, "received signal %d: terminating...\n", signum );
    
    /* free the sound device */
    esd_audio_close();

    /* close the listening socket */
    close( listen_socket );

    /* close the clients */
    while ( client != NULL )
    {
	close( client->fd );
	client = client->next;
    }

    /* trust the os to clean up the memory for the samples and such */
    fprintf( stderr, "bye bye.\n" );
    exit( 0 );
}

void reset_signal(int signum) {
    fprintf( stderr, "received signal %d: resetting...\n", signum );
    signal( signum, reset_signal);

    return;
}

/*******************************************************************/
/* returns the listening socket descriptor */
int open_listen_socket( int port )
{
    /*********************/
    /* socket test setup */
    struct sockaddr_in socket_addr;
    int socket_listen;
    struct linger lin;
   
    /* create the socket, and set for non-blocking */
    socket_listen=socket(AF_INET,SOCK_STREAM,0);
    if (socket_listen<0) 
    {
	fprintf(stderr,"Unable to create socket\n");
	return( -1 );
    }
    if (fcntl(socket_listen,F_SETFL,O_NONBLOCK)<0)
    {
	fprintf(stderr,"Unable to set socket to non-blocking\n");
	return( -1 );
    }

    /* set socket for linger? */
    lin.l_onoff=1;	/* block a closing socket for 1 second */
    lin.l_linger=100;	/* if data is waiting to be sent */
    if ( setsockopt( socket_listen, SOL_SOCKET, SO_LINGER,
		     &lin, sizeof(struct linger) ) < 0 ) 
    {
	fprintf(stderr,"Unable to set socket linger value to %d\n",
		lin.l_linger);
	return( -1 );
    }

    /* set the listening information */
    socket_addr.sin_family = AF_INET;
    socket_addr.sin_port = htons( port );
    socket_addr.sin_addr.s_addr = htonl( inet_addr("0.0.0.0") );
    if ( bind( socket_listen,
	       (struct sockaddr *) &socket_addr,
	       sizeof(struct sockaddr_in) ) < 0 )
    {
	fprintf(stderr,"Unable to bind port %d\n", 
		socket_addr.sin_port );
	exit(1);
    }
    if (listen(socket_listen,16)<0)
    {
	fprintf(stderr,"Unable to set socket listen buffer length\n");
	exit(1);
    }

    return socket_listen;
}

/*******************************************************************/
int main ( int argc, char *argv[] )
{
    /***************************/
    /* Enlightened sound Daemon */

    int audio = -1;
    int esd_port = ESD_DEFAULT_PORT;
    int length = 0;
    int arg = 0;

    void *output_buffer = NULL;

    struct timespec restrain;
    int do_sleep = 0;

    /* begin test scaffolding parameters */
    /* int format = AFMT_U8; AFMT_S16_LE; */
    /* int stereo = 0; */     /* 0=mono, 1=stereo */
    int default_rate = ESD_DEFAULT_RATE, default_buf_size = ESD_BUF_SIZE;
    int i, j, freq=440;
    int magl, magr;

    int default_format = ESD_BITS16 | ESD_STEREO;
    /* end test scaffolding parameters */

    /* start the initializatin process */
    printf( "initializing...\n" );

    /* parse the command line args */
    for ( arg = 1 ; arg < argc ; arg++ ) {
	if ( !strcmp( argv[ arg ], "-port" ) ) {
	    if ( ++arg != argc ) {
		esd_port = atoi( argv[ arg ] );
		if ( !esd_port ) {
		    esd_port = ESD_DEFAULT_PORT;
		    fprintf( stderr, "- could not determine port: %s\n", 
			     argv[ arg ] );
		}
		fprintf( stderr, "- accepting connections on port %d\n", 
			 esd_port );
	    }
	} else if ( !strcmp( argv[ arg ], "-b" ) ) {
	    fprintf( stderr, "- server format: 8 bit samples\n" );
	    default_format &= ~ESD_MASK_BITS; default_format |= ESD_BITS8; 
	} else if ( !strcmp( argv[ arg ], "-r" ) ) {
	    if ( ++arg != argc ) {
		default_rate = atoi( argv[ arg ] );
		if ( !default_rate ) {
		    default_rate = ESD_DEFAULT_RATE;
		    fprintf( stderr, "- could not determine rate: %s\n", 
			     argv[ arg ] );
		}
		fprintf( stderr, "- server_format: sample rate = %d Hz\n", 
			 default_rate );
	    }
	} else if ( !strcmp( argv[ arg ], "-vt" ) ) {
	    esdbg_trace = 1;
	    fprintf( stderr, "- enabling trace diagnostic info\n" );
	} else if ( !strcmp( argv[ arg ], "-vc" ) ) {
	    esdbg_comms = 1;
	    fprintf( stderr, "- enabling comms diagnostic info\n" );
	} else if ( !strcmp( argv[ arg ], "-nobeeps" ) ) {
	    esd_beeps = 0;
	    fprintf( stderr, "- disabling startup beeps\n" );
	} else {
	    fprintf( stderr, "unrecognized option: %s\n", argv[ arg ] );
	}
    }

    /* set the data size parameters */
    esd_audio_format = default_format;
    esd_audio_rate = default_rate;

    esd_sample_size = ( (esd_audio_format & ESD_MASK_BITS) == ESD_BITS16 )
	? sizeof(signed short) : sizeof(unsigned char);
    esd_buf_size_samples = default_buf_size / 2;
    esd_buf_size_octets = esd_buf_size_samples * esd_sample_size;

    /* open and initialize the audio device, /dev/dsp */
    audio = esd_audio_open();
    if ( audio < 0 ) {
	fprintf( stderr, "fatal error configuring sound, %s\n", 
		 "/dev/dsp" );
	exit( 1 );	    
    }

    /* allocate and zero out buffer */
    output_buffer = (void *) malloc( esd_buf_size_octets );
    memset( output_buffer, 0, esd_buf_size_octets );

    /* open the listening socket */
    listen_socket = open_listen_socket( esd_port );
    if ( listen_socket < 0 ) {
	fprintf( stderr, "fatal error opening socket\n" );
	exit( 1 );	    
    }
    
    /* install signal handlers for program integrity */
    signal( SIGINT, clean_exit );	/* for ^C */
    signal( SIGTERM, clean_exit );	/* for default kill */
    signal( SIGPIPE, reset_signal );	/* for closed rec/mon clients */
    signal( SIGHUP, clear_auth );	/* kill -HUP clear ownership */

    /* send some sine waves just to check the sound connection */
    i = 0;
    if ( esd_beeps ) {
	magl = magr = ( (esd_audio_format & ESD_MASK_BITS) == ESD_BITS16) 
	    ? 30000 : 100;

	for ( freq = 55 ; freq < esd_audio_rate/2 ; freq *= 2, i++ ) {
	    /* repeat the freq for a few buffer lengths */
	    for ( j = 0 ; j < esd_audio_rate / 4 / esd_buf_size_samples ; j++ ) {
		set_audio_buffer( output_buffer, esd_audio_format, 
				  ( (i%2) ? magl : 0 ),  ( (i%2) ? 0 : magr ),
				  freq, esd_audio_rate, esd_buf_size_samples, 
				  j * esd_buf_size_samples );
		esd_audio_write( output_buffer, esd_buf_size_octets );
	    }
	}
    }

    /* pause the sound output */
    esd_audio_pause();

    /* until we kill the daemon */
    while ( 1 )
    {
	/* block while waiting for more clients and new data */
	wait_for_clients_and_data( listen_socket );

	/* accept new connections */
	get_new_clients( listen_socket );

	/* check for new protocol requests */
	poll_client_requests();

	/* mix new requests, and output to device */
	length = mix_players_16s( output_buffer, esd_buf_size_octets );
	if ( length > 0 /* || esd_monitor */ ) {
	    do_sleep = 0;
	    if ( !esd_on_standby ) {
		/* standby check goes in here, so esd will eat sound data */
		/* TODO: eat a round of data with a better algorithm */
		/*        this will cause guaranteed timing issues */
		/* TODO: on monitor, why isn't this a buffer of zeroes? */
		/* esd_audio_write( output_buffer, esd_buf_size_octets ); */
		esd_audio_write( output_buffer, length );
		esd_audio_flush();
	    }
	} else {
	    /* be very quiet, and wait for a wabbit to come along */
	    do_sleep = 1;
	    esd_audio_pause();
	}

	/* if someone's monitoring the sound stream, send them data */
	/* mix_players, above, forces buffer to zero if no players */
	/* this clears out any leftovers from recording, below */
	if ( esd_monitor && !esd_on_standby ) {
	    /* TODO: maybe the last parameter here should be length? */
		if ( (esd_audio_format & ESD_MASK_BITS) == ESD_BITS16 ) 
		    length = mix_from_stereo_16s( output_buffer, 
						  esd_monitor, length );
		else
		    length = mix_from_stereo_8u( output_buffer, 
						 esd_monitor, length );

	    if( length )
		monitor_write();

	}

	/* if someone's recording the sound stream, send them data */
	if ( esd_recorder && !esd_on_standby ) { 
	    length = esd_audio_read( output_buffer, esd_buf_size_octets );
	    if ( length ) {
		if ( (esd_audio_format & ESD_MASK_BITS) == ESD_BITS16 ) 
		    mix_from_stereo_16s( output_buffer, 
					 esd_recorder, esd_buf_size_octets ); 
		else
		    mix_from_stereo_8u( output_buffer, 
					esd_recorder, esd_buf_size_octets ); 
		recorder_write(); 
	    }
	}

	if ( esd_on_standby  || do_sleep ) {
	    restrain.tv_sec = 0;
	    /* funky math to make sure a long can hold it all, calulate in ms */
	    restrain.tv_nsec = (long) esd_buf_size_samples * 1000L
		/ (long) esd_audio_rate / 4L; 	/* divide by two for stereo */
	    restrain.tv_nsec *= 1000000L; 	/* convert to nanoseconds */
	    nanosleep( &restrain, NULL );
	}

    } /* while ( 1 ) */

    /* how we'd get here, i have no idea */
    esd_audio_close();
    close( listen_socket );

    exit( 0 );
}
