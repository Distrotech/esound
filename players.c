
#include "esd-server.h"

/*******************************************************************/
/* globals */
esd_player_t *esd_players_list = NULL;
esd_player_t *esd_recorder = NULL;
esd_player_t *esd_monitor_list = NULL;

/*******************************************************************/
/* for debugging purposes, dump the list of the clients and data */
void dump_players()
{
    esd_player_t *player = esd_players_list;

    if ( !esdbg_trace ) return;

    while ( player != NULL ) {
	printf( "-%02d- player: [%p]\n", 
		player->source_id, player );
	player = player->next;
    }
    return;
}

/*******************************************************************/
/* deallocate memory for the player */
void free_player( esd_player_t *player )
{
    esd_sample_t *sample;

    /* see if we need to do any housekeeping */
    if ( ( player->format & ESD_MASK_MODE ) == ESD_STREAM ) {
	/* TODO: erase client should be independent of players */
	erase_client( player->parent );
    } else if ( ( player->format & ESD_MASK_MODE ) == ESD_SAMPLE ) {
	sample = (esd_sample_t *) (player->parent);
	sample->ref_count--;

	ESDBG_TRACE( printf( "<%02d> free player: [%p] refs=%d erase?=%d\n", 
			     player->source_id, player, sample->ref_count, 
			     sample->erase_when_done ); );

	if ( sample->erase_when_done && !sample->ref_count ) {
	    ESDBG_TRACE( printf( "<%02d> free_player: erasing sample\n", 
				 sample->sample_id ); );

	    erase_sample( sample->sample_id );
	}
    }

    /* free any memory allocated with the player */
    free( player->data_buffer );

    /* free the player memory itself */
    free( player );

    return;
}

/*******************************************************************/
/* add a complete new player into the list of players at head */
void add_player( esd_player_t *player )
{
    /* printf ( "adding player %p\n", new_player ); */
    player->next = esd_players_list;
    esd_players_list = player;
    return;
}

/*******************************************************************/
/* erase a player from the player list */
void erase_player( esd_player_t *player )
{
    esd_player_t *previous = NULL;
    esd_player_t *current = esd_players_list;

    /* iterate until we hit a NULL */
    while ( current != NULL )
    {
	/* see if we hit the target player */
	if ( current == player ) {
	    if( previous != NULL ){
		/* we are deleting in the middle of the list */
		previous->next = current->next;
	    } else { 
		/* we are deleting the head of the list */
		esd_players_list = current->next;
	    }

	    /* TODO: delete if needed */

	    free_player( player );

	    return;
	}

	/* iterate through the list */
	previous = current;
	current = current->next;
    }

    /* hmm, we didn't find the desired player, just get on with life */
    ESDBG_TRACE( printf( "-%02d- player not found\n", player->source_id ); );
    return;
}


/*******************************************************************/
/* erase a monitor from the monitor list */
void erase_monitor( esd_player_t *monitor )
{
    esd_player_t *previous = NULL;
    esd_player_t *current = esd_monitor_list;

    /* iterate until we hit a NULL */
    while ( current != NULL )
    {
	/* see if we hit the target monitor */
	if ( current == monitor ) {
	    if( previous != NULL ){
		/* we are deleting in the middle of the list */
		previous->next = current->next;
	    } else { 
		/* we are deleting the head of the list */
		esd_monitor_list = current->next;
	    }

	    /* TODO: delete if needed */
	    free_player( monitor );

	    return;
	}

	/* iterate through the list */
	previous = current;
	current = current->next;
    }

    /* hmm, we didn't find the desired monitor, just get on with life */
    ESDBG_TRACE( printf( "-%02d- monitor not found\n", monitor->source_id ); );
    return;
}


