#include "esd-server.h"

#include <limits.h>

/*******************************************************************/
/* ESD_BUF_SIZE is the maximum possible number of samples */
signed int mixed_buffer[ ESD_BUF_SIZE ];

/* TODO: straighten out the mix algorithm comment annotations */
/* TOTO: i don't think we're in kansas anymore... */

/* decides which of the mix_from_* functions to use, and calls it */
int mix_and_copy( void *dest_buf, int dest_len, 
	int dest_rate, esd_format_t dest_format, 
	void *source_data, int src_len, 
	int src_rate, esd_format_t src_format )
{
	if ( ( src_format & ESD_MASK_CHAN ) == ESD_MONO ) {
	    if ( (src_format & ESD_MASK_BITS) == ESD_BITS16 ) 
		return mix_from_mono_16s( dest_buf, dest_len, 
			dest_rate, dest_format, source_data, src_len,
			src_rate );
	    else
		return mix_from_mono_8u( dest_buf, dest_len, 
			dest_rate, dest_format, source_data, src_len,
			src_rate );
	} else {
	    if ( (src_format & ESD_MASK_BITS) == ESD_BITS16 ) 
		return mix_from_stereo_16s( dest_buf, dest_len, 
			dest_rate, dest_format, source_data, src_len,
			src_rate );
	    else
		return mix_from_stereo_8u( dest_buf, dest_len, 
			dest_rate, dest_format, source_data, src_len,
			src_rate );
	}
	return 0;
}

/*******************************************************************/
/* takes the 16 bit signed source waveform, and mixes to player */
int mix_from_stereo_16s( void *dest_buf, int dest_len, int dest_rate, esd_format_t dest_format, 
			signed short *source_data_ss, int src_len, int src_rate )
{
    int wr_dat = 0, rd_dat = 0, bytes_written = 0;
    register unsigned char *target_data_uc = NULL;
    register signed short *target_data_ss = NULL;
    signed short lsample, rsample;

    /* if nothing to mix, just bail */
    if ( !src_len ) {
	return 0;
    }

    /* mix it down */
    switch ( dest_format & ESD_MASK_BITS )
    {
    case ESD_BITS8:
	target_data_uc = (unsigned char *) dest_buf;

	if ( ( dest_format & ESD_MASK_CHAN ) == ESD_MONO ) {

	    /* mix mono, 8 bit sound source from stereo, 16 bit */
	    while ( wr_dat < dest_len )
	    {
		rd_dat = wr_dat * src_rate / dest_rate;
		rd_dat *= 2;		/* adjust for mono */
		rd_dat += rd_dat % 2;	/* force to left sample */

		lsample = source_data_ss[ rd_dat++ ];
		rsample = source_data_ss[ rd_dat++ ];

		lsample /= 256; lsample += 127;
		rsample /= 256; rsample += 127;

		target_data_uc[ wr_dat++ ] = (lsample + rsample) / 2;
	    }

	} else {

	    /* mix stereo, 8 bit sound source from stereo, 16 bit */
	    while ( wr_dat < dest_len )
	    {
		rd_dat = wr_dat * src_rate / dest_rate;

		lsample = source_data_ss[ rd_dat++ ];
		lsample /= 256; lsample += 127;
		rsample = source_data_ss[ rd_dat++ ];
		rsample /= 256; rsample += 127;

		target_data_uc[ wr_dat++ ] = lsample;
		target_data_uc[ wr_dat++ ] = rsample;
	    }
	}

	bytes_written = wr_dat * sizeof(unsigned char);
	break;

    case ESD_BITS16:
	target_data_ss = (signed short *) dest_buf;

	if ( ( dest_format & ESD_MASK_CHAN ) == ESD_MONO ) {

	    /* mix mono, 16 bit sound source from stereo, 16 bit */
	    while ( wr_dat < dest_len / sizeof(signed short) )
	    {
		rd_dat = wr_dat * src_rate / dest_rate;
		rd_dat *= 2;		/* adjust for stereo */

		lsample = source_data_ss[ rd_dat++ ];
		rsample = source_data_ss[ rd_dat++ ];

		target_data_ss[ wr_dat++ ] = (lsample + rsample) / 2;
	    }

	} else {

	    /* mix stereo, 16 bit sound source from stereo, 16 bit */

	    /* optimize for the case where all settings are the same */
	    if ( dest_rate == src_rate ) {
		memcpy( target_data_ss, source_data_ss, dest_len );
		bytes_written = dest_len;
		break;
	    }

	    /* scale the pointer, and copy the data */
	    while ( wr_dat < dest_len / sizeof(signed short) )
	    {
		rd_dat = wr_dat * src_rate / dest_rate;

		lsample = source_data_ss[ rd_dat++ ];
		rsample = source_data_ss[ rd_dat++ ];

		target_data_ss[ wr_dat++ ] = lsample;
		target_data_ss[ wr_dat++ ] = rsample;
	    }
	}

	bytes_written = wr_dat * sizeof(signed short);
	break;

    default:
	fprintf( stderr, "mix from 16s: format 0x%08x not supported\n", 
		 dest_format & ESD_MASK_BITS );
	break;
    }

    return bytes_written;
}

