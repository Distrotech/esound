#ifndef ESD_SERVER_H
#define ESD_SERVER_H

/* get public information from the public header file */
#include <esd.h>
/*******************************************************************/
/* server function prototypes */

/* clients.c - manage the client connections */
extern esd_client_t *esd_clients_list;
void dump_clients();
void add_new_client( esd_client_t *new_client );
void erase_client( esd_client_t *client );

void clear_auth( int signum ); /* TODO: sig_clear_auth ? */
int get_new_clients( int listen );
int poll_client_requests();
int wait_for_clients_and_data( int listen );

/* players.c - manage the players, recorder, and monitor */
extern esd_player_t *esd_players_list;
extern esd_player_t *esd_recorder;
extern esd_player_t *esd_monitor;

void dump_players();
void add_player( esd_player_t *player );
void erase_player( esd_player_t *player );
esd_player_t *new_stream_player( int client_fd );
esd_player_t *new_sample_player( int id, int loop );

int read_player( esd_player_t *player );
void recorder_write();
void monitor_write();

/* samples.c - manage the players, recorder, and monitor */
extern esd_sample_t *esd_samples_list;

void dump_samples();
void add_sample( esd_sample_t *sample );
void erase_sample( int id );
esd_sample_t *new_sample( int client_fd );
int read_sample( esd_sample_t *sample );
int play_sample( int sample_id, int loop );
int stop_sample( int sample_id );

/* mix.c - deal with mixing signals, and format conversion */
int mix_from_stereo_16s( signed short *source_data_ss, 
		       esd_player_t *player, int length );
int mix_players_16s( void *mixed, int length );

/*******************************************************************/
/* evil evil macros */

/* switch endian order for cross platform playing */
#define switch_endian_16(x) ( (x)>>8 & ( (x)&0xFF<<8 ) )

#endif /* #ifndef ESD_SERVER_H */