/*******************************************************************/
/* read block of data from client, return < 0 to have it erased */
int read_player( esd_player_t *player )
{
    fd_set rd_fds;
    int actual = 0, actual_2nd = 0, can_read = 0;
    struct timeval timeout;
    char message[ 100 ];
    short data, *pos; /* used for swapping */
    esd_client_t *client;
    short *buffer;

    switch( player->format & ESD_MASK_MODE ) {
    case ESD_STREAM:
	/* use select to prevent blocking clients that are ready */
	timeout.tv_sec = 0;
	timeout.tv_usec = 0;
	FD_ZERO( &rd_fds );
	FD_SET( player->source_id, &rd_fds );

	/* if the data is ready, read a block */
	can_read = select( player->source_id + 1, 
			   &rd_fds, NULL, NULL, &timeout ) ;
	if ( can_read > 0 )
	{
	    ESD_READ_BIN( player->source_id, player->data_buffer, 
			  player->buffer_length, actual, "str rd" );

	    /* check for end of stream */
	    if ( actual == 0 ) 
		return -1;
 
	    /* more data, save how much we got */
	    player->actual_length = actual;

	    /* endian swap multi-byte data if we need to */
	    client = (esd_client_t *) (player->parent);
	    if ( client->swap_byte_order 
		 && ( (player->format & ESD_MASK_BITS) == ESD_BITS16 ) )
	    {
		printf( "swapping...\n" );
		buffer = (short*) player->data_buffer;
		for ( pos = buffer 
			  ; pos < buffer + actual / sizeof(short)
			  ; pos += sizeof(short) )
		{
		    data = swap_endian_16( (*pos) );
		    *pos = data;
		}
	    }

	} else if ( can_read < 0 ) {
	    sprintf( message, "error reading client (%d)\n", 
		     player->source_id );
	    perror( message );
	    return -1;
	}

	break;

    case ESD_SAMPLE:

	/* printf( "player [%p], pos = %d, format = 0x%08x\n", 
		player, player->last_pos, player->format ); */
	
	/* only keep going if we didn't want to stop looping */
	if ( ( player->last_pos ) == 0 &&
	    ( ( ((esd_sample_t*)player->parent)->format & ESD_MASK_FUNC ) 
	      == ESD_STOP ) ) 
	{
	    return -1;
	}

	/* copy the data from the sample to the player */
	actual = ( ((esd_sample_t*)player->parent)->sample_length 
		   - player->last_pos > player->buffer_length )
	    ? player->buffer_length 
	    : ((esd_sample_t*)player->parent)->sample_length - player->last_pos;
	if ( actual > 0 ) {
	    memcpy( player->data_buffer, 
		    ((esd_sample_t*)player->parent)->data_buffer 
		    + player->last_pos, actual );
	    player->last_pos += actual;
	    if ( ( player->format & ESD_MASK_FUNC ) != ESD_LOOP ) {
		/* we're done for this iteration */
		break;
	    }
	} else {
	    /* something horrible has happened to the sample */
	    return -1;
	}

	/* we are looping, see if we need to copy another block */
	if ( player->last_pos >= ((esd_sample_t*)player->parent)->sample_length ) {
	    player->last_pos = 0;
	}

	actual_2nd = ( ((esd_sample_t*)player->parent)->sample_length 
		   - player->last_pos > player->buffer_length - actual )
	    ? player->buffer_length - actual
	    : ((esd_sample_t*)player->parent)->sample_length - player->last_pos;
	if ( actual_2nd >= 0 ) {
	    /* only keep going if we didn't want to stop looping */
	    if ( ( ((esd_sample_t*)player->parent)->format & ESD_MASK_FUNC )
		 != ESD_STOP ) {
		memcpy( player->data_buffer + actual, 
			((esd_sample_t*)player->parent)->data_buffer 
			+ player->last_pos, actual_2nd );
		player->last_pos += actual_2nd;
		actual += actual_2nd;

		/* make sure we're not at the end */
		if ( player->last_pos >= ((esd_sample_t*)player->parent)->sample_length ) {
		    player->last_pos = 0;
		}
	    }

	} else {
	    /* something horrible has happened to the sample */
	    return -1;
	}

	/* sample data is swapped as it's cached, no swap needed here */
	break;

    default:
	ESDBG_TRACE( printf( "-%02d- read_player: format 0x%08x not supported\n", 
			     player->source_id, player->format ); );
	return -1;
    }

    return actual;
}

