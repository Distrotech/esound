
#include "esd.h"

/*******************************************************************/
/* globals */

/* the list of the currently connected clients */
esd_client_t *esd_clients_list;

/* start unowned, first client claims it */
static int esd_is_owned = 0;

/* if owned, will prohibit foreign sources */
static int esd_is_locked = 1;

/* the key that locks the daemon */
static char esd_owner_key[ESD_KEY_LEN];

/*******************************************************************/
/* for debugging purposes, dump the list of the clients and data */
void dump_clients()
{
    long addr;
    short port;
    esd_client_t *clients = esd_clients_list;

    /* printf ( "adding client 0x%08x\n", new_client ); */
    while ( clients != NULL ) {
	port = ntohs( clients->source.sin_port );
	addr = ntohl( clients->source.sin_addr.s_addr );
	printf( "client from: %u.%u.%u.%u:%d (%d) [0x%p]\n", 
		(unsigned int) addr >> 24, 
		(unsigned int) (addr >> 16) % 256, 
		(unsigned int) (addr >> 8) % 256, 
		(unsigned int) addr % 256, 
		port, clients->fd, clients );
	clients = clients->next;
    }
    return;
}

/*******************************************************************/
/* deallocate memory for the client */
void free_client( esd_client_t *client )
{
    /* free the client memory */
    free( client );
    return;
}

/*******************************************************************/
/* add a complete new client into the list of clients at head */
void add_new_client( esd_client_t *new_client )
{
    /* printf ( "adding client 0x%08x\n", new_client ); */
    new_client->next = esd_clients_list;
    esd_clients_list = new_client;
    return;
}

/*******************************************************************/
/* erase a client from the client list */
void erase_client( esd_client_t *client )
{
    esd_client_t *previous = NULL;
    esd_client_t *current = esd_clients_list;

    /* iterate until we hit a NULL */
    while ( current != NULL )
    {
	/* see if we hit the target client */
	if ( current == client ) {
	    if( previous != NULL ){
		/* we are deleting in the middle of the list */
		previous->next = current->next;
	    } else { 
		/* we are deleting the head of the list */
		esd_clients_list = current->next;
	    }

	    printf ( "closing client connection (%d)\n", 
		     client->fd );
	    close( client->fd );
	    free_client( client );

	    return;
	}

	/* iterate through the list */
	previous = current;
	current = current->next;
    }

    /* hmm, we didn't find the desired client, just get on with life */
    printf( "client not found (%d)\n", client->fd );
    return;
}


/*******************************************************************/
/* resets ownership of the sound daemon */
void clear_auth( int signum )
{
    int i;

    printf( "resetting ownership of sound daemon\n" );

    /* reset the access rights */
    esd_is_owned = 0;
    esd_is_locked = 1;

    /* scramble the stored key */
    srand( time(NULL) );
    for ( i = 0 ; i < ESD_KEY_LEN ; i++ ) {
	esd_owner_key[ i ] = rand() % 256;
    }

    /* reset signal handler, if not called from a signal, no effect */
    signal( SIGHUP, clear_auth );
}

/*******************************************************************/
/* checks for authorization to use the sound daemon */
int validate_source( int fd, struct sockaddr_in source, int owner_only )
{
    char submitted_key[ESD_KEY_LEN];

    if ( ESD_KEY_LEN != read( fd, submitted_key, ESD_KEY_LEN ) ) {
	/* not even ESD_KEY_LEN bytes available? shut it down */
	return 0;
    }

    if( !esd_is_owned ) {
	/* noone owns it yet, the first client claims ownership */
	printf( "esd auth: claiming ownership of esd.\n" );
	esd_is_locked = 1;
	memcpy( esd_owner_key, submitted_key, ESD_KEY_LEN );
	esd_is_owned = 1;
	return 1;
    }

    if( !esd_is_locked && !owner_only ) {
	/* anyone can connect to it */
	/* printf( "esd auth: esd not locked, accepted.\n" ); */
	return 1;
    }

    if( !memcmp( esd_owner_key, submitted_key, ESD_KEY_LEN ) ) {
	/* the client key matches the owner, trivial acceptance */
	/* printf( "esd auth: source authorized to use esd\n" ); */
	return 1;
    }

    /* TODO: maybe check based on source ip? */
    /* if ( !owner_only ) { check_ip_etc(); } */
    /* the client is not authorized to connect to the server */
    printf( "esd auth: NOT authorized to use esd, closing conn.\n" );
    return 0;
}

