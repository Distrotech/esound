
#include "config.h"
#include "esd.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>
#include <signal.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/un.h>
#include <errno.h>
#include <sys/wait.h>

/*******************************************************************/
/* prototypes */
int esd_set_socket_buffers( int sock, int src_format, 
			    int src_rate, int base_rate );
static void dummy_signal(int signum);

/* dummy handler */
static void dummy_signal(int signum) {
      signal( signum, dummy_signal);
      return;
}

/* from esd_config.c */
extern char esd_spawn_options[];
extern int esd_no_spawn;
void esd_config_read(void);

/*******************************************************************/
/* alternate implementations */
#ifndef HAVE_INET_ATON
int inet_aton(const char *cp, struct in_addr *inp)
{
    union {
	unsigned int n;
	char parts[4];
    } u;
    int a=0,b=0,c=0,d=0, i;

    i = sscanf(cp, "%d.%d.%d.%d%*s", &a, &b, &c, &d);

    if(i != 4)
	return 0;

    u.parts[0] = a;
    u.parts[1] = b;
    u.parts[2] = c;
    u.parts[3] = d;

    inp->s_addr = u.n;

    return 1;
}
#endif

/*******************************************************************/
/* set socket buffer lengths to optimal length for audio data transfer */
int esd_set_socket_buffers( int sock, int src_format, 
			    int src_rate, int base_rate )
{
    int buf_size = ESD_BUF_SIZE;

    if ( src_rate > 0 ) buf_size = ( buf_size * base_rate ) / src_rate;
    if ( ( src_format & ESD_MASK_BITS ) == ESD_BITS16 )
	buf_size *= 2;
    if ( ! ( ( src_format & ESD_MASK_CHAN ) == ESD_MONO ) )
	buf_size *= 2;

    setsockopt( sock, SOL_SOCKET, SO_SNDBUF, &buf_size, sizeof( buf_size ) );
    setsockopt( sock, SOL_SOCKET, SO_RCVBUF, &buf_size, sizeof( buf_size ) );
    return buf_size;
}

/*******************************************************************/
/* get the stream latency to esound (latency is number of samples  */
/* at 44.1khz stereo 16 bit - you'll have to adjust if oyur input  */
/* sampling rate is less (in bytes per second)                     */
int esd_get_latency(int esd)
{
  int lag = 0;
  int proto = ESD_PROTO_LATENCY;
  void (*phandler)(int);

/* this is unavoidable - incase ESD "dissapears" (ie the socket conn dies) */
/* we need to catch SIGPIPE to avoid the default handler form giving us */
/* a bad day - ignore the SIGPIPE, then make sure to cathc all errors */
    phandler = signal( SIGPIPE, dummy_signal );    /* for closed esd conns */
    /* send the necessary information */
    if ( write( esd, &proto, sizeof(proto) ) != sizeof(proto) ) {
      signal( SIGPIPE, phandler ); 
      return -1;
    }

    /* get the latency back from the server */
    if ( read( esd, &lag, sizeof(lag) ) != sizeof(lag) ) {
      signal( SIGPIPE, phandler ); 
      return -1;
    }

    /* diagnostic info */
    /*
    if ( getenv( "ESDBG" ) )
	printf( "esound getting latency\n" );
    */

    /* return the sample id to the client */
    signal( SIGPIPE, phandler ); 
  
  lag += ESD_BUF_SIZE * 2;
  
  return lag;
}

