
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

    if ( esdbg_trace ) 
	printf( "(ca) resetting ownership of sound daemon\n" );

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
/* checks for authorization to use the sound daemon, and endianness */
int check_endian( esd_client_t *client )
{
    int endian, actual;

    ESD_READ_INT( client->fd, &endian, sizeof(endian), actual, "endian" );
    if ( sizeof(endian) != actual ) {
	/* not even enough bytes available? shut it down */
	return 0;
    }

    if ( endian == ESD_ENDIAN_KEY ) {
	/* printf( "same endian order.\n" ); */
	client->swap_byte_order = 0;
    } else if ( endian == ESD_SWAP_ENDIAN_KEY ) {
	/* printf( "different endian order!\n" ); */
	client->swap_byte_order = 1;
    } else {
	if ( esdbg_trace ) 
	    printf( "(%02d) unknown endian key: 0x%08x (same = 0x%08x, diff = 0x%08x)\n",
		    client->fd, endian, ESD_ENDIAN_KEY, ESD_SWAP_ENDIAN_KEY );
	
	return -1;
    }

    /* now we're done */
    return 1;
}

int validate_source( esd_client_t *client, struct sockaddr_in source, int owner_only )
{
    int actual;
    char submitted_key[ESD_KEY_LEN];

    ESD_READ_BIN( client->fd, submitted_key, ESD_KEY_LEN, actual, "key" );
    if ( ESD_KEY_LEN != actual ) {
	/* not even ESD_KEY_LEN bytes available? shut it down */
	return 0;
    }

    if( !esd_is_owned ) {
	/* noone owns it yet, the first client claims ownership */
	if ( esdbg_trace )
	    printf( "(%02d) esd auth: claiming ownership of esd.\n", client->fd );

	esd_is_locked = 1;
	memcpy( esd_owner_key, submitted_key, ESD_KEY_LEN );
	esd_is_owned = 1;
	goto check_endian;
    }

    if( !esd_is_locked && !owner_only ) {
	/* anyone can connect to it */
	/* printf( "esd auth: esd not locked, accepted.\n" ); */
	goto check_endian;
    }

    if( !memcmp( esd_owner_key, submitted_key, ESD_KEY_LEN ) ) {
	/* the client key matches the owner, trivial acceptance */
	/* printf( "esd auth: source authorized to use esd\n" ); */
	goto check_endian;
    }

    /* TODO: maybe check based on source ip? */
    /* if ( !owner_only ) { check_ip_etc(); } */
    /* the client is not authorized to connect to the server */
    if ( esdbg_trace )
	printf( "(%02d) esd auth: NOT authorized to use esd, closing conn.\n",
		client->fd );

    return 0;

 check_endian:
    if ( check_endian( client ) <= 0 ) { 
	/* irix is weird, may return -1, instead of EAGAIN, try again anyway */
	if ( esdbg_trace ) printf( "will check for endian key next trip\n" );
	client->state = ESD_NEEDS_ENDCHECK;
	return 1;
    }

    /* now we're done */
    return 1;
}

/*******************************************************************/
/* daemon rejects untrusted clients, return boolean ok */
int esd_proto_lock( esd_client_t *client )
{
    int ok = 1, actual;		/* already validated, can't fail */
    int client_ok = maybe_swap_32( client->swap_byte_order, ok );

    if ( esdbg_trace )
	printf( "(%02d) locking sound daemon\n", client->fd );
    esd_is_locked = 1;

    ESD_WRITE_INT( client->fd, &client_ok, sizeof(client_ok), actual, "lock ok" );
    fsync( client->fd );

    client->state = ESD_NEXT_REQUEST;
    return ok;
}

/*******************************************************************/
/* allows anyone to connect to the sound daemon, return boolean ok */
int esd_proto_unlock( esd_client_t *client )
{
    int ok = 1, actual;		/* already validated, can't fail */
    int client_ok = maybe_swap_32( client->swap_byte_order, ok );

    if ( esdbg_trace )
	printf( "(%02d) unlocking sound daemon\n", client->fd );
    esd_is_locked = 0;

    ESD_WRITE_INT( client->fd, &client_ok, sizeof(client_ok), actual, "unlck ok" );
    fsync( client->fd );

    client->state = ESD_NEXT_REQUEST;
    return ok;
}