/*******************************************************************/
/* send the players buffer to it's associated socket, erase if EOF */
void monitor_write( void *output_buffer, int length ) {
    fd_set wr_fds;
    int can_write;
    struct timeval timeout;
    esd_player_t *monitor, *remove = NULL;

    /* make sure we have a monitor connected */
    if ( !esd_monitor_list ) 
	return;

    /* shuffle through the list of monitors */
    monitor = esd_monitor_list;
    while ( monitor != NULL ) {

	/* see if we can write to the socket */
	timeout.tv_sec = 0;
	timeout.tv_usec = 0;
	FD_ZERO( &wr_fds );
	FD_SET( monitor->source_id, &wr_fds );
	can_write = select( monitor->source_id + 1, NULL, &wr_fds, NULL, &timeout );
	if ( !can_write ) 
	    break;

	/* mix down the monitor's buffer */
	if ( (esd_audio_format & ESD_MASK_BITS) == ESD_BITS16 ) {
	    length = mix_from_stereo_16s( monitor->data_buffer, monitor->buffer_length,
					  monitor->rate, monitor->format, 
					  output_buffer, length, esd_audio_rate );
	}
	else {
	    length = mix_from_stereo_8u( monitor->data_buffer, monitor->buffer_length,
					 monitor->rate, monitor->format, 
					 output_buffer, length, esd_audio_rate );
	}

	/* write the data buffer to the socket */
	ESD_WRITE_BIN( monitor->source_id, monitor->data_buffer, 
		       monitor->buffer_length, length, "mon wr" );

	if ( length < 0 ) {
	    /* error on write, close it down */
	    ESDBG_TRACE( printf( "(%02d) closing monitor\n", monitor->source_id ); );
	    remove = monitor;
	}

	monitor = monitor->next;

	if ( remove ) {
	    erase_client( remove->parent );
	    erase_monitor( remove );
	    remove = NULL;
	}
    }
    
    return;
}

void recorder_write() {
    fd_set wr_fds;
    int length, can_write;
    struct timeval timeout;

    /* make sure we have a recorder connected */
    if ( !esd_recorder ) 
	return;

    /* see if we can write to the socket */
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;
    FD_ZERO( &wr_fds );
    FD_SET( esd_recorder->source_id, &wr_fds );
    can_write = select( esd_recorder->source_id + 1, NULL, &wr_fds, NULL, &timeout );
    if ( !can_write ) 
	return;

    /* write the data buffer to the socket */
    ESD_WRITE_BIN( esd_recorder->source_id, esd_recorder->data_buffer, 
		   esd_recorder->buffer_length, length, "rec wr" );

    if ( length < 0 ) {
	/* couldn't send anything, close it down */
	ESDBG_TRACE( printf( "(%02d) closing recorder\n", esd_recorder->source_id ); );

	/* stop recording */
	esd_audio_close();
	esd_audio_format &= ~ESD_RECORD;
	esd_audio_open();

	/* clear the recorder */
	erase_client( esd_recorder->parent );
	free_player( esd_recorder );
	esd_recorder = NULL;
    }

    return;
}

