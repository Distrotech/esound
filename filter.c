#include "esd-server.h"

esd_player_t *esd_filter_list = NULL;

/*******************************************************************/
/* send the filter's buffer to it's associated socket, erase if EOF */
int filter_write( void *buffer, int size, esd_format_t format, int rate )
{
    int actual=0, total_data=0, data_size=0, data_rate=0;
    void *data_buffer=NULL;
    esd_player_t *filter=esd_filter_list, *erase=NULL;
    esd_format_t data_format;

    data_size = size;
    data_buffer = buffer;
    data_format = format;
    data_rate = rate;
    
    /* hop through the list of filters */
    while( filter ) {
	/* fprintf( stderr, "filter_write: writing to new filter...\n" ); */
	
	/* write the data data_buffer to the socket */
	total_data = 0;
	actual = 0;
	erase = NULL;
	
	/* All of the data from the previous read has to be written.
	 * There's no way I can think of to get around this w/o losing data.
	 * Maybe by using fread and fwrite, to buffer the stuff, but in the 
	 * end, all the data still has to be written from here to the buffer.
	 */
	data_size = mix_and_copy( filter->data_buffer, 
				filter->buffer_length, filter->rate, 
				filter->format, data_buffer, data_size, 
				data_rate, data_format );

	while( total_data < data_size )
	{
	    ESD_WRITE_BIN( filter->source_id, filter->data_buffer + total_data, data_size - total_data, actual, "mod wr" );
	    
	    if ( actual <= 0 ) {
		erase = filter;
		total_data = data_size;
	    } else {
		total_data += actual;
	    }
	    /* fprintf( stderr, "filter_write: just wrote %d bytes\n", actual ); */
	}
	
	if( erase == NULL ) {
	    /* read the client sound data */
	    actual = read_player( filter );

	    /* read_player(): >0 = data, ==0 = no data, <0 = erase it */
	    if ( actual > 0  ) {
		/* printf( "received: %d bytes from %d\n", 
		    actual, filter->source_id ); */
		data_buffer = filter->data_buffer;
		data_size = filter->actual_length;
		data_format = filter->format;
		data_rate = filter->rate;
	    } else if ( actual == 0 ) {
		if ( esdbg_trace) 
		    printf( "no data available from filter (%d)\n", 
			filter->source_id, filter ); 
		data_buffer = filter->data_buffer;
		data_size = 0;
		data_format = filter->format;
		data_rate = filter->rate;
	    } else {
		/* actual < 0 means erase the player */
		erase = filter;
	    }
	}
	
	filter = filter->next;
	
	/* clean up any finished filters */
	if ( erase != NULL ) {
	    erase_filter( erase );
	    erase = NULL;
	}
    }
    
    if( data_buffer != buffer )
    {
	/* memcpy( buffer, data_buffer, data_size ); */
	data_size = mix_and_copy( buffer, size, rate, format, 
			data_buffer, data_size, data_rate, data_format );
    }
    
    return data_size;
}

/*******************************************************************/
/* erase a filter from the filter list */
void erase_filter( esd_player_t *filter )
{
    esd_player_t *previous = NULL;
    esd_player_t *current = esd_filter_list;

    /* iterate until we hit a NULL */
    while ( current != NULL )
    {
	/* see if we hit the target filter */
	if ( current == filter ) {
	    if( previous != NULL ){
		/* we are deleting in the middle of the list */
		previous->next = current->next;
	    } else { 
		/* we are deleting the head of the list */
		esd_filter_list = current->next;
	    }

	    /* TODO: delete if needed */
	    free_player( filter );

	    return;
	}

	/* iterate through the list */
	previous = current;
	current = current->next;
    }

    /* hmm, we didn't find the desired filter, just get on with life */
    if ( esdbg_trace ) printf( "-%02d- filter not found\n", filter->source_id );
    return;
}