/*******************************************************************/
/* takes the 8 bit unsigned source waveform, and mixes to player */
int mix_from_stereo_8u( void *dest_buf, int dest_len, int dest_rate, esd_format_t dest_format, 
			unsigned char *source_data_uc, int src_len, int src_rate )
{
    int wr_dat = 0, rd_dat = 0, bytes_written = 0;
    register unsigned char *target_data_uc = NULL;
    register signed short *target_data_ss = NULL;
    signed short lsample, rsample;

    /* if nothing to mix, just bail */
    if ( !src_len ) {
	return 0;
    }

    /* mix it down */
    switch ( dest_format & ESD_MASK_BITS )
    {
    case ESD_BITS8:
	target_data_uc = (unsigned char *) dest_buf;

	if ( ( dest_format & ESD_MASK_CHAN ) == ESD_MONO ) {

	    /* mix mono, 8 bit sound source from stereo, 16 bit */
	    while ( wr_dat < dest_len )
	    {
		rd_dat = wr_dat * src_rate / dest_rate;
		rd_dat *= 2;		/* adjust for mono */

		lsample = source_data_uc[ rd_dat++ ];
		rsample = source_data_uc[ rd_dat++ ];

		target_data_uc[ wr_dat++ ] = (lsample + rsample) / 2;
	    }

	} else {

	    /* mix stereo, 8 bit sound source from stereo, 16 bit */

	    /* optimize for the case where all settings are the same */
	    if ( dest_rate == src_rate ) {
		memcpy( target_data_uc, source_data_uc, dest_len );
		bytes_written = dest_len;
		break;
	    }

	    /* scale the pointer, and copy the data */
	    while ( wr_dat < dest_len )
	    {
		rd_dat = wr_dat * src_rate / dest_rate;

		lsample = source_data_uc[ rd_dat++ ];
		rsample = source_data_uc[ rd_dat++ ];

		target_data_uc[ wr_dat++ ] = lsample;
		target_data_uc[ wr_dat++ ] = rsample;
	    }
	}

	bytes_written = wr_dat * sizeof(unsigned char);
	break;

    case ESD_BITS16:
	target_data_ss = (signed short *) dest_buf;

	if ( ( dest_format & ESD_MASK_CHAN ) == ESD_MONO ) {

	    /* mix mono, 16 bit sound source from stereo, 8 bit */
	    while ( wr_dat < dest_len / sizeof(signed short) )
	    {
		rd_dat = wr_dat * src_rate / dest_rate;
		rd_dat *= 2;		/* adjust for stereo */

		lsample = source_data_uc[ rd_dat++ ] - 127;
		rsample = source_data_uc[ rd_dat++ ] - 127;

		target_data_ss[ wr_dat++ ] = (lsample + rsample) * 256 / 2;
	    }

	} else {

	    /* mix stereo, 16 bit sound source from stereo, 8 bit */
	    while ( wr_dat < dest_len / sizeof(signed short) )
	    {
		rd_dat = wr_dat * src_rate / dest_rate;

		lsample = source_data_uc[ rd_dat++ ] - 127;
		rsample = source_data_uc[ rd_dat++ ] - 127;

		target_data_ss[ wr_dat++ ] = lsample * 256;
		target_data_ss[ wr_dat++ ] = rsample * 256;
	    }
	}

	bytes_written = wr_dat * sizeof(signed short);
	break;

    default:
	fprintf( stderr, "mix from 8u: format 0x%08x not supported\n", 
		 dest_format & ESD_MASK_BITS );
	break;
    }

    return bytes_written;
}

