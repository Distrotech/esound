
#include "esd-server.h"
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>

#include <netdb.h>
#include <arpa/inet.h>

/*******************************************************************/
/* send the authorization cookie, create one if needed */
int esd_send_auth( int sock )
{
    int auth_fd = -1, i = 0;
    int endian = ESD_ENDIAN_KEY;
    char *auth_filename = 0, auth_key[ESD_KEY_LEN];
    char *home = NULL;
    char tumbler = '\0';
    int namelen, retval;

    /* assemble the authorization filename */
    home = getenv( "HOME" );
    if ( !home ) {
	fprintf( stderr, "HOME environment variable not set?\n" );
	return -1;
    }

    namelen = strlen(home) + sizeof("/.esd_auth");
    if ((auth_filename = malloc(namelen + 1)) == 0) {
	fprintf( stderr, "Memory exhausted\n" );
	return -1;
    }

    strcpy( auth_filename, home );
    strcat( auth_filename, "/.esd_auth" );

    retval = 0;
    /* open the authorization file */
    if ( -1 == (auth_fd = open( auth_filename, O_RDONLY ) ) ) {
	/* it doesn't exist? create one */
	auth_fd = open( auth_filename, O_RDWR | O_CREAT | O_EXCL,
			S_IRUSR | S_IWUSR );

	if ( -1 == auth_fd ) {
	    /* coun't even create it?  bail */
	    perror( auth_filename );
	    goto exit_fn;
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
    if ( ESD_KEY_LEN != read( auth_fd, auth_key, ESD_KEY_LEN ) )
	goto exit_fd;

    /* send the key to the server */
    if ( ESD_KEY_LEN != write( sock, auth_key, ESD_KEY_LEN ) )
	/* send key failed */
	goto exit_fd;

    /* send the key to the server */
    if ( sizeof(endian) != write( sock, &endian, sizeof(endian) ) )
	/* send key failed */
	goto exit_fd;

    /* we've run the gauntlet, everything's ok, proceed as usual */
    fsync( sock );
    retval = 1;

 exit_fd:
    close( auth_fd );
 exit_fn:
    free( auth_filename );
    return retval;
}

/*******************************************************************/
/* lock/unlock will disable/enable foreign clients from connecting */
int esd_lock( int esd ) {
    int proto = ESD_PROTO_LOCK;
    int ok = 0;

    write( esd, &proto, sizeof(proto) );
    esd_send_auth( esd );

    if ( read( esd, &ok, sizeof(ok) ) != sizeof(ok) )
	return -1;

    return ok;
}

int esd_unlock( int esd ){
    int proto = ESD_PROTO_UNLOCK;
    int ok = 0;

    write( esd, &proto, sizeof(proto) );
    esd_send_auth( esd );

    if ( read( esd, &ok, sizeof(ok) ) != sizeof(ok) )
	return -1;

    return ok;
}

/*******************************************************************/
/* standby/resume will free/reclaim audio device so others may use it */
int esd_standby( int esd )
{
    int proto = ESD_PROTO_STANDBY;
    int ok = 0;

    write( esd, &proto, sizeof(proto) );
    esd_send_auth( esd );

    if ( read( esd, &ok, sizeof(ok) ) != sizeof(ok) )
	return -1;

    return ok;
}

int esd_resume( int esd )
{
    int proto = ESD_PROTO_RESUME;
    int ok = 0;

    write( esd, &proto, sizeof(proto) );
    esd_send_auth( esd );

    if ( read( esd, &ok, sizeof(ok) ) != sizeof(ok) )
	return -1;

    return ok;
}

/*******************************************************************/
/* initialize the socket to send data to the sound daemon */
int esd_open_sound( char *host )
{
    char *espeaker = NULL;
    struct hostent *he;

    /*********************/
    /* socket test setup */
    struct sockaddr_in socket_addr;
    int socket_out;
    int curstate = 1;

    /* TODO: find out why I don't have INET_ADDRSTRLEN in
       my copy of /usr/include/netinet/in.h (instead of 64)
    */

    char default_host[] = "0.0.0.0";
    char connect_host[64];
    int port = ESD_DEFAULT_PORT;
    int host_div = 0;
   
    /* see if we have a remote speaker to play to */
    espeaker = host ? host : getenv( "ESPEAKER" );
    if ( espeaker != NULL ) {
	/* split the espeaker host into host:port */
	host_div = strcspn( espeaker, ":" );

	/* get host */
	if ( host_div ) {
	    strncpy( connect_host, espeaker, host_div );
	    connect_host[ host_div ] = '\0';
	} else {
	    strcpy( connect_host, default_host );
	}

        /* Resolving the host name */
        if ( ( he = gethostbyname( connect_host ) ) == NULL ) {
	    fprintf( stderr, "Can\'t resolve host name \"%s\"!\n", 
		     connect_host);
	    return(-1);
        }
	memcpy( (struct in_addr *) &socket_addr.sin_addr, he->h_addr,
	       sizeof( struct in_addr ) );

	/* get port */
	if ( host_div != strlen( espeaker ) )
	    port = atoi( espeaker + host_div + 1 );
	if ( !port ) 
	    port = ESD_DEFAULT_PORT;
	/* printf( "(remote) host is %s : %d\n", connect_host, port ); */
    } else if( !inet_aton( default_host, &socket_addr.sin_addr ) ) {
	fprintf( stderr, "couldn't convert %s to inet address\n", 
		 default_host );
	return -1;
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
int esd_play_stream( esd_format_t format, int rate, char *host, char *name )
{
    int sock;
    int proto = ESD_PROTO_STREAM_PLAY;
    char name_buf[ ESD_NAME_MAX ];

    /* connect to the EsounD server */
    sock = esd_open_sound( host );
    if ( sock < 0 ) 
	return sock;

    /* prepare the name buffer */
    if ( name )
	strncpy( name_buf, name, ESD_NAME_MAX );
    else
	name_buf[ 0 ] = '\0';

    /* send the audio format information */
    if ( write( sock, &proto, sizeof(proto) ) != sizeof(proto) )
	return -1;
    if ( write( sock, &format, sizeof(format) ) != sizeof(format) )
	return -1;
    if( write( sock, &rate, sizeof(rate) ) != sizeof(rate) )
	return -1;
    if( write( sock, name_buf, ESD_NAME_MAX ) != ESD_NAME_MAX )
	return -1;

    /* flush the socket */
    fsync( sock );
    
    return sock;
}

/*******************************************************************/
/* open a socket for playing as a stream, fallback to /dev/dsp */
int esd_play_stream_fallback( esd_format_t format, int rate, char *host, char *name )
{
    int socket_out;
    /* try to open a connection to the server */
    socket_out = esd_play_stream( format, rate, host, name );
    if ( socket_out >= 0 ) 
	return socket_out;

    /* if ESPEAKER is set, this is an error, bail out */
    if ( getenv( "ESPEAKER" ) )
	return -1;

    /* go for /dev/dsp */
    esd_audio_format = format;
    esd_audio_rate = rate;
    socket_out = esd_audio_open();

    /* we either got it, or we didn't */
    return socket_out;
}

/*******************************************************************/
/* open a socket for monitoring as a stream */
int esd_monitor_stream( esd_format_t format, int rate, char *host, char *name )
{
    int sock;
    int proto = ESD_PROTO_STREAM_MON;
    char name_buf[ ESD_NAME_MAX ];

    /* connect to the EsounD server */
    sock = esd_open_sound( host );
    if ( sock < 0 ) 
	return sock;
    
    /* prepare the name buffer */
    if ( name )
	strncpy( name_buf, name, ESD_NAME_MAX );
    else
	name_buf[ 0 ] = '\0';

    /* send the audio format information */
    if ( write( sock, &proto, sizeof(proto) ) != sizeof(proto) )
	return -1;
    if ( write( sock, &format, sizeof(format) ) != sizeof(format) )
	return -1;
    if( write( sock, &rate, sizeof(rate) ) != sizeof(rate) )
	return -1;
    if( write( sock, name_buf, ESD_NAME_MAX ) != ESD_NAME_MAX )
	return -1;

    /* flush the socket */
    fsync( sock );
    
    return sock;
}

/*******************************************************************/
/* open a socket for recording as a stream */
int esd_record_stream( esd_format_t format, int rate, char *host, char *name )
{
    int sock;
    int proto = ESD_PROTO_STREAM_REC;
    char name_buf[ ESD_NAME_MAX ];

    /* connect to the EsounD server */
    sock = esd_open_sound( host );
    if ( sock < 0 ) 
	return sock;
    
    /* prepare the name buffer */
    if ( name )
	strncpy( name_buf, name, ESD_NAME_MAX );
    else
	name_buf[ 0 ] = '\0';

    /* send the audio format information */
    if ( write( sock, &proto, sizeof(proto) ) != sizeof(proto) )
	return -1;
    if ( write( sock, &format, sizeof(format) ) != sizeof(format) )
	return -1;
    if( write( sock, &rate, sizeof(rate) ) != sizeof(rate) )
	return -1;
    if( write( sock, name_buf, ESD_NAME_MAX ) != ESD_NAME_MAX )
	return -1;

    /* flush the socket */
    fsync( sock );
    
    return sock;
}

/*******************************************************************/
/* open a socket for recording as a stream, fallback to /dev/dsp */
int esd_record_stream_fallback( esd_format_t format, int rate, char *host, char *name )
{
    int socket_out;

    /* try to open a connection to the server */
    socket_out = esd_record_stream( format, rate, host, name );
    if ( socket_out >= 0 ) 
	return socket_out;

    /* if ESPEAKER is set, this is an error, bail out */
    if ( getenv( "ESPEAKER" ) )
	return -1;

    /* go for /dev/dsp */
    esd_audio_format = format;
    esd_audio_rate = rate;
    socket_out = esd_audio_open();

    /* we either got it, or we didn't */
    return socket_out;
}

/*******************************************************************/
/* cache a sample in the server returns sample id, <= 0 is error */
int esd_sample_cache( int esd, esd_format_t format, int rate, long size, char *name )
{
    int id = 0;
    int proto = ESD_PROTO_SAMPLE_CACHE;

    /* prepare the name buffer */
    char name_buf[ ESD_NAME_MAX ];
    if ( name )
	strncpy( name_buf, name, ESD_NAME_MAX );
    else
	name_buf[ 0 ] = '\0';
    /* printf( "caching sample: %s (%d) - %ld bytes\n", 
	    name_buf, esd, size ); */

    /* send the necessary information */
    if ( write( esd, &proto, sizeof(proto) ) != sizeof(proto) )
	return -1;

    if ( write( esd, &format, sizeof(format) ) != sizeof(format) )
	return -1;
    if ( write( esd, &rate, sizeof(rate) ) != sizeof(rate) )
	return -1;
    if ( write( esd, &size, sizeof(size) ) != sizeof(size) )
	return -1;
    if ( write( esd, name_buf, ESD_NAME_MAX ) != ESD_NAME_MAX )
	return -1;

    /* flush the socket */
    fsync( esd );

    /* get the sample id back from the server */
    if ( read( esd, &id, sizeof(id) ) != sizeof(id) )
	return -1;

    /* return the sample id to the client */
    return id;
}

/*******************************************************************/
/* call this after sending the sample data to the server, should */
/* return the same sample id read previously, <= 0 is error */
int esd_confirm_sample_cache( int esd )
{
    int id = 0;

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

    /* printf( "freeing sample (%d) - <%d>\n", esd, sample ); */

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

    /* printf( "playing sample (%d) - <%d>\n", esd, sample ); */

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

    /* printf( "looping sample (%d) - <%d>\n", esd, sample ); */

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

    /* printf( "stopping sample (%d) - <%d>\n", esd, sample ); */

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
/* closes fd, previously obtained by esd_open */
int esd_close( int esd )
{
    return close( esd );
}