/*******************************************************************/
/* send the authorization cookie, create one if needed */
int esd_send_auth( int sock )
{
    int auth_fd = -1, i = 0;
    int endian = ESD_ENDIAN_KEY;
    int reply;
    char *auth_filename = 0, auth_key[ESD_KEY_LEN];
    char *home = NULL;
    char tumbler = '\0';
    int namelen, retval;
    void (*phandler)(int);
  
/* this is unavoidable - incase ESD "dissapears" (ie the socket conn dies) */
/* we need to catch SIGPIPE to avoid the default handler form giving us */
/* a bad day - ignore the SIGPIPE, then make sure to cathc all errors */
    phandler = signal( SIGPIPE, dummy_signal );    /* for closed esd conns */
  
    /* assemble the authorization filename */
    home = getenv( "HOME" );
    if ( !home ) {
	fprintf( stderr, "HOME environment variable not set?\n" );
        signal( SIGPIPE, phandler ); 
	return -1;
    }

    namelen = strlen(home) + sizeof("/.esd_auth");
    if ((auth_filename = malloc(namelen + 1)) == 0) {
	fprintf( stderr, "Memory exhausted\n" );
        signal( SIGPIPE, phandler ); 
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

    /* read auth reply. esd will reply 1 as an int for yes and 0 for no */
    /* then close the connection */
    if ( sizeof(reply) != read( sock, &reply, sizeof(reply) ) ) {
      /* read ok failed */
      retval = 0;
      goto exit_fd;
    }
    /* we got a reply and it's no - so esd will close the socket now */
    /* on us anyway... time to return invalid auth... */
    if (reply == 0) {
      /* auth failed */
      retval = 0;
      goto exit_fd;
    }
  
  
    /* we've run the gauntlet, everything's ok, proceed as usual */
    /* fsync( sock ); */
    retval = 1;

 exit_fd:
    close( auth_fd );
 exit_fn:
    free( auth_filename );
    signal( SIGPIPE, phandler ); 
    return retval;
}

/*******************************************************************/
/* lock/unlock will disable/enable foreign clients from connecting */
int esd_lock( int esd ) {
    int proto = ESD_PROTO_LOCK;
    int ok = 0;
    void (*phandler)(int);
  
/* this is unavoidable - incase ESD "dissapears" (ie the socket conn dies) */
/* we need to catch SIGPIPE to avoid the default handler form giving us */
/* a bad day - ignore the SIGPIPE, then make sure to cathc all errors */
    phandler = signal( SIGPIPE, dummy_signal );    /* for closed esd conns */
    /* diagnostic info */
    /*
    if ( getenv( "ESDBG" ) )
        printf( "esound locking\n" );
    */

    write( esd, &proto, sizeof(proto) );
    esd_send_auth( esd );

    if ( read( esd, &ok, sizeof(ok) ) != sizeof(ok) ) {
      signal( SIGPIPE, phandler ); 
      return -1;
    }

    signal( SIGPIPE, phandler ); 
    return ok;
}

int esd_unlock( int esd ){
    int proto = ESD_PROTO_UNLOCK;
    int ok = 0;
    void (*phandler)(int);
  
/* this is unavoidable - incase ESD "dissapears" (ie the socket conn dies) */
/* we need to catch SIGPIPE to avoid the default handler form giving us */
/* a bad day - ignore the SIGPIPE, then make sure to cathc all errors */
    phandler = signal( SIGPIPE, dummy_signal );    /* for closed esd conns */
    /* diagnostic info */
    /*
    if ( getenv( "ESDBG" ) )
	printf( "esound unlocking\n" );
    */

    write( esd, &proto, sizeof(proto) );
    esd_send_auth( esd );

    if ( read( esd, &ok, sizeof(ok) ) != sizeof(ok) ) {
      signal( SIGPIPE, phandler ); 
      return -1;
    }

    signal( SIGPIPE, phandler ); 
    return ok;
}

/*******************************************************************/
/* standby/resume will free/reclaim audio device so others may use it */
int esd_standby( int esd )
{
    int proto = ESD_PROTO_STANDBY;
    int ok = 0;
    void (*phandler)(int);
  
/* this is unavoidable - incase ESD "dissapears" (ie the socket conn dies) */
/* we need to catch SIGPIPE to avoid the default handler form giving us */
/* a bad day - ignore the SIGPIPE, then make sure to cathc all errors */
    phandler = signal( SIGPIPE, dummy_signal );    /* for closed esd conns */
    /* diagnostic info */
    /*
    if ( getenv( "ESDBG" ) )
	printf( "esound standing by\n" );
    */

    write( esd, &proto, sizeof(proto) );
    esd_send_auth( esd );

    if ( read( esd, &ok, sizeof(ok) ) != sizeof(ok) ) {
      signal( SIGPIPE, phandler ); 
      return -1;
    }

    signal( SIGPIPE, phandler ); 
    return ok;
}

int esd_resume( int esd )
{
    int proto = ESD_PROTO_RESUME;
    int ok = 0;
    void (*phandler)(int);
  
/* this is unavoidable - incase ESD "dissapears" (ie the socket conn dies) */
/* we need to catch SIGPIPE to avoid the default handler form giving us */
/* a bad day - ignore the SIGPIPE, then make sure to cathc all errors */
    phandler = signal( SIGPIPE, dummy_signal );    /* for closed esd conns */
    /* diagnostic info */
    /*
    if ( getenv( "ESDBG" ) )
	printf( "esound resuming\n" );
    */

    write( esd, &proto, sizeof(proto) );
    esd_send_auth( esd );

    if ( read( esd, &ok, sizeof(ok) ) != sizeof(ok) ) {
      signal( SIGPIPE, phandler ); 
      return -1;
    }

    signal( SIGPIPE, phandler ); 
    return ok;
}

static int
esd_connect_tcpip(const char *host)
{
  const char *espeaker = NULL;
  struct hostent *he;
  struct sockaddr_in socket_addr;
  int socket_out = -1;
  int curstate = 1;
  char default_host[] = "0.0.0.0";
  char connect_host[64];
  int port = ESD_DEFAULT_PORT;
  unsigned int host_div = 0;
  
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
    return -1;
  return socket_out;	  
}

static int
esd_connect_unix(const char *host)
{
  struct sockaddr_un socket_unix;
  int socket_out = -1;
  int curstate = 1;
  
  /* create the socket, and set for non-blocking */
  socket_out = socket( AF_UNIX, SOCK_STREAM, 0 );
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
  socket_unix.sun_family = AF_UNIX;
  strncpy(socket_unix.sun_path, "/tmp/.esd/socket", sizeof(socket_unix.sun_path));
  
  if ( connect( socket_out,
	       (struct sockaddr *) &socket_unix,
	       sizeof(socket_unix.sun_family) + 
	       strlen(socket_unix.sun_path) ) < 0 )
    return -1;
  
  return socket_out;
}

/*******************************************************************/
/* initialize the socket to send data to the sound daemon */
int esd_open_sound( const char *host )
{
  int connect_count = 0;
  int socket_out = -1;
  char use_unix = 0;
  
  if (access("/tmp/.esd/socket", R_OK | W_OK) == -1)
    {
      if (errno == EACCES) 
	{
	  /* not allowed access - can't init unix socket comms - esd already */
	  /* there */
          socket_out = esd_connect_tcpip(host);
	}
      else if (errno == ENOENT)
	{
	  use_unix = 1;
	  /* does not exist - try tcp/ip */
	  socket_out = esd_connect_tcpip(host);	  
	}
    }
  else
    use_unix = 1;

  /* tcp/ip fialed or we havent tried unix sockets yet */
  if (socket_out < 0)
    {
      if (use_unix)
	socket_out = esd_connect_unix(host);
      else
	socket_out = esd_connect_tcpip(host);
    }

  /* try tcp/ip, if we tried via usock and it didn't work */
  if ((socket_out < 0) && use_unix)
    socket_out = esd_connect_tcpip(host);

  /* esd basically is uncontactable - lets run it and try again */
  if (socket_out < 0)
    {
      esd_config_read();

      if (!esd_no_spawn && !getenv("ESPEAKER"))
	{
	  int childpid;

	  childpid = fork();
	  if (!childpid)
	    {
	      if (!fork())
		{
		  char *cmd;

		  setsid();
		  
		  cmd = malloc(sizeof("esd ") + strlen(esd_spawn_options));
		  sprintf(cmd, "esd %s", esd_spawn_options);

		  execl("/bin/sh", "/bin/sh", "-c", cmd, NULL);
		  perror("execl");
		  _exit(1);
		}
	      else
		_exit(0);
	    }
	  else
	    {
	      int estat;
	      waitpid(childpid, &estat, 0); /* reap zombie */
	    }
	}
      else
	return socket_out;
    }

  /* sit and spin for a bit to wait for esd to start - try 5 times over */
  /* 5 seconds - if esd still hasnt started - give up */
  while ((socket_out < 0) && (connect_count < 60))
    {
#if defined(HAVE_NANOSLEEP) && !defined(HAVE_USLEEP)
      struct timespec timewait;
#endif
      
      socket_out = esd_connect_unix(host);
      if (socket_out < 0)
	socket_out = esd_connect_tcpip(host);
      connect_count++;

#if defined(HAVE_USLEEP)
      usleep(1000);
#elif defined(HAVE_NANOSLEEP)
      timewait.tv_sec = 0;
      timewait.tv_nsec = 10000000;
      nanosleep(&timewait, NULL);
#endif
    }

  if (socket_out >= 0)
    {
      /* send authorization */
      if (!esd_send_auth( socket_out )) 
	{
	/* couldn't send authorization key, bail */
	close( socket_out );	
	return -1;
      }
    }
  return socket_out;
}

/*******************************************************************/
/* open a socket for playing as a stream */
int esd_play_stream( esd_format_t format, int rate, 
		    const char *host, const char *name )
{
    int sock;
    int proto = ESD_PROTO_STREAM_PLAY;
    char name_buf[ ESD_NAME_MAX ];
    void (*phandler)(int);
      
    /* connect to the EsounD server */
    sock = esd_open_sound( host );
    if ( sock < 0 ) 
	return sock;

    /* prepare the name buffer */
    if ( name )
	strncpy( name_buf, name, ESD_NAME_MAX );
    else
	name_buf[ 0 ] = '\0';

/* this is unavoidable - incase ESD "dissapears" (ie the socket conn dies) */
/* we need to catch SIGPIPE to avoid the default handler form giving us */
/* a bad day - ignore the SIGPIPE, then make sure to cathc all errors */
    phandler = signal( SIGPIPE, dummy_signal );    /* for closed esd conns */
    /* send the audio format information */
    if ( write( sock, &proto, sizeof(proto) ) != sizeof(proto) ) {
      signal( SIGPIPE, phandler );
      return -1;
      }
    if ( write( sock, &format, sizeof(format) ) != sizeof(format) ) {
      signal( SIGPIPE, phandler );
      return -1;
      }
    if( write( sock, &rate, sizeof(rate) ) != sizeof(rate) ) {
      signal( SIGPIPE, phandler );
      return -1;
      }
    if( write( sock, name_buf, ESD_NAME_MAX ) != ESD_NAME_MAX ) {
      signal( SIGPIPE, phandler );
      return -1;
      }

    /* Reduce buffers on sockets to the minimum needed */
    esd_set_socket_buffers( sock, format, rate, 44100 );

    /* flush the socket */
    /* fsync( sock ); */
    
    /* diagnostic info */
    /*
    if ( getenv( "ESDBG" ) )
	printf( "esound playing stream\n" );
    */

  signal( SIGPIPE, phandler );
  return sock;
}

/*******************************************************************/
/* open a socket for playing as a stream, fallback to /dev/dsp */
int esd_play_stream_fallback( esd_format_t format, int rate, 
			      const char *host, const char *name )
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

    /* diagnostic info */
    /*
    if ( getenv( "ESDBG" ) )
	printf( "esound playing stream fallback\n" );
    */

    /* we either got it, or we didn't */
    return socket_out;
}

