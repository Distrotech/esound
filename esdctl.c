
#include "esd.h"

int main(int argc, char **argv)
{
    int sock = -1, arg = 0;
  
    /* check number of args */
    if ( argc == 1 ) 
	return 0;

    /* contact the server */
    sock = esd_open_sound( 0, 0 );
    if ( sock <= 0 ) return 1;

    /* control the daemon */
    for ( arg = 1 ; arg < argc ; arg++)
    {
	if ( !strcmp( "lock", argv[arg] ) )
	    esd_lock( sock );
	else if ( !strcmp( "unlock", argv[arg] ) )
	    esd_unlock( sock );
	else if ( !strcmp( "off", argv[arg] ) )
	    esd_standby( sock );
	else if ( !strcmp( "standby", argv[arg] ) )
	    esd_standby( sock );
	else if ( !strcmp( "on", argv[arg] ) )
	    esd_resume( sock );
	else if ( !strcmp( "resume", argv[arg] ) )
	    esd_resume( sock );
	else
	{
	    printf( "unrecognized control word: %s\n", argv[ arg ] );
	    printf( "usage: %s [lock] [unlock] [standby] [resume]\n", argv[0] );
	    printf( "\tlock     - prevent foreign clients from connecting\n" );
	    printf( "\tunlock   - allow foreign clients from connecting\n" );
	    printf( "\tstandby  - free audio device for use by other apps\n" );
	    printf( "\tresume   - reclaim audio device for use by esd\n" );
	    return 1;
	}
    }

    close( sock );
    return 0;
}
