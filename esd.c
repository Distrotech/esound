
#include "esd-server.h"

#include <arpa/inet.h>

/*******************************************************************/
/* globals */
int esd_on_standby = 0;	/* set to 1 to route all audio to /dev/null */
int esdbg_trace = 0;		/* show warm fuzzy debug messages */
int esdbg_comms = 0;		/* show protocol level debuig messages */

volatile int esd_terminate = 0;	/* signals set this for a clean exit */
int esd_beeps = 1;		/* whether or not to beep on startup */

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
	    uc_buf[i] = 127 + magl * sin( (float)(i+offset) * kf );
	    uc_buf[i+1] = 127 + magr * sin( (float)(i+offset) * kf );
	}
	break;
    case ESD_BITS16:	/* assume same endian */
	for ( i = 0 ; i < length/sizeof(short) ; i+=2 ) {
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
    fprintf( stderr, "received signal %d: terminating...\n", signum );
    esd_terminate = 1;
    return;
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
    int listen_socket = -1;
    int esd_port = ESD_DEFAULT_PORT;
    int length = 0;
    int arg = 0;

    void *output_buffer = NULL; 

    /* begin test scaffolding parameters */
    /* int format = AFMT_U8; AFMT_S16_LE; */
    /* int stereo = 0; */     /* 0=mono, 1=stereo */
    int rate = 44100, buf_size = ESD_BUF_SIZE;
    int i, j, freq=440;
    int magl, magr;

    int format = ESD_BITS16 | ESD_STEREO;
    magl = magr = ( (format & ESD_MASK_BITS) == ESD_BITS16) 
	? 30000 : 100;
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

    /* open and initialize the audio device, /dev/dsp */
    esd_audio_format = format;
    esd_audio_rate = rate;

    audio = esd_audio_open();
    if ( audio < 0 ) {
	fprintf( stderr, "fatal error configuring sound, %s\n", 
		 "/dev/dsp" );
	exit( 1 );	    
    }

    /* allocate and zero out buffer */
    output_buffer = (void *) malloc( buf_size );
    memset( output_buffer, 0, buf_size);

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
	for ( freq = 55 ; freq < rate/2 ; freq *= 2, i++ ) {
	    /* repeat the freq for a few buffer lengths */
	    for ( j = 0 ; j < rate / 2 / buf_size ; j++ ) {
		set_audio_buffer( output_buffer, format, 
				  ( (i%2) ? magl : 0 ), 
				  ( (i%2) ? 0 : magr ),
				  freq, rate, buf_size, 
				  j * buf_size / sizeof(signed short) );
		esd_audio_write( output_buffer, buf_size );
	    }
	}
    }

    /* pause the sound output */
    esd_audio_pause();

    /* until we kill the daemon */
    while ( !esd_terminate )
    {
	/* block while waiting for more clients and new data */
	wait_for_clients_and_data( listen_socket );

	/* accept new connections */
	get_new_clients( listen_socket );

	/* check for new protocol requests */
	poll_client_requests();

	/* mix new requests, and output to device */
	length = mix_players_16s( output_buffer, buf_size );
	if ( length > 0 /* || esd_monitor */ ) {
	    if ( !esd_on_standby ) {
		/* standby check goes in here, so esd will eat sound data */
		/* TODO: eat a round of data with a better algorithm */
		/*        this will cause guaranteed timing issues */
		esd_audio_write( output_buffer, buf_size );
		esd_audio_flush();
	    }
	} else {
	    /* be very quiet, and wait for a wabbit to come along */
	    esd_audio_pause();
	}

	/* if someone's monitoring the sound stream, send them data */
	/* mix_players, above, forces buffer to zero if no players */
	/* this clears out any leftovers from recording, below */
	if ( esd_monitor && !esd_on_standby ) {
	    /* TODO: maybe the last parameter here should be length? */
	    length = mix_from_stereo_16s( output_buffer, 
					  esd_monitor, buf_size );
	    if( length )
		monitor_write();
	}

	/* if someone's recording the sound stream, send them data */
	if ( esd_recorder && !esd_on_standby ) { 
	    length = esd_audio_read( output_buffer, buf_size );
	    if ( length ) {
		mix_from_stereo_16s( output_buffer, 
				     esd_recorder, buf_size ); 
		recorder_write(); 
	    }
	}

	/* TODO: if ( esd_on_standby ) [nano]sleep( buf_size/sample_rate seconds ); */

    }

    esd_audio_close();
    close( listen_socket );

    exit( 0 );
}