/*******************************************************************/
/* daemon eats sound data, without playing anything, return boolean ok */
int esd_proto_standby( esd_client_t *client )
{
    int ok = 1, actual;		/* already validated, can't fail */
    int client_ok = maybe_swap_32( client->swap_byte_order, ok );

    /* we're already on standby, no problem */
    if ( esd_on_standby ) {
	ESD_WRITE_INT( client->fd, &client_ok, sizeof(client_ok), actual, "stdby ok" );
	fsync( client->fd );

	client->state = ESD_NEXT_REQUEST;
	return ok;
    }	

    if ( esdbg_trace )
	printf( "(%02d) setting sound daemon to standby\n", client->fd );

    esd_on_standby = 1;
    /* TODO: close down any recorders, too */
    esd_audio_close();

    ESD_WRITE_INT( client->fd, &client_ok, sizeof(client_ok), actual, "stdby ok" );
    fsync( client->fd );

    client->state = ESD_NEXT_REQUEST;
    return ok;
}

/*******************************************************************/
/* daemon eats sound data, without playing anything, return boolean ok */
int esd_proto_resume( esd_client_t *client )
{
    int ok = 1, actual;		/* already validated, can't fail */
    int client_ok = maybe_swap_32( client->swap_byte_order, ok );

    /* we're already not on standby, no problem */
    if ( !esd_on_standby ) {
	ESD_WRITE_INT( client->fd, &client_ok, sizeof(client_ok), actual, "resum ok" );
	fsync( client->fd );

	client->state = ESD_NEXT_REQUEST;
	return ok;
    }	

    if ( esdbg_trace ) 
	printf( "(%02d) resuming sound daemon\n", client->fd );

    esd_audio_open();
    esd_on_standby = 0;

    ESD_WRITE_INT( client->fd, &client_ok, sizeof(client_ok), actual, "resum ok" );
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
    player = new_stream_player( client );
    
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
    esd_recorder = new_stream_player( client );
    if ( esd_recorder != NULL ) {

	/* let the device know we want to record */
	esd_audio_close();
	esd_audio_format |= ESD_RECORD;
	esd_audio_open();

	/* flesh out the recorder */
	esd_recorder->parent = client;

	if ( esdbg_trace )
	    printf ( "(%02d) recording on client\n", client->fd );

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
    esd_monitor = new_stream_player( client );
    if ( esd_monitor != NULL ) {
	/* flesh out the monitor */
	esd_monitor->parent = client;

	if ( esdbg_trace )
	    printf ( "(%02d) monitoring on client\n", client->fd );
    } else {
	/* failed to initialize the recorder, kill its client */
	return 0;
    }

    client->state = ESD_STREAMING_DATA;
    return 1;
}

/*******************************************************************/
/* manage the filter client, return boolean ok */
int esd_proto_stream_filter( esd_client_t *client )
{
    esd_player_t *filter;
    
    /* sign up the new monitor client */
    filter = new_stream_player( client );
    if ( filter != NULL ) {
	/* flesh out the monitor */
	filter->parent = client;
	filter->next = esd_filter_list;
	esd_filter_list = filter;

	if ( esdbg_trace )
	    printf ( "(%02d) filter on client\n", client->fd );
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
    int client_id;

    if ( esdbg_trace )
	printf( "(%02d) proto: caching sample\n", client->fd );

    if ( client->state == ESD_CACHING_SAMPLE ) {
	sample = find_caching_sample( client );
    } else {
	sample = new_sample( client );
	add_sample( sample );
    }

    /* add to the list of sample */
    if ( sample != NULL ) {
	sample->parent = client;
	if( ( length = read_sample( sample ) ) < sample->sample_length ) {
	    client->state = ESD_CACHING_SAMPLE;
	    if ( esdbg_trace ) 
		printf( "(%02d) continue caching sample next trip\n", client->fd );
	    return 1;
	} else {
	    if ( esdbg_trace ) 
		printf( "(%02d) sample cached, moving on\n", client->fd );
	    client->state = ESD_NEXT_REQUEST;
	}
    } else {
	fprintf( stderr, "(%02d) not enough mem for sample, closing\n", 
		 client->fd );
	return 0;
    }

    client_id = maybe_swap_32( client->swap_byte_order, 
			       sample->sample_id );
    ESD_WRITE_INT( client->fd, &client_id, sizeof(client_id), length, "smp cach" );
    fsync( client->fd );
    return 1;
}

/*******************************************************************/
/* check for an existing sample name */
int esd_proto_sample_getid(esd_client_t *client)
{
    int client_id, actual, length;
    esd_sample_t *sample = esd_samples_list;
    char namebuf[ESD_NAME_MAX];

    if ( esdbg_trace )
	printf( "(%02d) proto: getting sample ID\n", client->fd );

    ESD_READ_BIN(client->fd, namebuf, ESD_NAME_MAX, actual, "smp getid");
    namebuf[ESD_NAME_MAX - 1] = '\0';

    while(sample) {
	if(!strcmp(sample->name, namebuf))
	    break;
    }

    if(sample)
	client_id = maybe_swap_32( client->swap_byte_order, 
				   sample->sample_id );
    else
	client_id = maybe_swap_32(client->swap_byte_order, -1);

    ESD_WRITE_INT( client->fd, &client_id, sizeof(client_id), length, "smp getid" );
    fsync( client->fd );
    return 1;
}


/*******************************************************************/
/* free a sample cached by the client, return boolean ok */
int esd_proto_sample_free( esd_client_t *client )
{
    int sample_id, client_id, actual;

    ESD_READ_INT( client->fd, &client_id, sizeof(client_id), actual, "smp free" );
    if ( sizeof( sample_id ) != actual ) {
	client->state = ESD_NEEDS_REQDATA;
	return 1;
    }

    sample_id = maybe_swap_32( client->swap_byte_order, client_id );

    if ( esdbg_trace )
	printf( "(%02d) proto: erasing sample <%d>\n", client->fd, sample_id );
    erase_sample( sample_id );

    ESD_WRITE_INT( client->fd, &client_id, sizeof(client_id), actual, "smp free" );
    if ( sizeof( client_id ) != actual )
	return 0;

    client->state = ESD_NEXT_REQUEST;
    return 1;
}

/*******************************************************************/
/* play a sample cached by the client, return boolean ok */
int esd_proto_sample_play( esd_client_t *client )
{
    int sample_id, client_id, actual;
    
    ESD_READ_INT( client->fd, &client_id, sizeof(client_id), actual, "smp play" );
    if ( sizeof( client_id ) != actual ) {
	client->state = ESD_NEEDS_REQDATA;
	return 1;
    }
    sample_id = maybe_swap_32( client->swap_byte_order, client_id );

    if ( esdbg_trace )
	printf( "(%02d) proto: playing sample <%d>\n", client->fd, sample_id );
    if ( !play_sample( sample_id, 0 ) )
	sample_id = 0;

    ESD_WRITE_INT( client->fd, &client_id, sizeof(client_id), actual, "smp play" );
    if ( sizeof( client_id ) != actual )
	return 0;

    client->state = ESD_NEXT_REQUEST;
    return 1;
}

/*******************************************************************/
/* play a sample cached by the client, return boolean ok */
int esd_proto_sample_loop( esd_client_t *client )
{
    int sample_id, client_id, actual;

    ESD_READ_INT( client->fd, &client_id, sizeof(client_id), actual, "smp loop" );
    if ( sizeof( client_id ) != actual ) {
	client->state = ESD_NEEDS_REQDATA;
	return 1;
    }

    sample_id = maybe_swap_32( client->swap_byte_order, client_id );

    if ( esdbg_trace )
	printf( "(%02d) proto: looping sample <%d>\n", client->fd, sample_id );

    if ( !play_sample( sample_id, 1 ) )
	sample_id = 0;

    ESD_WRITE_INT( client->fd, &client_id, sizeof(client_id), actual, "smp loop" );
    if ( sizeof( sample_id ) != actual )
	return 0;

    client->state = ESD_NEXT_REQUEST;
    return 1;
}

/*******************************************************************/
/* play a sample cached by the client, return boolean ok */
int esd_proto_sample_stop( esd_client_t *client )
{
    int sample_id, client_id, actual;

    ESD_READ_INT( client->fd, &client_id, sizeof(client_id), actual, "smp stop" );
    if ( sizeof( client_id ) != actual ) {
	client->state = ESD_NEEDS_REQDATA;
	return 1;
    }
    
    sample_id = maybe_swap_32( client->swap_byte_order, client_id );

    if ( esdbg_trace )
	printf( "(%02d) proto: stopping sample <%d>\n", client->fd, sample_id );

    if ( !stop_sample( sample_id ) )
	sample_id = 0;

    ESD_WRITE_INT( client->fd, &client_id, sizeof(client_id), actual, "smp stop" );
    if ( sizeof( client_id ) != actual )
	return 0;

    client->state = ESD_NEXT_REQUEST;
    return 1;
}

/*******************************************************************/
/* now that we trust the client, do it's bidding, return boolean ok */
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
        fprintf( stderr, "(%02d) proto: unknown protocol request:  0x%08x\n",
	        client->fd, client->request );
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
	if ( client->state == ESD_STREAMING_DATA ) {
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

	/* see what the client needs to do next */
	/* TODO: switch( client->state ) --> function pointers off of client->state */
 	if ( client->state == ESD_NEEDS_VALIDATION ) {
 	    /* validate client */
 	    is_ok = validate_source( client, client->source, 0 );
 
 	    if ( is_ok && !(client->state == ESD_NEEDS_ENDCHECK) ) {
 		is_ok = do_validated_action( client );
 	        client->state = ESD_NEXT_REQUEST;
 	    }
 	    else if ( client->state == ESD_NEEDS_ENDCHECK ) {
		/* wait for next trip through */
		if ( esdbg_trace )
		    printf( "waiting to check for endian key next trip\n" );
	    } 
 	    else if ( client->request != ESD_PROTO_INVALID ) {
 	        /* send failure code to client */
 	        fprintf( stderr, 
			 "(%02d) poll: only owner may control sound daemon\n",
			 client->fd );
		ESD_WRITE_INT( client->fd, &is_ok, sizeof(is_ok), length, "poll val" );
		fsync( client->fd );
 	    }
 	}
	else if ( client->state == ESD_NEEDS_ENDCHECK ) {
	    if ( esdbg_trace ) printf( "rechecking for endian key\n" );
	    is_ok = check_endian( client );

	    /* check_endian() return vals don't exactly match is_ok's meaning */
	    if ( is_ok < 0 ) is_ok = 0;		/* socket error */
	    else if ( is_ok == 0 ) is_ok = 1; 	/* try next time */
	    else client->state = ESD_NEXT_REQUEST;
	}
	else if ( client->state == ESD_CACHING_SAMPLE ) {
	    if ( esdbg_trace ) printf( "resuming sample cache\n" );
	    is_ok = esd_proto_sample_cache( client );
	}
	else if ( client->state == ESD_NEEDS_REQDATA ) {
	    if ( esdbg_trace ) printf( "resuming request data\n" );

	    switch ( client->request )
	    {
	    case ESD_PROTO_SAMPLE_FREE:
		is_ok = esd_proto_sample_free( client ); break;
 
	    case ESD_PROTO_SAMPLE_PLAY:
		is_ok = esd_proto_sample_play( client ); break;
 
	    case ESD_PROTO_SAMPLE_LOOP:
		is_ok = esd_proto_sample_loop( client ); break;
		
	    case ESD_PROTO_SAMPLE_STOP:
		is_ok = esd_proto_sample_stop( client ); break;

	    default:
		if ( esdbg_trace ) printf( "(%02d) invalid request: %d\n",
					   client->fd, client->request );
		break;
	    }
	    
	}
 	else {
 
 	    /* make sure there's a request as EOF may return as readable */
	    if ( esdbg_comms ) printf( "--------------------------------\n" );
	    ESD_READ_INT( client->fd, &client->request, sizeof(client->request), 
			  length, "request" );
	    if ( esdbg_comms ) printf( "\n" );
	    if ( client->swap_byte_order )
		client->request = swap_endian_32( client->request );

 	    if ( length <= 0 ) {
 		/* no more data available from that client, close it */
		if ( esdbg_trace )
		    printf( "(%02d) no more protocol requests for client\n", 
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
 
 		case ESD_PROTO_STREAM_FILT:
 		    is_ok = esd_proto_stream_filter( client );
 		    break;

 		case ESD_PROTO_SAMPLE_CACHE:
 		    is_ok = esd_proto_sample_cache( client );
 		    break;

		case ESD_PROTO_SAMPLE_GETID:
		    is_ok = esd_proto_sample_getid( client );
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
		    fprintf( stderr, 
			     "(%02d) proto: unknown protocol request:  0x%08x\n",
			     client->fd, client->request );
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