/*******************************************************************/
/* takes the 16 bit mono signed source waveform, and mixes to player */
int mix_from_mono_16s( void *dest_buf, int dest_len, int dest_rate, esd_format_t dest_format, 
			signed short *source_data_ss, int src_len, int src_rate )
{
    int wr_dat = 0, rd_dat = 0, bytes_written = 0;
    register unsigned char *target_data_uc = NULL;
    register signed short *target_data_ss = NULL;
    signed short lsample;

    /* if nothing to mix, just bail */
    if ( !src_len ) {
	return 0;
    }

    /* mix it down */
    switch ( dest_format & ESD_MASK_BITS )
    {
    case ESD_BITS8:
	target_data_uc = (unsigned char *) dest_buf;

	if ( ( dest_format & ESD_MASK_CHAN ) == ESD_MONO ) {

	    /* mix mono, 8 bit sound source from mono, 16 bit */
	    while ( wr_dat < dest_len )
	    {
		rd_dat = wr_dat * src_rate / dest_rate;

		lsample = source_data_ss[ rd_dat ];
		lsample /= 256; lsample += 127;

		target_data_uc[ wr_dat++ ] = lsample;
	    }

	} else {

	    /* mix mono, 8 bit sound source from stereo, 8 bit */
	    while ( wr_dat < dest_len )
	    {
		rd_dat = wr_dat * src_rate / dest_rate;
		rd_dat /= 2;

		lsample = source_data_ss[ rd_dat++ ];
		lsample /= 256; lsample += 127;

		target_data_uc[ wr_dat++ ] = lsample;
		target_data_uc[ wr_dat++ ] = lsample;
	    }
	}

	bytes_written = wr_dat * sizeof(unsigned char);
	break;

    case ESD_BITS16:
	target_data_ss = (signed short *) dest_buf;

	if ( ( dest_format & ESD_MASK_CHAN ) == ESD_MONO ) {

	    /* mix mono, 16 bit sound source from mono, 16 bit */

	    /* optimize for the case where all settings are the same */
	    if ( dest_rate == src_rate ) {
		memcpy( target_data_ss, source_data_ss, dest_len );
		bytes_written = dest_len;
		break;
	    }

	    /* scale the pointer, and copy the data */
	    while ( wr_dat < dest_len / sizeof(signed short) )
	    {
		rd_dat = wr_dat * src_rate / dest_rate;

		lsample = source_data_ss[ rd_dat ];

		target_data_ss[ wr_dat++ ] = lsample;
	    }

	} else {

	    /* mix stereo, 16 bit sound source from mono, 16 bit */
	    while ( wr_dat < dest_len / sizeof(signed short) )
	    {
		rd_dat = wr_dat * src_rate / dest_rate;
		rd_dat /= 2;

		lsample = source_data_ss[ rd_dat++ ];

		target_data_ss[ wr_dat++ ] = lsample;
		target_data_ss[ wr_dat++ ] = lsample;
	    }
	}

	bytes_written = wr_dat * sizeof(signed short);
	break;

    default:
	fprintf( stderr, "mix from 16s: format 0x%08x not supported\n", 
		 dest_format & ESD_MASK_BITS );
	break;
    }

    return bytes_written;
}