/*******************************************************************/
/* open a socket for monitoring as a stream */
int esd_monitor_stream( esd_format_t format, int rate, 
			const char *host, const char *name )
{
    int sock;
    int proto = ESD_PROTO_STREAM_MON;
    char name_buf[ ESD_NAME_MAX ];
    void (*phandler)(int);

    /* connect to the EsounD server */
    sock = esd_open_sound( host );
    if ( sock < 0 ) 
	return sock;
    
    /* prepare the name buffer */
    if ( name )
	strncpy( name_buf, name, ESD_NAME_MAX );
    else
	name_buf[ 0 ] = '\0';

/* this is unavoidable - incase ESD "dissapears" (ie the socket conn dies) */
/* we need to catch SIGPIPE to avoid the default handler form giving us */
/* a bad day - ignore the SIGPIPE, then make sure to cathc all errors */
    phandler = signal( SIGPIPE, dummy_signal );    /* for closed esd conns */
    /* send the audio format information */
    if ( write( sock, &proto, sizeof(proto) ) != sizeof(proto) ) {
      signal( SIGPIPE, phandler ); 
      return -1;
    }
    if ( write( sock, &format, sizeof(format) ) != sizeof(format) ) {
      signal( SIGPIPE, phandler ); 
      return -1;
    }
    if( write( sock, &rate, sizeof(rate) ) != sizeof(rate) ) {
      signal( SIGPIPE, phandler ); 
      return -1;
    }
    if( write( sock, name_buf, ESD_NAME_MAX ) != ESD_NAME_MAX ) {
      signal( SIGPIPE, phandler ); 
      return -1;
    }

    /* Reduce buffers on sockets to the minimum needed */
    esd_set_socket_buffers( sock, format, rate, 44100 );

    /* flush the socket */
    /* fsync( sock ); */
    
    /* diagnostic info */
    /*
    if ( getenv( "ESDBG" ) )
	printf( "esound monitoring stream\n" );
    */

    signal( SIGPIPE, phandler ); 
    return sock;
}

