
#include "esd-server.h"
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>

#include <arpa/inet.h>

/*******************************************************************/
/* send the authorization cookie, create one if needed */
int esd_send_auth( int socket )
{
    int auth_fd = -1, i = 0;
    char auth_filename[NAME_MAX+1] = "", auth_key[ESD_KEY_LEN];
    char *home = NULL;
    char tumbler = '\0';

    /* assemble the authorization filename */
    home = getenv( "HOME" );
    if ( !home ) {
	printf( "HOME environment variable not set?\n" );
	return -1;
    }
    strcpy( auth_filename, home );
    strcat( auth_filename, "/.esd_auth" );

    /* open the authorization file */
    if ( -1 == (auth_fd = open( auth_filename, O_RDONLY ) ) )
    {
	/* it doesn't exist? create one */
	auth_fd = open( auth_filename, O_RDWR | O_CREAT | O_EXCL,
			S_IRUSR | S_IWUSR );

	if ( -1 == auth_fd ) {
	    /* coun't even create it?  bail */
	    perror( auth_filename );
	    return 0;
	}

	/* spew random garbage for a key */
	srand( time(NULL) );
	for ( i = 0 ; i < ESD_KEY_LEN ; i++ ) {
	    tumbler = rand() % 256;
	    write( auth_fd, &tumbler, 1 );
	}

	/* rewind the file to the beginning, and proceed */
	lseek( auth_fd, 0, SEEK_SET );
    }

    /* read the key from the authorization file */
    if ( ESD_KEY_LEN != read( auth_fd, auth_key, ESD_KEY_LEN ) ) {
	close( auth_fd );
	return 0;
    }
	
    /* send the key to the server */
    if ( ESD_KEY_LEN != write( socket, auth_key, ESD_KEY_LEN ) ) {
	/* send key failed */
	return 0;
    }

    /* we've run the gauntlet, everything's ok, proceed as usual */
    return 1;
}

/*******************************************************************/
/* initialize the socket to send data to the sound daemon */
int esd_open_sound()
{
    char *espeaker = NULL;

    /*********************/
    /* socket test setup */
    struct sockaddr_in socket_addr;
    int socket_out;
    int curstate = 1;

    char host[64] = "0.0.0.0";	/* TODO: is there a MAX_??? for this? */
    int port = ESD_DEFAULT_PORT;
    int host_div = 0;
   
    /* see if we have a remote speaker to play to */
    espeaker = getenv( "ESPEAKER" );
    if ( espeaker != NULL ) {
	/* split the espeaker host into host:port */
	host_div = strcspn( espeaker, ":" );

	/* get host */
	if ( host_div ) {
	    strncpy( host, espeaker, host_div );
	    host[ host_div ] = '\0';
	}

	/* TODO: gethostbyname(host) */
	
	/* get port */
	if ( host_div != strlen( espeaker ) )
	    port = atoi( espeaker + host_div + 1 );
	if ( !port ) 
	    port = ESD_DEFAULT_PORT;
	printf( "(remote) host is %s : %d\n", host, port );
    }

    /* create the socket, and set for non-blocking */
    socket_out = socket( AF_INET, SOCK_STREAM, 0 );
    if ( socket_out < 0 ) 
    {
	fprintf(stderr,"Unable to create socket\n");
	return( -1 );
    }

    /* this was borrowed blindly from the Tcl socket stuff */
    if ( fcntl( socket_out, F_SETFD, FD_CLOEXEC ) < 0 )
    {
	fprintf(stderr,"Unable to set socket to non-blocking\n");
	return( -1 );
    }
    if ( setsockopt( socket_out, SOL_SOCKET, SO_REUSEADDR,
		     &curstate, sizeof(curstate) ) < 0 ) 
    {
	fprintf(stderr,"Unable to set for a fresh socket\n");
	return( -1 );
    }

    /* set the connect information */
    socket_addr.sin_family = AF_INET;
    socket_addr.sin_port = htons( port );

    if( !inet_aton( host, &socket_addr.sin_addr ) )
    { 
	printf( "couldn't convert %s to inet address\n", host );
	return -1;
    }

    if ( connect( socket_out,
		  (struct sockaddr *) &socket_addr,
		  sizeof(struct sockaddr_in) ) < 0 )
    {
	fprintf(stderr,"Unable to connect to server port %d\n", 
		socket_addr.sin_port );
	return -1;
    }

    /* send authorization */
    if ( !esd_send_auth( socket_out ) ) {
	/* couldn't send authorization key, bail */
	close( socket_out );
	return -1;
    }

    return socket_out;
}