/*******************************************************************/
/* takes the 8 bit mono unsigned source waveform, and mixes to player */
int mix_from_mono_8u( void *dest_buf, int dest_len, int dest_rate, esd_format_t dest_format, 
			unsigned char *source_data_uc, int src_len, int src_rate )
{
    int wr_dat = 0, rd_dat = 0, bytes_written = 0;
    register unsigned char *target_data_uc = NULL;
    register signed short *target_data_ss = NULL;
    signed short lsample;

    /* if nothing to mix, just bail */
    if ( !src_len ) {
	return 0;
    }

    /* mix it down */
    switch ( dest_format & ESD_MASK_BITS )
    {
    case ESD_BITS8:
	target_data_uc = (unsigned char *) dest_buf;

	if ( ( dest_format & ESD_MASK_CHAN ) == ESD_MONO ) {

	    /* mix mono, 8 bit sound source from mono, 8 bit */

	    /* optimize for the case where all settings are the same */
	    if ( dest_rate == src_rate ) {
		memcpy( target_data_uc, source_data_uc, dest_len );
		bytes_written = dest_len;
		break;
	    }

	    /* scale the pointer, and copy the data */
	    while ( wr_dat < dest_len )
	    {
		rd_dat = wr_dat * src_rate / dest_rate;

		lsample = source_data_uc[ rd_dat ];
		target_data_uc[ wr_dat++ ] = lsample;
	    }

	} else {

	    /* mix stereo, 8 bit sound source from mono, 8 bit */
	    while ( wr_dat < dest_len )
	    {
		rd_dat = wr_dat * src_rate / dest_rate;
		rd_dat /= 2;

		lsample = source_data_uc[ rd_dat ];

		target_data_uc[ wr_dat++ ] = lsample;
		target_data_uc[ wr_dat++ ] = lsample;
	    }
	}

	bytes_written = wr_dat * sizeof(unsigned char);
	break;

    case ESD_BITS16:
	target_data_ss = (signed short *) dest_buf;

	if ( ( dest_format & ESD_MASK_CHAN ) == ESD_MONO ) {

	    /* mix mono, 16 bit sound source from mono, 8 bit */
	    while ( wr_dat < dest_len / sizeof(signed short) )
	    {
		rd_dat = wr_dat * src_rate / dest_rate;

		lsample = source_data_uc[ rd_dat ] - 127;
		lsample *= 256;

		target_data_ss[ wr_dat++ ] = lsample;
	    }

	} else {

	    /* mix stereo, 16 bit sound source from mono, 8 bit */
	    while ( wr_dat < dest_len / sizeof(signed short) )
	    {
		rd_dat = wr_dat * src_rate / dest_rate;
		rd_dat /= 2;

		lsample = source_data_uc[ rd_dat++ ] - 127;
		lsample *= 256;

		target_data_ss[ wr_dat++ ] = lsample;
		target_data_ss[ wr_dat++ ] = lsample;
	    }
	}

	bytes_written = wr_dat * sizeof(signed short);
	break;

    default:
	fprintf( stderr, "mix from 8u: format 0x%08x not supported\n", 
		 dest_format & ESD_MASK_BITS );
	break;
    }

    return bytes_written;
}

/*******************************************************************/
/* takes the input player, and mixes to 16 bit signed waveform */
int mix_to_stereo_32s( esd_player_t *player, int length )
{
    int wr_dat = 0, rd_dat = 0, count = 0, step = 0;
    register unsigned char *source_data_uc = NULL;
    register signed short *source_data_ss = NULL;
    signed short sample;

    switch ( player->format & ESD_MASK_BITS )
    {
    case ESD_BITS8:
	source_data_uc = (unsigned char *) player->data_buffer;

	if ( ( player->format & ESD_MASK_CHAN ) == ESD_MONO ) {

	    /* mix mono, 8 bit sound source to stereo, 16 bit */
	    while ( wr_dat < length/sizeof(signed short) )
	    {
		rd_dat = wr_dat * player->rate / esd_audio_rate;
		rd_dat /= 2;	/* adjust for mono */

		sample = source_data_uc[ rd_dat++ ];
		sample -= 127; sample *= 256;

		mixed_buffer[ wr_dat++ ] += sample;
		mixed_buffer[ wr_dat++ ] += sample;
	    }

	} else {

	    /* mix stereo, 8 bit sound source to stereo, 16 bit */
	    if ( player->rate == esd_audio_rate ) {
		while ( wr_dat < length/sizeof(signed short) )
		{
		    sample = ( source_data_uc[ wr_dat ] - 127 ) * 256;
		    mixed_buffer[ wr_dat ] += sample;
		    wr_dat++;
		}
	    } else {
		while ( wr_dat < length/sizeof(signed short) )
		{
		    rd_dat = wr_dat * player->rate / esd_audio_rate;
		    
		    sample = source_data_uc[ rd_dat++ ];
		    sample -= 127; sample *= 256;
		    
		    mixed_buffer[ wr_dat++ ] += sample;
		}
	    }
	}

	break;
    case ESD_BITS16:
	source_data_ss = (signed short *) player->data_buffer;

	/* rough sketch, based on 8 bit stuff */
	if ( ( player->format & ESD_MASK_CHAN ) == ESD_MONO ) {

	    /* mix mono, 16 bit sound source to stereo, 16 bit */
	    while ( wr_dat < length/sizeof(signed short) )
	    {
		rd_dat = wr_dat * player->rate / esd_audio_rate;
		rd_dat /= 2;	/* adjust for mono */

		sample = source_data_ss[ rd_dat++ ];

		mixed_buffer[ wr_dat++ ] += sample;
		mixed_buffer[ wr_dat++ ] += sample;
	    }

	} else {

	    /* mix stereo, 16 bit sound source to stereo, 16 bit */
	    if ( player->rate == esd_audio_rate ) {
		/* optimize for simple increment by one and add loop */
		while ( wr_dat < length/sizeof(signed short) )
		{
		    mixed_buffer[ wr_dat ] += source_data_ss[ wr_dat ];
		    wr_dat++;
		}
	    } else {
		/* non integral multiple of sample rates, do it the hard way */
		while ( wr_dat < length/sizeof(signed short) )
		{
		    rd_dat = wr_dat * player->rate / esd_audio_rate;
		    sample = source_data_ss[ rd_dat++ ];
		    mixed_buffer[ wr_dat++ ] += sample;
		}
	    }
	}
	break;

    default:
	fprintf( stderr, "mix_to: format 0x%08x not supported (%d)\n", 
		 player->format & ESD_MASK_BITS, player->source_id );
	break;
    }

    return wr_dat * sizeof(signed short);
}