/*******************************************************************/
/* open a socket for filtering as a stream */
int esd_filter_stream( esd_format_t format, int rate, 
		       const char *host, const char *name )
{
    int sock;
    int proto = ESD_PROTO_STREAM_FILT;
    char name_buf[ ESD_NAME_MAX ];
    void (*phandler)(int);

    /* connect to the EsounD server */
    sock = esd_open_sound( host );
    if ( sock < 0 ) 
	return sock;
    
    /* prepare the name buffer */
    if ( name )
	strncpy( name_buf, name, ESD_NAME_MAX );
    else
	name_buf[ 0 ] = '\0';

/* this is unavoidable - incase ESD "dissapears" (ie the socket conn dies) */
/* we need to catch SIGPIPE to avoid the default handler form giving us */
/* a bad day - ignore the SIGPIPE, then make sure to cathc all errors */
    phandler = signal( SIGPIPE, dummy_signal );    /* for closed esd conns */
    /* send the audio format information */
    if ( write( sock, &proto, sizeof(proto) ) != sizeof(proto) ) {
      signal( SIGPIPE, phandler ); 
      return -1;
    }
    if ( write( sock, &format, sizeof(format) ) != sizeof(format) ) {
      signal( SIGPIPE, phandler ); 
      return -1;
    }
    if( write( sock, &rate, sizeof(rate) ) != sizeof(rate) ) {
      signal( SIGPIPE, phandler ); 
      return -1;
    }
    if( write( sock, name_buf, ESD_NAME_MAX ) != ESD_NAME_MAX ) {
      signal( SIGPIPE, phandler ); 
      return -1;
    }

    /* Reduce buffers on sockets to the minimum needed */
    esd_set_socket_buffers( sock, format, rate, 44100 );

    /* flush the socket */
    /* fsync( sock ); */
    
    /* diagnostic info */
    /*
    if ( getenv( "ESDBG" ) )
	printf( "esound filterng stream\n" );
    */

    signal( SIGPIPE, phandler ); 
    return sock;
}

