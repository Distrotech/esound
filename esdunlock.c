
#include "esd.h"

int main(int argc, char **argv)
{
    int sock = -1;
    esd_proto_t proto = ESD_PROTO_UNLOCK;
  
    sock = esd_open_sound( 0, 0 );
    if (sock<=0) return 1;

    /* send the audio format information */
    write( sock, &proto, sizeof(proto) );
    esd_send_auth( sock );

    close( sock );
    return 0;
}