/*******************************************************************/
/* takes mixed data, and clips data to the output buffer */
void clip_mix_to_output_16s( signed short *output, int length )
{
    signed int *mixed = mixed_buffer;
    signed int *end = mixed_buffer + length/sizeof(signed short);

    while ( mixed != end ) {
	if (*mixed < SHRT_MIN) {
	    *output++ = SHRT_MIN; mixed++;
	} else if (*mixed > SHRT_MAX) {
	    *output++ = SHRT_MAX; mixed++;
	} else {
	    *output++ = *mixed++;
	}
    }
}

/*******************************************************************/
/* takes mixed data, and clips data to the output buffer */
void clip_mix_to_output_8u( signed char *output, int length )
{
    signed int *mixed = mixed_buffer;
    signed int *end = mixed_buffer + length/sizeof(signed short);

    while ( mixed != end ) {
	if (*mixed < SHRT_MIN) {
	    *output++ = 0; mixed++;
	} else if (*mixed > SHRT_MAX) {
	    *output++ = 255; mixed++;
	} else {
	    *output++ = (*mixed++) / 256 + 128;
	}
    }
}

/*******************************************************************/
/* takes all input players, and mixes them to the mixed_buffer */
int mix_players_16s( void *output, int length )
{
    int actual = 0, max = 0;
    esd_player_t *iterator = NULL;
    esd_player_t *erase = NULL;

    /* zero the sum buffer */
    memset( mixed_buffer, 0, esd_buf_size_samples * sizeof(int) );
    
    /* as long as there's a player out there */
    iterator = esd_players_list;
    while( iterator != NULL )
    {
	/* read the client sound data */
	actual = read_player( iterator );

	/* read_player(): >0 = data, ==0 = no data, <0 = erase it */
	if ( actual > 0  ) {
	    /* printf( "received: %d bytes from %d\n", 
	            actual, iterator->source_id ); */
	    actual = mix_to_stereo_32s( iterator, length );
	    if ( actual > max ) max = actual;
	    
	} else if ( actual == 0 ) {
	    ESDBG_TRACE( printf( "no data available from player (%d)\n", 
				 iterator->source_id, iterator ); );
	} else {
	    /* actual < 0 means erase the player */
	    erase = iterator;
	}

	/* check out the next item in the list */
	iterator = iterator->next;

	/* clean up any fished players */
	if ( erase != NULL ) {
	    erase_player( erase );
	    erase = NULL;
	}
    }

    /* if ( esdbg_comms ) printf( "maximum stream length = %d bytes\n", max ); */
    ESDBG_COMMS( printf( "maximum stream length = %d bytes\n", max ); );

    if ( (esd_audio_format & ESD_MASK_BITS) == ESD_BITS16 ) 
	clip_mix_to_output_16s( output, max );
    else {
	clip_mix_to_output_8u( output, max );
	max /= 2; /* half as many samples as you'd think */
    }

    return max;
}