/*******************************************************************/
/* daemon rejects untrusted clients, return boolean ok */
int esd_proto_lock( esd_client_t *client )
{
    /* can only be locked by owner */
    if( !validate_source( client->fd, client->source, 1 ) ) {
	/* connection refused, shut down the client connection */
	printf( "only owner may lock sound daemon\n" );
	return 0;
    }

    printf( "locking sound daemon\n" );
    esd_is_locked = 1;
    return 1;
}

/*******************************************************************/
/* allows anyone to connect to the sound daemon, return boolean ok */
int esd_proto_unlock( esd_client_t *client )
{

    /* can only be unlocked by owner */
    if( !validate_source( client->fd, client->source, 1 ) ) {
	/* connection refused, shut down the client connection */
	printf( "only owner may unlock sound daemon\n" );
	return 0;
    }

    printf( "unlocking sound daemon\n" );
    esd_is_locked = 0;
    return 1;
}

/*******************************************************************/
/* manage the single recording client, return boolean ok */
int esd_proto_stream_recorder( esd_client_t *client )
{
    /* if we're already recording, go away */
    if ( esd_recorder ) {
	return 0;
    }

    /* sign up the new recorder client */
    esd_recorder = new_stream_player( client->fd );
    if ( esd_recorder != NULL ) {

	/* let the device know we want to record */
	audio_close();
	esd_audio_format |= ESD_RECORD;
	audio_open();

	/* flesh out the recorder */
	esd_recorder->parent = client;
	printf ( "recording on client (%d)\n", client->fd );

    } else {
	/* failed to initialize the recorder, kill its client */
	return 0;
    }

    return 1;
}

/*******************************************************************/
/* manage the single monitoring client, return boolean ok */
int esd_proto_stream_monitor( esd_client_t *client )
{
    /* if we're already monitoring, go away */
    if ( esd_monitor ) {
	return 0;
    }

    /* sign up the new monitor client */
    esd_monitor = new_stream_player( client->fd );
    if ( esd_monitor != NULL ) {
	/* flesh out the monitor */
	esd_monitor->parent = client;
	printf ( "monitoring on client (%d)\n", client->fd );
    } else {
	/* failed to initialize the recorder, kill its client */
	return 0;
    }

    return 1;
}

/*******************************************************************/
/* cache a sample from the client, return boolean ok  */
int esd_proto_sample_cache( esd_client_t *client )
{
    esd_sample_t *sample;
    int length;
    printf( "caching sample (%d)\n", client->fd );

    sample = new_sample( client->fd );
    /* add to the list of sample */
    if ( sample != NULL ) {
	sample->parent = client;
	if( ( length = read_sample( sample ) ) < sample->sample_length ) {
	    return 0;
	} else
	    add_sample( sample );
    } else {
	printf( "not enough mem for sample, closing\n");
	return 0;
    }

    write( client->fd, &sample->sample_id, sizeof(sample->sample_id) );
    fsync( client->fd );

    return 1;
}

/*******************************************************************/
/* free a sample cached by the client, return boolean ok */
int esd_proto_sample_free( esd_client_t *client )
{
    int sample_id;

    if ( read( client->fd, &sample_id, sizeof(sample_id) ) 
	 != sizeof( sample_id ) )
	return 0;

    printf( "freeing sample (%d)\n", sample_id );
    erase_sample( sample_id );

    if ( write( client->fd, &sample_id, sizeof(sample_id) ) 
	 != sizeof( sample_id ) )
	return 0;

    return 1;
}

