
#include <esd.h>
#include <stdio.h>

/*********************************************************************/
/* print server into to stdout */
void esd_print_server_info( esd_server_info_t *server_info )
{
    printf( "server info:\n" );
    printf( "- version = %d\n", server_info->version );
    printf( "- format  = 0x%08x\n", server_info->format );
    printf( "- rate    = %d\n", server_info->rate );
    return;
}

/*********************************************************************/
/* print all info to stdout */
void esd_print_all_info( esd_info_t *all_info )
{
    esd_player_info_t *player_info;
    esd_sample_info_t *sample_info;

    /* dump server info */
    printf( "server info:\n" );
    printf( "- version = %d\n", all_info->server->version );
    printf( "- format  = 0x%08x\n", all_info->server->format );
    printf( "- rate    = %d\n", all_info->server->rate );

    /* dump player info */
    for ( player_info = all_info->player_list 
	      ; player_info != NULL ; player_info = player_info->next )
    {
	    printf( "player %d: %s\n", player_info->source_id, player_info->name );
	    printf( "- format  = 0x%08x\n", player_info->format );
	    printf( "- rate    = %d\n", player_info->rate );
    }

    /* dump sample info */
    for ( sample_info = all_info->sample_list 
	      ; sample_info != NULL ; sample_info = sample_info->next )
    {
	    printf( "sample %d: %s\n", sample_info->sample_id, sample_info->name );
	    printf( "- format  = 0x%08x\n", sample_info->format );
	    printf( "- rate    = %d\n", sample_info->rate );
	    printf( "- length  = %d\n", sample_info->length );
    }

    return;
}

/*********************************************************************/
/* retrieve server properties (sample rate, format, version number) */
esd_server_info_t *esd_get_server_info( int esd )
{
    int proto = ESD_PROTO_SERVER_INFO;
    int ok = 0;

    /* allocate the server info structure */
    esd_server_info_t *server_info 
	= (esd_server_info_t *) malloc( sizeof(esd_server_info_t) );
    if ( !server_info ) return server_info;

    /* tell the server to cough up the info */
    if ( write( esd, &proto, sizeof(proto) ) != sizeof(proto) ) {
	free( server_info );
	return NULL;
    }

    /* get the info from the server */
    read( esd, &server_info->version, sizeof(server_info->version) );
    read( esd, &server_info->rate, sizeof(server_info->rate) );
    if ( read( esd, &server_info->format, sizeof(server_info->format) )
	 != sizeof(server_info->format) ) {
	free( server_info );
	return NULL;
    }

    return server_info;
}

/*********************************************************************/
/* release all memory allocated for the server properties structure */
void esd_free_server_info( esd_server_info_t *server_info )
{
    free( server_info );
    return;
}

/*********************************************************************/
/* retrieve all information from server */
esd_info_t *esd_get_all_info( int esd )
{
    int proto = ESD_PROTO_ALL_INFO;
    int ok = 0;
    esd_server_info_t *server_info;
    esd_player_info_t *player_info;
    esd_sample_info_t *sample_info;

    /* allocate the entire info structure, and set defaults to NULL */
    esd_info_t *info = (esd_info_t *) malloc( sizeof(esd_info_t) );
    info->player_list = NULL;
    info->sample_list = NULL;

    /* allocate the server info structure */
    server_info = (esd_server_info_t *) malloc( sizeof(esd_server_info_t) );
    if ( !server_info ) return NULL;

    /* tell the server to cough up the info */
    if ( write( esd, &proto, sizeof(proto) ) != sizeof(proto) ) {
	free( server_info );
	return NULL;
    }

    /* get the server info */
    read( esd, &server_info->version, sizeof(server_info->version) );
    read( esd, &server_info->rate, sizeof(server_info->rate) );
    if ( read( esd, &server_info->format, sizeof(server_info->format) )
	 != sizeof(server_info->format) ) {
	free( server_info );
	return NULL;
    }
    info->server = server_info;

    /* get the player info */
    do 
    {
	player_info = (esd_player_info_t *) malloc( sizeof(esd_player_info_t) );
	if ( !player_info ) {
	    esd_free_all_info( info );
	    return NULL;
	}

	read( esd, &player_info->source_id, sizeof(player_info->source_id) );
	read( esd, &player_info->name, ESD_NAME_MAX );
	player_info->name[ ESD_NAME_MAX - 1 ] = '\0';
	read( esd, &player_info->rate, sizeof(player_info->rate) );
	if ( read( esd, &player_info->format, sizeof(player_info->format) )
	     != sizeof(player_info->format) ) {
	    free( player_info );
	    esd_free_all_info( info );	    
	    return NULL;
	}
	
	if ( player_info->source_id > 0 ) {
	    player_info->next = info->player_list;
	    info->player_list = player_info;
	}

    } while( player_info->source_id > 0 );

    /* get the sample info */
    do 
    {
	sample_info = (esd_sample_info_t *) malloc( sizeof(esd_sample_info_t) );
	if ( !sample_info ) {
	    esd_free_all_info( info );
	    return NULL;
	}

	read( esd, &sample_info->sample_id, sizeof(sample_info->sample_id) );
	read( esd, &sample_info->name, ESD_NAME_MAX );
	sample_info->name[ ESD_NAME_MAX - 1 ] = '\0';
	read( esd, &sample_info->rate, sizeof(sample_info->rate) );
	read( esd, &sample_info->format, sizeof(sample_info->format) );
	if ( read( esd, &sample_info->length, sizeof(sample_info->length) )
	     != sizeof(sample_info->length) ) {
	    free( sample_info );
	    esd_free_all_info( info );	    
	    return NULL;
	}
	
	if ( sample_info->sample_id > 0 ) {
	    sample_info->next = info->sample_list;
	    info->sample_list = sample_info;
	}

    } while( sample_info->sample_id > 0 );

    return info;
}

/*********************************************************************/
/* retrieve all information from server, and update until unsubsribed or closed */
esd_info_t *esd_subscribe_all_info( int esd )
{
    fprintf( stderr, "- esd_subscribe_all_info: not yet implemented!\n" );
    return NULL;
}

/*********************************************************************/
/* call to update the info structure with new information, and call callbacks */
esd_info_t *esd_update_info( int esd, esd_info_t *info, 
			     esd_update_info_callbacks_t *callbacks )
{
    fprintf( stderr, "- esd_update_info: not yet implemented!\n" );
    return NULL;
}

/*********************************************************************/
/* call to update the info structure with new information, and call callbacks */
esd_info_t *esd_unsubscribe_info( int esd )
{
    fprintf( stderr, "- esd_unsubscribe_info: not yet implemented!\n" );
    return NULL;
}

/*********************************************************************/
/* release all memory allocated for the esd info structure */
void esd_free_all_info( esd_info_t *info )
{
    esd_player_info_t *player_info, *next_player_info;
    esd_sample_info_t *sample_info, *next_sample_info;

    free( info->server );

    player_info = info->player_list;
    while ( player_info != NULL ) {
	next_player_info = player_info->next;
	free( player_info );
	player_info = next_player_info;
    }

    sample_info = info->sample_list;
    while ( sample_info != NULL ) {
	next_sample_info = sample_info->next;
	free( sample_info );
	sample_info = next_sample_info;
    }

    free( info );
    return;
}
