
#include "esd-server.h"

/*******************************************************************/
/* globals */

/* start unowned, first client claims it */
static int esd_is_owned = 0;

/* if owned, will prohibit foreign sources */
static int esd_is_locked = 1;

/* the key that locks the daemon */
static char esd_owner_key[ESD_KEY_LEN];

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
    int ok = 1;		/* already validated, can't fail */

    printf( "locking sound daemon\n" );
    esd_is_locked = 1;

    write( client->fd, &ok, sizeof(ok) );
    fsync( client->fd );

    client->state = ESD_NEXT_REQUEST;
    return ok;
}

/*******************************************************************/
/* allows anyone to connect to the sound daemon, return boolean ok */
int esd_proto_unlock( esd_client_t *client )
{
    int ok = 1;		/* already validated, can't fail */

    printf( "unlocking sound daemon\n" );
    esd_is_locked = 0;

    write( client->fd, &ok, sizeof(ok) );
    fsync( client->fd );

    client->state = ESD_NEXT_REQUEST;
    return ok;
}

/*******************************************************************/
/* daemon eats sound data, without playing anything, return boolean ok */
int esd_proto_standby( esd_client_t *client )
{
    int ok = 1;		/* already validated, can't fail */

    /* we're already on standby, no problem */
    if ( esd_on_standby ) {
	write( client->fd, &ok, sizeof(ok) );
	fsync( client->fd );

	client->state = ESD_NEXT_REQUEST;
	return ok;
    }	

    printf( "setting sound daemon to standby\n" );
    esd_on_standby = 1;
    /* TODO: close down any recorders, too */
    esd_audio_close();

    write( client->fd, &ok, sizeof(ok) );
    fsync( client->fd );

    client->state = ESD_NEXT_REQUEST;
    return ok;
}

/*******************************************************************/
/* daemon eats sound data, without playing anything, return boolean ok */
int esd_proto_resume( esd_client_t *client )
{
    int ok = 1;		/* already validated, can't fail */

    /* we're already not on standby, no problem */
    if ( !esd_on_standby ) {
	write( client->fd, &ok, sizeof(ok) );
	fsync( client->fd );

	client->state = ESD_NEXT_REQUEST;
	return ok;
    }	

    printf( "resuming sound daemon\n" );
    esd_audio_open();
    esd_on_standby = 0;

    write( client->fd, &ok, sizeof(ok) );
    fsync( client->fd );

    client->state = ESD_NEXT_REQUEST;
    return ok;
}

/*******************************************************************/
/* add another stream player to the list, returns boolean ok */
int esd_proto_stream_play( esd_client_t *client )
{
    /* spawn a new player to handle this stream */
    esd_player_t *player = NULL;
    player = new_stream_player( client->fd );
    
    /* we got one, right? */
    if ( !player ) {
	return 0;
    }
    
    /* add to the list of players */
    player->parent = client;
    add_player( player );

    client->state = ESD_STREAMING_DATA;
    return 1;
}

/*******************************************************************/
/* manage the single recording client, return boolean ok */
int esd_proto_stream_recorder( esd_client_t *client )
{
    /* if we're already recording, or in standby mode, go away */
    if ( esd_recorder || esd_on_standby ) {
	return 0;
    }

    /* sign up the new recorder client */
    esd_recorder = new_stream_player( client->fd );
    if ( esd_recorder != NULL ) {

	/* let the device know we want to record */
	esd_audio_close();
	esd_audio_format |= ESD_RECORD;
	esd_audio_open();

	/* flesh out the recorder */
	esd_recorder->parent = client;
	printf ( "recording on client (%d)\n", client->fd );

    } else {
	/* failed to initialize the recorder, kill its client */
	return 0;
    }

    client->state = ESD_STREAMING_DATA;
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

    client->state = ESD_STREAMING_DATA;
    return 1;
}

/*******************************************************************/
/* cache a sample from the client, return boolean ok  */
int esd_proto_sample_cache( esd_client_t *client )
{
    esd_sample_t *sample;
    int length;
    printf( "proto: caching sample (%d)\n", client->fd );

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

    client->state = ESD_NEXT_REQUEST;
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

    printf( "proto: erasing sample (%d)\n", sample_id );
    erase_sample( sample_id );

    if ( write( client->fd, &sample_id, sizeof(sample_id) ) 
	 != sizeof( sample_id ) )
	return 0;

    client->state = ESD_NEXT_REQUEST;
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

    client->state = ESD_NEXT_REQUEST;
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

    client->state = ESD_NEXT_REQUEST;
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

    client->state = ESD_NEXT_REQUEST;
    return 1;
}

static int do_validated_action ( esd_client_t *client )
{
    int is_ok;
    
    switch ( client->request ) {
    case ESD_PROTO_LOCK:
        is_ok = esd_proto_lock( client );
        break;

    case ESD_PROTO_UNLOCK:
        is_ok = esd_proto_unlock( client );
        break;

    case ESD_PROTO_STANDBY:
        is_ok = esd_proto_standby( client );
        break;

    case ESD_PROTO_RESUME:
        is_ok = esd_proto_resume( client );
        break;

    case ESD_PROTO_INVALID:
        /* this was initial validation of client */
        is_ok = 1;
	break;

    default:
        printf( "unknown protocol request:  %d (check esd.h)\n",
	        client->request );
        is_ok = 0;
        break;
    }

    return is_ok;
}

/*******************************************************************/
/* checks for new client requiests - returns 1 */
int poll_client_requests()
{
    esd_client_t *client = NULL;
    esd_client_t *erase = NULL;
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
	/* TODO: if ( client->state == ESD_STREAMING_DATA ) */
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

 	if ( client->state == ESD_NEEDS_VALIDATION ) {
 	    /* validate client */
 	    is_ok = validate_source( client->fd, client->source, 0 );
 
 	    if ( is_ok ) {
 		is_ok = do_validated_action( client );
 	        client->state = ESD_NEXT_REQUEST;
 	    }
 	    else if ( client->request != ESD_PROTO_INVALID ) {
 	        /* send failure code to client */
 	        printf( "only owner may control sound daemon\n" );
 		write( client->fd, &is_ok, sizeof(is_ok) );
		fsync( client->fd );
 	    }
 	}
 	else {
 
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
 		case ESD_PROTO_UNLOCK:
 		case ESD_PROTO_STANDBY:
 		case ESD_PROTO_RESUME:
 		    /* these requests need validation -- the validation */
 		    /* key will appear in the next read */
 		    client->state = ESD_NEEDS_VALIDATION;
 		    is_ok = 1;
 		    break;

 		case ESD_PROTO_STREAM_PLAY:
		    is_ok = esd_proto_stream_play ( client );
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
 		    break;
 		}
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