/*******************************************************************/
/* play a sample cached by the client, return boolean ok */
int esd_proto_sample_play( esd_client_t *client )
{
    int sample_id;

    if ( read( client->fd, &sample_id, sizeof(sample_id) ) 
	 != sizeof( sample_id ) )
	return 0;
    
    printf( "playing sample <%d>\n", sample_id );
    if ( !play_sample( sample_id, 0 ) )
	sample_id = 0;

    if ( write( client->fd, &sample_id, sizeof(sample_id) ) 
	 != sizeof( sample_id ) )
	return 0;

    return 1;
}

/*******************************************************************/
/* play a sample cached by the client, return boolean ok */
int esd_proto_sample_loop( esd_client_t *client )
{
    int sample_id;

    if ( read( client->fd, &sample_id, sizeof(sample_id) ) 
	 != sizeof( sample_id ) )
	return 0;
    
    printf( "looping sample <%d>\n", sample_id );
    if ( !play_sample( sample_id, 1 ) )
	sample_id = 0;

    if ( write( client->fd, &sample_id, sizeof(sample_id) ) 
	 != sizeof( sample_id ) )
	return 0;

    return 1;
}

/*******************************************************************/
/* play a sample cached by the client, return boolean ok */
int esd_proto_sample_stop( esd_client_t *client )
{
    int sample_id;

    if ( read( client->fd, &sample_id, sizeof(sample_id) ) 
	 != sizeof( sample_id ) )
	return 0;
    
    printf( "stopping sample <%d>\n", sample_id );
    if ( !stop_sample( sample_id ) )
	sample_id = 0;

    if ( write( client->fd, &sample_id, sizeof(sample_id) ) 
	 != sizeof( sample_id ) )
	return 0;

    return 1;
}

/*******************************************************************/
/* checks for new connections at listener - zero when done */
int get_new_clients( int listen )
{
    int fd;
    struct sockaddr_in incoming;
    size_t size_in = sizeof(struct sockaddr_in);
    esd_client_t *new_client = NULL;
    
    unsigned long addr;
    short port;

    /* see who awakened us */
    do {
	fd = accept( listen, (struct sockaddr*) &incoming, &size_in );
	if ( fd > 0 ) {
	    port = ntohs( incoming.sin_port );
	    addr = ntohl( incoming.sin_addr.s_addr );
	    printf( "new client from: %u.%u.%u.%u:%d (%d)\n", 
		    (unsigned int) addr >> 24, 
		    (unsigned int) (addr >> 16) % 256, 
		    (unsigned int) (addr >> 8) % 256, 
		    (unsigned int) addr % 256, 
		    port, fd );
	    
	    /* make sure we have the memory to save the client... */
	    new_client = (esd_client_t*) malloc( sizeof(esd_client_t) );
	    if ( new_client == NULL ) {
		close( fd );
		return -1;
	    }

	    /* authenticate user... */
	    if( !validate_source( fd, incoming, 0 ) ) {
		/* connection refused, try next client connection */
		free( new_client ); new_client = NULL;
		close( fd );
		continue;
	    }

	    /* fill in the new_client structure - sockaddr = works!? */
	    /* request = ..._INVALID forces polling client next time */
	    new_client->next = NULL;
	    new_client->request = ESD_PROTO_INVALID;
	    new_client->fd = fd;
	    new_client->source = incoming; 
	    
	    add_new_client( new_client );
	}
    } while ( fd > 0 );

    return 0;
}