/*******************************************************************/
/* open a socket for recording as a stream */
int esd_record_stream( esd_format_t format, int rate, 
		       const char *host, const char *name )
{
    int sock;
    int proto = ESD_PROTO_STREAM_REC;
    char name_buf[ ESD_NAME_MAX ];
    void (*phandler)(int);

    /* connect to the EsounD server */
    sock = esd_open_sound( host );
    if ( sock < 0 ) 
	return sock;
    
    /* prepare the name buffer */
    if ( name )
	strncpy( name_buf, name, ESD_NAME_MAX );
    else
	name_buf[ 0 ] = '\0';

/* this is unavoidable - incase ESD "dissapears" (ie the socket conn dies) */
/* we need to catch SIGPIPE to avoid the default handler form giving us */
/* a bad day - ignore the SIGPIPE, then make sure to cathc all errors */
    phandler = signal( SIGPIPE, dummy_signal );    /* for closed esd conns */
    /* send the audio format information */
    if ( write( sock, &proto, sizeof(proto) ) != sizeof(proto) ) {
      signal( SIGPIPE, phandler ); 
      return -1;
    }
    if ( write( sock, &format, sizeof(format) ) != sizeof(format) ) {
      signal( SIGPIPE, phandler ); 
      return -1;
    }
    if( write( sock, &rate, sizeof(rate) ) != sizeof(rate) ) {
      signal( SIGPIPE, phandler ); 
      return -1;
    }
    if( write( sock, name_buf, ESD_NAME_MAX ) != ESD_NAME_MAX ) {
      signal( SIGPIPE, phandler ); 
      return -1;
    }

    /* Reduce buffers on sockets to the minimum needed */
    esd_set_socket_buffers( sock, format, rate, 44100 );

    /* flush the socket */
    /* fsync( sock ); */
    
    /* diagnostic info */
    /*
    if ( getenv( "ESDBG" ) )
	printf( "esound recording stream\n" );
    */

    signal( SIGPIPE, phandler ); 
    return sock;
}