/*******************************************************************/
/* open a socket for playing as a stream */
int esd_play_stream( esd_format_t format, int rate )
{
    int sock;
    int proto = ESD_PROTO_STREAM_PLAY;

    sock = esd_open_sound( format, rate );
    if ( sock < 0 ) 
	return sock;
    
    /* send the audio format information */
    if ( write( sock, &proto, sizeof(proto) ) != sizeof(proto) )
	return -1;
    if ( write( sock, &format, sizeof(format) ) != sizeof(format) )
	return -1;
    if( write( sock, &rate, sizeof(rate) ) != sizeof(rate) )
	return -1;
    fsync( sock );
    
    return sock;
}

/*******************************************************************/
/* open a socket for playing as a stream, fallback to /dev/dsp */
int esd_play_stream_fallback( esd_format_t format, int rate )
{
    int socket_out;

    /* try to open a connection to the server */
    socket_out = esd_play_stream( format, rate );
    if ( socket_out >= 0 ) 
	return socket_out;

    /* if ESPEAKER is set, this is an error, bail out */
    if ( getenv( "ESPEAKER" ) )
	return -1;

    /* go for /dev/dsp */
    esd_audio_format = format;
    esd_audio_rate = rate;
    socket_out = audio_open();

    /* we either got it, or we didn't */
    return socket_out;
}

/*******************************************************************/
/* open a socket for monitoring as a stream */
int esd_monitor_stream( esd_format_t format, int rate )
{
    int sock;
    int proto = ESD_PROTO_STREAM_MON;

    sock = esd_open_sound( format, rate );
    if ( sock < 0 ) 
	return sock;
    
    /* send the audio format information */
    if ( write( sock, &proto, sizeof(proto) ) != sizeof(proto) )
	return -1;
    if ( write( sock, &format, sizeof(format) ) != sizeof(format) )
	return -1;
    if( write( sock, &rate, sizeof(rate) ) != sizeof(rate) )
	return -1;
    fsync( sock );
    
    return sock;
}

/*******************************************************************/
/* open a socket for recording as a stream */
int esd_record_stream( esd_format_t format, int rate )
{
    int sock;
    int proto = ESD_PROTO_STREAM_REC;

    sock = esd_open_sound( format, rate );
    if ( sock < 0 ) 
	return sock;
    
    /* send the audio format information */
    if ( write( sock, &proto, sizeof(proto) ) != sizeof(proto) )
	return -1;
    if ( write( sock, &format, sizeof(format) ) != sizeof(format) )
	return -1;
    if( write( sock, &rate, sizeof(rate) ) != sizeof(rate) )
	return -1;
    fsync( sock );
    
    return sock;
}

/*******************************************************************/
/* open a socket for recording as a stream, fallback to /dev/dsp */
int esd_record_stream_fallback( esd_format_t format, int rate )
{
    int socket_out;

    /* try to open a connection to the server */
    socket_out = esd_record_stream( format, rate );
    if ( socket_out >= 0 ) 
	return socket_out;

    /* if ESPEAKER is set, this is an error, bail out */
    if ( getenv( "ESPEAKER" ) )
	return -1;

    /* go for /dev/dsp */
    esd_audio_format = format;
    esd_audio_rate = rate;
    socket_out = audio_open();

    /* we either got it, or we didn't */
    return socket_out;
}

