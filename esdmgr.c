
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
    fprintf( stderr, "- esd_print_all_info: not yet implemented!\n" );
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
    fprintf( stderr, "- esd_get_all_info: not yet implemented!\n" );
    return NULL;
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
void esd_free_esd_info( esd_info_t *info )
{
    free( info );
    return;
}