/*******************************************************************/
/* open a socket for recording as a stream, fallback to /dev/dsp */
int esd_record_stream_fallback( esd_format_t format, int rate, 
				const char *host, const char *name )
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

    /* Reduce buffers on sockets to the minimum needed */
    esd_set_socket_buffers( socket_out, format, rate, 44100 );

    /* diagnostic info */
    /*
    if ( getenv( "ESDBG" ) )
	printf( "esound recording stream fallback\n" );
    */

    /* we either got it, or we didn't */
    return socket_out;
}

/*******************************************************************/
/* cache a sample in the server returns sample id, <= 0 is error */
int esd_sample_cache( int esd, esd_format_t format, int rate, 
		      const int size, const char *name )
{
    int id = 0;
    int proto = ESD_PROTO_SAMPLE_CACHE;
    void (*phandler)(int);

    /* prepare the name buffer */
    char name_buf[ ESD_NAME_MAX ];
    if ( name )
	strncpy( name_buf, name, ESD_NAME_MAX );
    else
	name_buf[ 0 ] = '\0';
    /* printf( "caching sample: %s (%d) - %ld bytes\n", 
	    name_buf, esd, size ); */

/* this is unavoidable - incase ESD "dissapears" (ie the socket conn dies) */
/* we need to catch SIGPIPE to avoid the default handler form giving us */
/* a bad day - ignore the SIGPIPE, then make sure to cathc all errors */
    phandler = signal( SIGPIPE, dummy_signal );    /* for closed esd conns */
    /* send the necessary information */
    if ( write( esd, &proto, sizeof(proto) ) != sizeof(proto) ) {
      signal( SIGPIPE, phandler ); 
      return -1;
    }

    if ( write( esd, &format, sizeof(format) ) != sizeof(format) ) {
      signal( SIGPIPE, phandler ); 
      return -1;
    }
    if ( write( esd, &rate, sizeof(rate) ) != sizeof(rate) ) {
      signal( SIGPIPE, phandler ); 
      return -1;
    }
    if ( write( esd, &size, sizeof(size) ) != sizeof(size) ) {
      signal( SIGPIPE, phandler ); 
      return -1;
    }
    if ( write( esd, name_buf, ESD_NAME_MAX ) != ESD_NAME_MAX ) {
      signal( SIGPIPE, phandler ); 
      return -1;
    }

    /* flush the socket */
    /* fsync( esd ); */

    /* get the sample id back from the server */
    if ( read( esd, &id, sizeof(id) ) != sizeof(id) ) {
      signal( SIGPIPE, phandler ); 
      return -1;
    }

    /* diagnostic info */
    /*
    if ( getenv( "ESDBG" ) )
	printf( "esound caching sample\n" );
    */

    /* return the sample id to the client */
    signal( SIGPIPE, phandler ); 
    return id;
}

/*******************************************************************/
/* call this after sending the sample data to the server, should */
/* return the same sample id read previously, <= 0 is error */
int esd_confirm_sample_cache( int esd )
{
    int id = 0;
    void (*phandler)(int);

/* this is unavoidable - incase ESD "dissapears" (ie the socket conn dies) */
/* we need to catch SIGPIPE to avoid the default handler form giving us */
/* a bad day - ignore the SIGPIPE, then make sure to cathc all errors */
    phandler = signal( SIGPIPE, dummy_signal );    /* for closed esd conns */
    /* get the sample id back from the server */
    if ( read( esd, &id, sizeof(id) ) != sizeof(id) ) {
      signal( SIGPIPE, phandler ); 
      return -1;
    }

    /* diagnostic info */
    /*
    if ( getenv( "ESDBG" ) )
	printf( "esound confirming cached sample\n" );
    */

    /* return the sample id to the client */
    signal( SIGPIPE, phandler ); 
    return id;
}