/*******************************************************************/
/* allocate and initialize a player from client stream */
esd_player_t *new_stream_player( esd_client_t *client )
{
    esd_player_t *player;
    int actual;

    /* make sure we have the memory to save the client... */
    player = (esd_player_t*) malloc( sizeof(esd_player_t) );
    if ( player == NULL ) {
	return NULL;
    }
    
    /* and initialize the player */
    player->next = NULL;
    player->parent = NULL;

    ESD_READ_INT( client->fd, &player->format, sizeof(player->format), 
		  actual, "newstr fmt" );
    if ( client->swap_byte_order )
	player->format = swap_endian_32( player->format );

    player->format &= ~ESD_MASK_MODE; /* force to ESD_STREAM */
    ESD_READ_INT( client->fd, &player->rate, sizeof(player->rate), 
		  actual, "newstr rat" );
    if ( client->swap_byte_order )
	player->rate = swap_endian_32( player->rate );

    player->source_id = client->fd;
    ESD_READ_BIN( client->fd, player->name, ESD_NAME_MAX, 
		  actual, "newstr nam" );

    player->name[ ESD_NAME_MAX - 1 ] = '\0';

    ESDBG_TRACE( printf( "(%02d) stream %s: 0x%08x at %d Hz\n", client ->fd, 
			 player->name, player->format, player->rate ); );

  /* Reduce buffers on sockets to the minimum needed */
    {
      int fd_bufsize;
      int src_format;
      
      src_format = player->format;
      fd_bufsize = esd_buf_size_octets;
      if (player->rate > 0)
	fd_bufsize = (fd_bufsize * esd_audio_rate) / player->rate;
      else
	fd_bufsize = (fd_bufsize * esd_audio_rate) / 44100;
      if ((src_format & ESD_MASK_BITS) == ESD_BITS16)
	{
	  fd_bufsize *= 2;
	}
      if (!((src_format & ESD_MASK_CHAN) == ESD_MONO))
	{
	  fd_bufsize *= 2;	      
	}
      setsockopt(player->source_id, SOL_SOCKET, SO_SNDBUF, 
		 &fd_bufsize, sizeof(fd_bufsize));
      setsockopt(player->source_id, SOL_SOCKET, SO_RCVBUF,  
		 &fd_bufsize, sizeof(fd_bufsize));
    }
    /* calculate buffer length to match the mix buffer duration */
    player->buffer_length = esd_buf_size_octets * player->rate / esd_audio_rate;
    if ( (player->format & ESD_MASK_BITS) == ESD_BITS8 )
	player->buffer_length /= 2;
    if ( (player->format & ESD_MASK_CHAN) == ESD_MONO )
	player->buffer_length /= 2;

    /* force to an even multiple of 4 */
    player->buffer_length += ( 4 - (player->buffer_length % 4) ) % 4;

    player->data_buffer
	= (void *) malloc( player->buffer_length );

    /* if not enough room for data buffer, clean up, and return NULL */
    if ( player->data_buffer == NULL ) {
	free( player );
	return NULL;
    }

    /* everything's ok, return the allocated player */
    /* player->last_read = time(NULL); */
    player->left_vol_scale = player->right_vol_scale = ESD_VOLUME_BASE;

    ESDBG_TRACE( printf( "(%02d) player: [%p]\n", player->source_id, player ); );

    return player;
}

/*******************************************************************/
/* allocate and initialize a player from client stream */
esd_player_t *new_sample_player( int sample_id, int loop )
{
    esd_player_t *player;
    esd_sample_t *sample;

    /* find the sample we want to play */
    for ( sample = esd_samples_list ; sample != NULL 
	      ; sample = sample->next )
    {
	if ( sample->sample_id == sample_id ) {
	    break;
	}
    }
    /* if we didn't find it, return NULL */
    if ( sample == NULL ) {
	return NULL;
    }
        
    /* make sure we have the memory to save the player... */
    player = (esd_player_t*) malloc( sizeof(esd_player_t) );
    if ( player == NULL ) {
	return NULL;
    }
    
    /* and initialize the player */
    player->next = NULL;
    player->parent = sample;
    player->format = sample->format & ~ESD_MASK_MODE;
    player->format |= ESD_SAMPLE;
    if ( loop ) {
	player->format &= ~ESD_MASK_FUNC;
	player->format |= ESD_LOOP;
    }
    player->rate = sample->rate;
    player->source_id = sample->sample_id;
    player->left_vol_scale = sample->left_vol_scale;
    player->right_vol_scale = sample->right_vol_scale;

    ESDBG_TRACE( printf( "<%02d> connection format: 0x%08x at %d Hz\n", 
			 player->source_id, player->format, player->rate ); );

    /* calculate buffer length to match the mix buffer duration */
    player->buffer_length = esd_buf_size_octets * player->rate / esd_audio_rate;
    if ( (player->format & ESD_MASK_BITS) == ESD_BITS8 )
	player->buffer_length /= 2;
    if ( (player->format & ESD_MASK_CHAN) == ESD_MONO )
	player->buffer_length /= 2;

    /* force to an even multiple of 4 */
    player->buffer_length += ( 4 - (player->buffer_length % 4) ) % 4;

    player->data_buffer
	= (void *) malloc( player->buffer_length );

    /* if not enough room for data buffer, clean up, and return NULL */
    if ( player->data_buffer == NULL ) {
	free( player );
	return NULL;
    }

    /* everything's ok, return the allocated player */
    player->last_pos = 0;
    sample->ref_count++;
    sample->erase_when_done = 0;

    ESDBG_TRACE( printf( "<%02d> new player: refs=%d [%p]\n", 
			 player->source_id, sample->ref_count, player ); );

    return player;
}