/*******************************************************************/
/* cache a sample in the server returns sample id, <= 0 is error */
int esd_sample_cache( int esd, esd_format_t format, int rate, long size )
{
    int id = 0;
    int proto = ESD_PROTO_SAMPLE_CACHE;

    printf( "caching sample (%d) - %ld bytes\n", esd, size );

    /* send the necessary information */
    if ( write( esd, &proto, sizeof(proto) ) != sizeof(proto) )
	return -1;
    if ( write( esd, &format, sizeof(format) ) != sizeof(format) )
	return -1;
    if ( write( esd, &rate, sizeof(rate) ) != sizeof(rate) )
	return -1;
    if ( write( esd, &size, sizeof(size) ) != sizeof(size) )
	return -1;

    fsync( esd );

    /* get the sample id back from the server */
    if ( read( esd, &id, sizeof(id) ) != sizeof(id) )
	return -1;

    /* return the sample id to the client */
    return id;
}

/*******************************************************************/
/* uncache a sample in the server */
int esd_sample_free( int esd, int sample )
{
    int id;
    int proto = ESD_PROTO_SAMPLE_FREE;

    printf( "freeing sample (%d) - <%d>\n", esd, sample );

    /* send the necessary information */
    if ( write( esd, &proto, sizeof(proto) ) != sizeof(proto) )
	return -1;
    if ( write( esd, &sample, sizeof(sample) ) != sizeof(sample) )
	return -1;
    fsync( esd );

    /* get the sample id back from the server */
    if ( read( esd, &id, sizeof(id) ) != sizeof(id) )
	return -1;

    /* return the id to the client (0 = error, 1 = ok) */
    return id;
}

/*******************************************************************/
/* uncache a sample in the server */
int esd_sample_play( int esd, int sample )
{
    int is_ok;
    int proto = ESD_PROTO_SAMPLE_PLAY;

    printf( "playing sample (%d) - <%d>\n", esd, sample );

    /* send the necessary information */
    if ( write( esd, &proto, sizeof(proto) ) != sizeof(proto) )
	return -1;
    if ( write( esd, &sample, sizeof(sample) ) != sizeof(sample) )
	return -1;
    fsync( esd );

    /* get the sample id back from the server */
    if ( read( esd, &is_ok, sizeof(is_ok) ) != sizeof(is_ok) )
	return -1;

    /* return the id to the client (0 = error, 1 = ok) */
    return is_ok;
}


/*******************************************************************/
/* loop a previously cached sample in the server */
int esd_sample_loop( int esd, int sample )
{
    int is_ok;
    int proto = ESD_PROTO_SAMPLE_LOOP;

    printf( "playing sample (%d) - <%d>\n", esd, sample );

    /* send the necessary information */
    if ( write( esd, &proto, sizeof(proto) ) != sizeof(proto) )
	return -1;
    if ( write( esd, &sample, sizeof(sample) ) != sizeof(sample) )
	return -1;
    fsync( esd );

    /* get the sample id back from the server */
    if ( read( esd, &is_ok, sizeof(is_ok) ) != sizeof(is_ok) )
	return -1;

    /* return the id to the client (0 = error, 1 = ok) */
    return is_ok;
}

/*******************************************************************/
/* stop a looping sample in the server */
int esd_sample_stop( int esd, int sample )
{
    int is_ok;
    int proto = ESD_PROTO_SAMPLE_STOP;

    printf( "stopping sample (%d) - <%d>\n", esd, sample );

    /* send the necessary information */
    if ( write( esd, &proto, sizeof(proto) ) != sizeof(proto) )
	return -1;
    if ( write( esd, &sample, sizeof(sample) ) != sizeof(sample) )
	return -1;
    fsync( esd );

    /* get the sample id back from the server */
    if ( read( esd, &is_ok, sizeof(is_ok) ) != sizeof(is_ok) )
	return -1;

    /* return the id to the client (0 = error, 1 = ok) */
    return is_ok;
}