/*******************************************************************/
/* get the sample ID for an already-cached sample */
int esd_sample_getid( int esd, const char *name)
{
    int proto = ESD_PROTO_SAMPLE_GETID;
    int id;
    char namebuf[ESD_NAME_MAX];
    void (*phandler)(int);

/* this is unavoidable - incase ESD "dissapears" (ie the socket conn dies) */
/* we need to catch SIGPIPE to avoid the default handler form giving us */
/* a bad day - ignore the SIGPIPE, then make sure to cathc all errors */
    phandler = signal( SIGPIPE, dummy_signal );    /* for closed esd conns */
    if ( write( esd, &proto, sizeof(proto) ) != sizeof(proto) ) {
      signal( SIGPIPE, phandler ); 
      return -1;
    }

    /* prepare the name buffer */
    if ( name )
	strncpy( namebuf, name, ESD_NAME_MAX );
    else
	namebuf[ 0 ] = '\0';

    if ( write( esd, namebuf, ESD_NAME_MAX ) != ESD_NAME_MAX ) {
      signal( SIGPIPE, phandler ); 
      return -1;
    }

    /* flush the socket */
    /* fsync( esd ); */

    /* get the sample id back from the server */
    if ( read( esd, &id, sizeof(id) ) != sizeof(id) ) {
      signal( SIGPIPE, phandler ); 
      return -1;
    }

    /* diagnostic info */
    /*
    if ( getenv( "ESDBG" ) )
	printf( "esound getting cached sample id: \'%s\' = %d\n",
		name, id );
    */
    
    /* return the sample id to the client */
    signal( SIGPIPE, phandler ); 
    return id;
}

/*******************************************************************/
/* uncache a sample in the server */
int esd_sample_free( int esd, int sample )
{
    int id;
    int proto = ESD_PROTO_SAMPLE_FREE;
    void (*phandler)(int);

    /* printf( "freeing sample (%d) - <%d>\n", esd, sample ); */

/* this is unavoidable - incase ESD "dissapears" (ie the socket conn dies) */
/* we need to catch SIGPIPE to avoid the default handler form giving us */
/* a bad day - ignore the SIGPIPE, then make sure to cathc all errors */
    phandler = signal( SIGPIPE, dummy_signal );    /* for closed esd conns */
    /* send the necessary information */
    if ( write( esd, &proto, sizeof(proto) ) != sizeof(proto) ) {
      signal( SIGPIPE, phandler ); 
      return -1;
    }
    if ( write( esd, &sample, sizeof(sample) ) != sizeof(sample) ) {
      signal( SIGPIPE, phandler ); 
      return -1;
    }
    /* fsync( esd ); */

    /* get the sample id back from the server */
    if ( read( esd, &id, sizeof(id) ) != sizeof(id) ) {
      signal( SIGPIPE, phandler ); 
      return -1;
    }

    /* diagnostic info */
    /*
    if ( getenv( "ESDBG" ) )
        printf( "esound freeing sample\n" );
    */

    /* return the id to the client (0 = error, 1 = ok) */
    signal( SIGPIPE, phandler ); 
    return id;
}

