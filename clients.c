
#include "esd-server.h"

/*******************************************************************/
/* globals */

/* the list of the currently connected clients */
esd_client_t *esd_clients_list;

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

	    /* fill in the new_client structure - sockaddr = works!? */
	    /* request = ..._INVALID forces polling client next time */
	    new_client->next = NULL;
	    new_client->state = ESD_NEEDS_VALIDATION;
	    new_client->request = ESD_PROTO_INVALID;
	    new_client->fd = fd;
	    new_client->source = incoming; 
	    
	    add_new_client( new_client );
	}
    } while ( fd > 0 );

    return 0;
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

    /* if we're doing something useful, make sure we return immediately */
    if ( esd_players_list || esd_recorder ) {
	timeout.tv_sec = 0;
	timeout.tv_usec = 0;
	timeout_ptr = &timeout;
    } else {
	/* we might not be doing something useful, kill audio echos */
	esd_audio_pause();
    }

    select( max_fd+1, &rd_fds, NULL, NULL, timeout_ptr );

    return 0;
}
