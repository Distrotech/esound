
#include "esd-server.h"

/*******************************************************************/
/* globals */
esd_sample_t *esd_samples_list = NULL;

static int esd_next_sample_id = 1;	/* sample id = 0 is an error */

/*******************************************************************/
/* for debugging purposes, dump the list of the clients and data */
void dump_samples()
{
    esd_sample_t *sample = esd_samples_list;

    while ( sample != NULL ) {
	printf( "sample: (%d) [0x%p] - %d bytes\n", 
		sample->sample_id, sample, sample->sample_length );
	sample = sample->next;
    }
    return;
}

/*******************************************************************/
/* deallocate memory for the sample */
void free_sample( esd_sample_t *sample )
{
    /* free any memory allocated with the sample */
    free( sample->data_buffer );

    /* TODO: free sample players with this id */

    /* free the sample memory itself */
    free( sample );
    printf( "freed sample: [0x%p]\n", sample );
    return;
}

/*******************************************************************/
/* add a complete new client into the list of samples at head */
void add_sample( esd_sample_t *new_sample )
{
    /* printf ( "adding sample 0x%p\n", new_sample ); */
    new_sample->next = esd_samples_list;
    esd_samples_list = new_sample;
    return;
}

/*******************************************************************/
/* erase a sample from the sample list */
/* TODO: add "force kill" boolean option */
void erase_sample( int id )
{
    esd_sample_t *previous = NULL;
    esd_sample_t *current = esd_samples_list;

    printf( "erasing sample (%d)\n", id );

    /* iterate until we hit a NULL */
    while ( current != NULL )
    {
	/* see if we hit the target sample */
	if ( current->sample_id == id ) {

	    /* if the ref count is non-zero, just flag it for deletion */
	    if ( current->ref_count ) {
		printf( "erasing sample (%d) - deferred\n", id );
		current->erase_when_done = 1;
		return;
	    }

	    /* ref_count is zero, get rid of it */
	    if ( previous != NULL ){
		/* we are deleting in the middle of the list */
		previous->next = current->next;
	    } else { 
		/* we are deleting the head of the list */
		esd_samples_list = current->next;
	    }

	    /* erase last traces of sample from existence */
	    free_sample( current );

	    return;
	}

	/* iterate through the list */
	previous = current;
	current = current->next;
    }

    /* hmm, we didn't find the desired sample, just get on with life */
    printf( "sample not found (%d)\n", id );
    return;
}

/*******************************************************************/
/* allocate and initialize a sample from client stream */
#define min(a,b) ( ( (a)<(b) ) ? (a) : (b) )
int read_sample( esd_sample_t *sample )
{
    int actual = -1, total = 0;

    while ( ( total < sample->sample_length ) && ( actual != 0 ) ) {
	actual = read( sample->parent->fd, sample->data_buffer + total,
		       min( ESD_BUF_SIZE, sample->sample_length-total) );
	total += actual;
    }

    /* TODO: what if total != sample_length ? */
    printf( "%d bytes total\n", total );
    return total;
}

/*******************************************************************/
/* allocate and initialize a sample from client stream */
esd_sample_t *new_sample( int client_fd )
{
    esd_sample_t *sample;

    /* make sure we have the memory to save the client... */
    sample = (esd_sample_t*) malloc( sizeof(esd_sample_t) );
    if ( sample == NULL ) {
	return NULL;
    }
    
    /* and initialize the sample */
    sample->next = NULL;
    sample->parent = NULL;
    read( client_fd, &sample->format, sizeof(sample->format) );
    read( client_fd, &sample->rate, sizeof(sample->rate) );
    read( client_fd, &sample->sample_length, sizeof(sample->sample_length) );
    read( client_fd, sample->name, ESD_NAME_MAX );
    sample->name[ ESD_NAME_MAX - 1 ] = '\0';

    sample->sample_id = esd_next_sample_id++;

    printf( "sample %s: 0x%08x at %d Hz\n", sample->name, 
	    sample->format, sample->rate );

    /* force to an even multiple of 4, do it in the player */
    sample->data_buffer
	= (void *) malloc( sample->sample_length );

    /* if not enough room for data buffer, clean up, and return NULL */
    if ( sample->data_buffer == NULL ) {
	free( sample );
	return NULL;
    }

    /* set ref. count */
    sample->ref_count = 0;

    printf( "sample: <%d> [0x%p] - %d bytes\n", 
	    sample->sample_id, sample, sample->sample_length );
    write( client_fd, &sample->sample_id, sizeof(sample->sample_id) );

    return sample;
}

/*******************************************************************/
/* spawn a player for this sample */
int play_sample( int sample_id, int loop )
{
    esd_player_t *player = NULL;

    player = new_sample_player( sample_id, loop );
    if ( player == NULL )
	return 0;

    add_player( player );
    return 1;
}

/*******************************************************************/
/*  stopa sample from the sample list */
int stop_sample( int id )
{
    esd_player_t *player = esd_players_list;

    printf( "stopping sample (%d)\n", id );

    /* iterate until we hit a NULL */
    while ( player != NULL )
    {
	printf( "checking player [0x%p], format = 0x%08x, id = %d\n",
		player, player->format, player->source_id );

	/* see if we hit the target sample, and it's really a sample */
	if ( ( ( (player->format) & ESD_MASK_MODE ) == ESD_SAMPLE )
	     && ( player->source_id == id ) ) {

	    /* remove the loop setting on the sample */
	    player->format &= ~ESD_MASK_FUNC;
	    player->format |= ESD_STOP;

	    printf( "found sample (%d), prepared for removal - 0x%08x\n",
		    id, player->format );
	    
	    return 1;
	}

	/* iterate through the list */
	player = player->next;
    }

    /* hmm, we didn't find the desired sample, just get on with life */
    printf( "player for sample <%d> not found\n", id );
    return 0;
}