/*******************************************************************/
/* uncache a sample in the server */
int esd_sample_play( int esd, int sample )
{
    int is_ok;
    int proto = ESD_PROTO_SAMPLE_PLAY;
    void (*phandler)(int);

    /* printf( "playing sample (%d) - <%d>\n", esd, sample ); */

/* this is unavoidable - incase ESD "dissapears" (ie the socket conn dies) */
/* we need to catch SIGPIPE to avoid the default handler form giving us */
/* a bad day - ignore the SIGPIPE, then make sure to cathc all errors */
    phandler = signal( SIGPIPE, dummy_signal );    /* for closed esd conns */
    /* send the necessary information */
    if ( write( esd, &proto, sizeof(proto) ) != sizeof(proto) ) {
      signal( SIGPIPE, phandler ); 
      return -1;
    }
    if ( write( esd, &sample, sizeof(sample) ) != sizeof(sample) ) {
      signal( SIGPIPE, phandler ); 
      return -1;
    }
    /* fsync( esd ); */

    /* get the sample id back from the server */
    if ( read( esd, &is_ok, sizeof(is_ok) ) != sizeof(is_ok) ) {
      signal( SIGPIPE, phandler ); 
      return -1;
    }

    /* diagnostic info */
    /*
    if ( getenv( "ESDBG" ) )
	printf( "esound playing sample\n" );
    */

    /* return the id to the client (0 = error, 1 = ok) */
    signal( SIGPIPE, phandler ); 
    return is_ok;
}


/*******************************************************************/
/* loop a previously cached sample in the server */
int esd_sample_loop( int esd, int sample )
{
    int is_ok;
    int proto = ESD_PROTO_SAMPLE_LOOP;
    void (*phandler)(int);

    /* printf( "looping sample (%d) - <%d>\n", esd, sample ); */

/* this is unavoidable - incase ESD "dissapears" (ie the socket conn dies) */
/* we need to catch SIGPIPE to avoid the default handler form giving us */
/* a bad day - ignore the SIGPIPE, then make sure to cathc all errors */
    phandler = signal( SIGPIPE, dummy_signal );    /* for closed esd conns */
    /* send the necessary information */
    if ( write( esd, &proto, sizeof(proto) ) != sizeof(proto) ) {
      signal( SIGPIPE, phandler ); 
      return -1;
    }
    if ( write( esd, &sample, sizeof(sample) ) != sizeof(sample) ) {
      signal( SIGPIPE, phandler ); 
      return -1;
    }
    /* fsync( esd ); */

    /* get the sample id back from the server */
    if ( read( esd, &is_ok, sizeof(is_ok) ) != sizeof(is_ok) ) {
      signal( SIGPIPE, phandler ); 
      return -1;
    }

    /* diagnostic info */
    /*
    if ( getenv( "ESDBG" ) )
	printf( "esound looping sample\n" );
    */

    /* return the id to the client (0 = error, 1 = ok) */
    signal( SIGPIPE, phandler ); 
    return is_ok;
}

/*******************************************************************/
/* stop a looping sample in the server */
int esd_sample_stop( int esd, int sample )
{
    int is_ok;
    int proto = ESD_PROTO_SAMPLE_STOP;
    void (*phandler)(int);

    /* printf( "stopping sample (%d) - <%d>\n", esd, sample ); */

/* this is unavoidable - incase ESD "dissapears" (ie the socket conn dies) */
/* we need to catch SIGPIPE to avoid the default handler form giving us */
/* a bad day - ignore the SIGPIPE, then make sure to cathc all errors */
    phandler = signal( SIGPIPE, dummy_signal );    /* for closed esd conns */
    /* send the necessary information */
    if ( write( esd, &proto, sizeof(proto) ) != sizeof(proto) ) {
      signal( SIGPIPE, phandler ); 
      return -1;
    }
    if ( write( esd, &sample, sizeof(sample) ) != sizeof(sample) ) {
      signal( SIGPIPE, phandler ); 
      return -1;
    }
    /* fsync( esd ); */

    /* get the sample id back from the server */
    if ( read( esd, &is_ok, sizeof(is_ok) ) != sizeof(is_ok) ) {
      signal( SIGPIPE, phandler ); 
      return -1;
    }

    /* diagnostic info */
    /*
    if ( getenv( "ESDBG" ) )
	printf( "esound stopping sample\n" );
    */

    /* return the id to the client (0 = error, 1 = ok) */
    signal( SIGPIPE, phandler ); 
    return is_ok;
}

/*******************************************************************/
/* closes fd, previously obtained by esd_open */
int esd_close( int esd )
{
    /* diagnostic info */
    /*
    if ( getenv( "ESDBG" ) )
	printf( "esound closing\n" );
    */

    return close( esd );
}