/*******************************************************************/
/* checks for new client requiests - returns 1 */
int poll_client_requests()
{
    esd_client_t *client = NULL;
    esd_client_t *erase = NULL;
    esd_player_t *player = NULL;
    fd_set rd_fds;
    struct timeval timeout;
    int can_read, length, is_ok;

    /* for each readable socket in the FD_SET, check protocol reqs? */
    /* check all, as some may become readable between the previous */
    /* blocking select() and now */

    /* for each client */
    client = esd_clients_list;
    while ( client != NULL )
    {
	/* if it's a streaming client connection, just skip it */
	/* data will be read (if available) during the mix process */
	if ( client->request == ESD_PROTO_STREAM_PLAY
	     || client->request == ESD_PROTO_STREAM_REC
	     || client->request == ESD_PROTO_STREAM_MON ) {
	    client = client->next;
	    continue;
	}
	    
	/* find out if this client wants to do anything yet */
	timeout.tv_sec = 0;
	timeout.tv_usec = 0;
	FD_ZERO( &rd_fds );
	FD_SET( client->fd, &rd_fds );
	can_read = select( client->fd + 1, &rd_fds, NULL, NULL, &timeout );
	if ( !can_read ) {
	    client = client->next;
	    continue;
	}

	/* make sure there's a request as EOF may return as readable */
	length = read( client->fd, &client->request, 
		      sizeof(client->request) );

	if ( length <= 0 ) {
	    /* no more data available from that client, close it */
	    printf( "no more protocol requests for client (%d)\n", 
		    client->fd );
	    is_ok = 0;

	} else {

	    /* find out what we want to do next */
	    switch ( client->request )
	    {
	    case ESD_PROTO_LOCK:
		is_ok = esd_proto_lock( client );
		break;
		
	    case ESD_PROTO_UNLOCK:
		is_ok = esd_proto_unlock( client );
		break;
		
	    case ESD_PROTO_STREAM_PLAY:
		player = new_stream_player( client->fd );
		
		/* add to the list of players */
		if ( player != NULL ) {
		    player->parent = client;
		    add_player( player );
		    is_ok = 1;
		} else {
		    is_ok = 0;
		}
		break;
		
	    case ESD_PROTO_STREAM_REC:
		is_ok = esd_proto_stream_recorder( client );
		break;
		
	    case ESD_PROTO_STREAM_MON:
		is_ok = esd_proto_stream_monitor( client );
		break;
		
	    case ESD_PROTO_SAMPLE_CACHE:
		is_ok = esd_proto_sample_cache( client );
		break;
		
	    case ESD_PROTO_SAMPLE_FREE:
		is_ok = esd_proto_sample_free( client );
		break;
		
	    case ESD_PROTO_SAMPLE_PLAY:
		is_ok = esd_proto_sample_play( client );
		break;
		
	    case ESD_PROTO_SAMPLE_LOOP:
		is_ok = esd_proto_sample_loop( client );
		break;
		
	    case ESD_PROTO_SAMPLE_STOP:
		is_ok = esd_proto_sample_stop( client );
		break;
		
	    default:
		printf( "unknown protocol request:  %d (check esd.h)\n",
			client->request );
		is_ok = 0;
		/* free( player ); player = NULL; */
		break;
	    }
	}

	/* if there was a problem, erase the client */
	if ( !is_ok ) {
	    erase = client; 
	}

	/* update the iterator  before removing */
	client = client->next;
	if ( erase != NULL ) {
	    erase_client( erase );
	    erase = NULL;
	}
    }

    return 1;
}

/*******************************************************************/
/* blocks waiting for data from the listener, and client conns. */
int wait_for_clients_and_data( int listen )
{
    fd_set rd_fds;
    struct timeval timeout;
    struct timeval *timeout_ptr = NULL;
    esd_client_t *client = esd_clients_list;
    int max_fd = listen;

    /* add the listener to the file descriptor list */
    FD_ZERO( &rd_fds );
    FD_SET( listen, &rd_fds );

    /* add the clients to the list, too */
    while ( client != NULL )
    {
	/* add this client */
	FD_SET( client->fd, &rd_fds );

	/* update the maximum fd for the select() */
	if ( client->fd > max_fd )
	    max_fd = client->fd;

	/* next client */
	client = client->next;
    }

    
    if ( esd_recorder ) {
	timeout.tv_sec = 0;
	timeout.tv_usec = 0;
	timeout_ptr = &timeout;
    }

    select( max_fd+1, &rd_fds, NULL, NULL, timeout_ptr );

    return;
}
