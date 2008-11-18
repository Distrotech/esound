#include "esd-server.h"

#include <limits.h>

/*******************************************************************/
signed int *mixed_buffer = NULL;

/* prototype for compiler */
int mix_and_copy( void *dest_buf, int dest_len, 
		  int dest_rate, esd_format_t dest_format, 
		  void *source_data, int src_len, 
		  int src_rate, esd_format_t src_format );

int mix_from_stereo_16s( void *dest_buf, unsigned int dest_len, 
			 int dest_rate, esd_format_t dest_format, 
			 signed short *source_data_ss, int src_len, int src_rate );
int mix_from_stereo_8u( void *dest_buf, unsigned int dest_len, 
			int dest_rate, esd_format_t dest_format, 
			unsigned char *source_data_uc, int src_len, int src_rate );
int mix_from_mono_16s( void *dest_buf, unsigned int dest_len, 
		       int dest_rate, esd_format_t dest_format, 
		       signed short *source_data_ss, int src_len, int src_rate );
int mix_from_mono_8u( void *dest_buf, unsigned int dest_len, 
		      int dest_rate, esd_format_t dest_format, 
		      unsigned char *source_data_uc, int src_len, int src_rate );

int mix_mono_8u_to_stereo_32s_sv( esd_player_t *player, int length );
int mix_stereo_8u_to_stereo_32s_sv( esd_player_t *player, int length );
int mix_mono_16s_to_stereo_32s_sv( esd_player_t *player, int length );
int mix_stereo_16s_to_stereo_32s_sv( esd_player_t *player, int length );

int mix_mono_8u_to_stereo_32s( esd_player_t *player, int length );
int mix_stereo_8u_to_stereo_32s( esd_player_t *player, int length );
int mix_mono_16s_to_stereo_32s( esd_player_t *player, int length );
int mix_stereo_16s_to_stereo_32s( esd_player_t *player, int length );

void clip_mix_to_output_16s( signed short *output, int length );
void clip_mix_to_output_8u( unsigned char *output, int length );

/* TODO: straighten out the mix algorithm comment annotations */
/* TOTO: i don't think we're in kansas anymore... */

mix_func_t get_mix_func( esd_player_t *player )
{
    switch ( player->format & ESD_MASK_BITS )
    {
    case ESD_BITS8:
	if ( ( player->format & ESD_MASK_CHAN ) == ESD_MONO )
	    if ( ( player->left_vol_scale == ESD_VOLUME_BASE )
		 && ( player->right_vol_scale == ESD_VOLUME_BASE ) )
		return (mix_func_t)&mix_mono_8u_to_stereo_32s_sv;
	    else
		return (mix_func_t)&mix_mono_8u_to_stereo_32s;
	else if ( ( player->format & ESD_MASK_CHAN ) == ESD_STEREO )
	    if ( ( player->left_vol_scale == ESD_VOLUME_BASE )
		 && ( player->right_vol_scale == ESD_VOLUME_BASE ) )
		return (mix_func_t)&mix_stereo_8u_to_stereo_32s_sv;
	    else
		return (mix_func_t)&mix_stereo_8u_to_stereo_32s;
	else
	    return NULL;
    case ESD_BITS16:
	if ( ( player->format & ESD_MASK_CHAN ) == ESD_MONO )
	    if ( ( player->left_vol_scale == ESD_VOLUME_BASE )
		 && ( player->right_vol_scale == ESD_VOLUME_BASE ) )
		return (mix_func_t)&mix_mono_16s_to_stereo_32s_sv;
	    else
		return (mix_func_t)&mix_mono_16s_to_stereo_32s;
	else if ( ( player->format & ESD_MASK_CHAN ) == ESD_STEREO )
	    if ( ( player->left_vol_scale == ESD_VOLUME_BASE )
		 && ( player->right_vol_scale == ESD_VOLUME_BASE ) )
		return (mix_func_t)&mix_stereo_16s_to_stereo_32s_sv;
	    else {
		return (mix_func_t)&mix_stereo_16s_to_stereo_32s;
	    }
	else
	    return NULL;
    default:
	return NULL;
    }
}

translate_func_t get_translate_func( esd_format_t src_fmt, int src_rate, 
				     esd_format_t dst_fmt, int dst_rate )
{
    if ( ( src_fmt & ESD_MASK_CHAN ) == ESD_MONO ) {
	if ( (src_fmt & ESD_MASK_BITS) == ESD_BITS16 ) 
	    return (translate_func_t)&mix_from_mono_16s;
	else
	    return (translate_func_t)&mix_from_mono_8u;
    } else {
	if ( (src_fmt & ESD_MASK_BITS) == ESD_BITS16 ) 
	    return (translate_func_t)&mix_from_stereo_16s;
	else
	    return (translate_func_t)&mix_from_stereo_8u;
    }
    
    return NULL;
}

/* decides which of the mix_from_* functions to use, and calls it */
int mix_and_copy( void *dest_buf, int dest_len, 
		  int dest_rate, esd_format_t dest_format, 
		  void *source_data, int src_len, 
		  int src_rate, esd_format_t src_format )
{
    if ( ( src_format & ESD_MASK_CHAN ) == ESD_MONO ) {
	if ( (src_format & ESD_MASK_BITS) == ESD_BITS16 ) 
	    return mix_from_mono_16s( dest_buf, dest_len, 
				      dest_rate, dest_format, 
				      source_data, src_len,
				      src_rate );
	else
	    return mix_from_mono_8u( dest_buf, dest_len, 
				     dest_rate, dest_format, 
				     source_data, src_len,
				     src_rate );
    } else {
	if ( (src_format & ESD_MASK_BITS) == ESD_BITS16 ) 
	    return mix_from_stereo_16s( dest_buf, dest_len, 
					dest_rate, dest_format, 
					source_data, src_len,
					src_rate );
	else
	    return mix_from_stereo_8u( dest_buf, dest_len, 
				       dest_rate, dest_format, 
				       source_data, src_len,
				       src_rate );
    }

    return 0;
}

/*******************************************************************/
/* takes the 16 bit signed source waveform, and mixes to player */
int mix_from_stereo_16s( void *dest_buf, unsigned int dest_len, 
			 int dest_rate, esd_format_t dest_format, 
			 signed short *source_data_ss, int src_len, int src_rate )
{
    int rd_dat = 0, bytes_written = 0;
    unsigned int wr_dat = 0;
    register unsigned char *target_data_uc = NULL;
    register signed short *target_data_ss = NULL;
    signed short lsample, rsample;

    ESDBG_MIXER( printf( "mixing from stereo 16s\n" ); );

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
		/* just multipled by two, it's on an even byte */
		/* rd_dat += rd_dat % 2;*/	/* force to left sample */

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
		rd_dat &= ~1;

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
		rd_dat &= ~1;

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
int mix_from_stereo_8u( void *dest_buf, unsigned int dest_len, 
			int dest_rate, esd_format_t dest_format, 
			unsigned char *source_data_uc, int src_len, int src_rate )
{
    int rd_dat = 0, bytes_written = 0;
    unsigned int wr_dat = 0;
    register unsigned char *target_data_uc = NULL;
    register signed short *target_data_ss = NULL;
    signed short lsample, rsample;

    ESDBG_MIXER( printf( "mixing from stereo 8u\n" ); );

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
		rd_dat &= ~1;

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
		rd_dat &= ~1;

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
int mix_from_mono_16s( void *dest_buf, unsigned int dest_len, 
		       int dest_rate, esd_format_t dest_format, 
		       signed short *source_data_ss, int src_len, int src_rate )
{
    int rd_dat = 0, bytes_written = 0;
    unsigned int wr_dat = 0;
    register unsigned char *target_data_uc = NULL;
    register signed short *target_data_ss = NULL;
    signed short lsample;

    ESDBG_MIXER( printf( "mixing from mono 16s\n" ); );

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
int mix_from_mono_8u( void *dest_buf, unsigned int dest_len, 
		      int dest_rate, esd_format_t dest_format, 
		      unsigned char *source_data_uc, int src_len, int src_rate )
{
    int rd_dat = 0, bytes_written = 0;
    unsigned int wr_dat = 0;
    register unsigned char *target_data_uc = NULL;
    register signed short *target_data_ss = NULL;
    signed short lsample;

    ESDBG_MIXER( printf( "mixing from mono 8u\n" ); );

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

int mix_mono_8u_to_stereo_32s_sv( esd_player_t *player, int length )
{
    int rd_dat = 0;
    unsigned int wr_dat=0;
    signed short sample;
    register unsigned char *source_data_uc 
	= (unsigned char *) player->data_buffer;

    ESDBG_MIXER( printf( "mixing mono 8u to stereo 32s\n" ); );

    while ( wr_dat < length/sizeof(signed short) )
    {
	rd_dat = wr_dat * player->rate / esd_audio_rate;
	rd_dat /= 2;	/* adjust for mono */
	
	sample = source_data_uc[ rd_dat ];
	sample -= 127; sample *= 256;
	
	mixed_buffer[ wr_dat++ ] += sample;
	mixed_buffer[ wr_dat++ ] += sample;
    }
    
    return wr_dat * sizeof(signed short);
}

int mix_stereo_8u_to_stereo_32s_sv( esd_player_t *player, int length )
{
    int rd_dat = 0;
    unsigned int wr_dat=0;
    signed short sample;
    register unsigned char *source_data_uc 
	= (unsigned char *) player->data_buffer;

    ESDBG_MIXER( printf( "mixing stereo 8u to stereo 32s\n" ); );

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
	    rd_dat = (wr_dat * player->rate / esd_audio_rate) &0xFFFFFFFE;
	    
	    sample = source_data_uc[ rd_dat ];
            sample -= 127; sample *= 256;
	    mixed_buffer[ wr_dat++ ] += sample;

	    sample = source_data_uc[ rd_dat+1 ];
	    sample -= 127; sample *= 256;   
	    mixed_buffer[ wr_dat++ ] += sample;
	}
    }
    
    return wr_dat * sizeof(signed short);
}    

int mix_mono_16s_to_stereo_32s_sv( esd_player_t *player, int length )
{
    int rd_dat = 0;
    unsigned int wr_dat=0;
    signed short sample;
    register signed short *source_data_ss
	= (signed short *) player->data_buffer;

    ESDBG_MIXER( printf( "mixing mono 16s to stereo 32s\n" ); );

    /* mix mono, 16 bit sound source to stereo, 16 bit */
    while ( wr_dat < length/sizeof(signed short) )
    {
	rd_dat = wr_dat * player->rate / esd_audio_rate;
	rd_dat /= 2;	/* adjust for mono */
	
	sample = source_data_ss[ rd_dat ];
	
	mixed_buffer[ wr_dat++ ] += sample;
	mixed_buffer[ wr_dat++ ] += sample;
    }
    return wr_dat * sizeof(signed short);
}	

int mix_stereo_16s_to_stereo_32s_sv( esd_player_t *player, int length )
{
    int rd_dat = 0;
    unsigned int wr_dat=0;
    signed short sample;
    register signed short *source_data_ss
	= (signed short *) player->data_buffer;

    ESDBG_MIXER( printf( "mixing stereo 16s to stereo 32s length %d\n", length ); );

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
	    rd_dat &= ~1;

	    sample = source_data_ss[ rd_dat ];
	    mixed_buffer[ wr_dat ] += sample;
	    wr_dat++;
	    rd_dat++;

	    sample = source_data_ss[ rd_dat++ ];
	    mixed_buffer[ wr_dat ] += sample;
	    wr_dat++;
	}
    }
 
    return wr_dat * sizeof(signed short);
}

/*******************************************************************/
/* take input player, mix to 16 bit signed waveform, volume control */

int mix_mono_8u_to_stereo_32s( esd_player_t *player, int length )
{
    int rd_dat = 0;
    unsigned int wr_dat=0;
    signed short sample;
    register unsigned char *source_data_uc 
	= (unsigned char *) player->data_buffer;

    ESDBG_MIXER( printf( "mixing mono 8u to stereo 32s\n" ); );

    while ( wr_dat < length/sizeof(signed short) )
    {
	rd_dat = wr_dat * player->rate / esd_audio_rate;
	rd_dat /= 2;	/* adjust for mono */
	
	sample = source_data_uc[ rd_dat ];
	sample -= 127; sample *= 256;
	
	sample = sample * player->left_vol_scale / ESD_VOLUME_BASE;
	sample = sample * player->right_vol_scale / ESD_VOLUME_BASE;

	mixed_buffer[ wr_dat++ ] += sample;
	mixed_buffer[ wr_dat++ ] += sample;
    }
    
    return wr_dat * sizeof(signed short);
}

int mix_stereo_8u_to_stereo_32s( esd_player_t *player, int length )
{
    int rd_dat = 0;
    unsigned int wr_dat=0;
    signed short sample;
    register unsigned char *source_data_uc 
	= (unsigned char *) player->data_buffer;

    ESDBG_MIXER( printf( "mixing stereo 8u to stereo 32s\n" ); );

    if ( player->rate == esd_audio_rate ) {
	while ( wr_dat < length/sizeof(signed short) )
	{
	    sample = ( source_data_uc[ wr_dat ] - 127 ) * 256;
	    sample = sample * player->left_vol_scale / ESD_VOLUME_BASE;
	    mixed_buffer[ wr_dat++ ] += sample;

	    sample = ( source_data_uc[ wr_dat ] - 127 ) * 256;
	    sample = sample * player->right_vol_scale / ESD_VOLUME_BASE;
	    mixed_buffer[ wr_dat++ ] += sample;
	}
    } else {
	while ( wr_dat < length/sizeof(signed short) )
	{
	    rd_dat = wr_dat * player->rate / esd_audio_rate;
	    rd_dat &= ~1;

	    sample = source_data_uc[ rd_dat++ ];
	    sample -= 127; sample *= 256;
	    sample = sample * player->left_vol_scale / ESD_VOLUME_BASE;
	    mixed_buffer[ wr_dat++ ] += sample;

	    sample = source_data_uc[ rd_dat ];
	    sample -= 127; sample *= 256;
	    sample = sample * player->right_vol_scale / ESD_VOLUME_BASE;
	    mixed_buffer[ wr_dat++ ] += sample;
	}
    }
    
    return wr_dat * sizeof(signed short);
}    

int mix_mono_16s_to_stereo_32s( esd_player_t *player, int length )
{
    int rd_dat = 0;
    unsigned int wr_dat=0;
    signed short sample;
    register signed short *source_data_ss
	= (signed short *) player->data_buffer;

    ESDBG_MIXER( printf( "mixing mono 16s to stereo 32s\n" ); );

    /* mix mono, 16 bit sound source to stereo, 16 bit */
    while ( wr_dat < length/sizeof(signed short) )
    {
	rd_dat = wr_dat * player->rate / esd_audio_rate;
	rd_dat /= 2;	/* adjust for mono */
	
	sample = source_data_ss[ rd_dat ];
	
	mixed_buffer[ wr_dat++ ] 
	    += sample * player->left_vol_scale / ESD_VOLUME_BASE;
	mixed_buffer[ wr_dat++ ] 
	    += sample * player->right_vol_scale / ESD_VOLUME_BASE;
    }
    return wr_dat * sizeof(signed short);
}	

int mix_stereo_16s_to_stereo_32s( esd_player_t *player, int length )
{
    int rd_dat = 0;
    unsigned int wr_dat=0;
    signed short sample;
    register signed short *source_data_ss
	= (signed short *) player->data_buffer;

    ESDBG_MIXER( printf( "mixing stereo 16s to stereo 32s\n" ); );

    if ( player->rate == esd_audio_rate ) {
	/* optimize for simple increment by one and add loop */
	while ( wr_dat < length/sizeof(signed short) )
	{
	    sample = source_data_ss[ wr_dat ];
	    sample = sample * player->left_vol_scale / ESD_VOLUME_BASE;
	    mixed_buffer[ wr_dat++ ] += sample;

	    sample = source_data_ss[ wr_dat ];
	    sample = sample * player->right_vol_scale / ESD_VOLUME_BASE;
	    mixed_buffer[ wr_dat++ ] += sample;
	}
    } else {
	/* non integral multiple of sample rates, do it the hard way */
	while ( wr_dat < length/sizeof(signed short) )
	{
	    rd_dat = wr_dat * player->rate / esd_audio_rate;
	    rd_dat &= ~1;

	    sample = source_data_ss[ rd_dat++ ];
	    sample = sample * player->left_vol_scale / ESD_VOLUME_BASE;
	    mixed_buffer[ wr_dat++ ] += sample;

	    sample = source_data_ss[ rd_dat ];
	    sample = sample * player->right_vol_scale / ESD_VOLUME_BASE;
	    mixed_buffer[ wr_dat++ ] += sample;
	}
    }
 
    return wr_dat * sizeof(signed short);
}

/*******************************************************************/
/* takes mixed data, and clips data to the output buffer */
void clip_mix_to_output_16s( signed short *output, int length )
{
    signed int *mixed = mixed_buffer;
    signed int *end = mixed_buffer + length/sizeof(signed short);

    ESDBG_MIXER( printf( "clipping mix to output 16 bit (%d bytes)\n",
			 length ); );

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
void clip_mix_to_output_8u( unsigned char *output, int length )
{
    signed int *mixed = mixed_buffer;
    signed int *end = mixed_buffer + length/sizeof(signed short);

    ESDBG_MIXER( printf( "clipping mix to output 8 bit (%d bytes)\n", 
			 length ); );

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
int mix_players( void *output, int length )
{
    int actual = 0, min = 0, max = 0, translated = 0, iBps, used;
    esd_player_t *player = NULL;
    esd_player_t *erase = NULL;

    ESDBG_MIXER( printf( "++++++++++++++++++++++++++++++++++++++++++\n" ); );

    /* zero the sum buffer */
    memset( mixed_buffer, 0, esd_buf_size_samples * sizeof(*mixed_buffer) );
    
    /* as long as there's a player out there */
    player = esd_players_list;
    while( player != NULL )
    {
	/* read the client sound data */
	actual = read_player( player );
	
	if ( actual == 0 ) {
	    ESDBG_TRACE( printf( "(%02d) no new data available from player [%p]\n", 
				 player->source_id, player ); );
	    if ( player->actual_length == 0 ) {
		player = player->next;
		continue;
	    }
	}
	
	if ( actual < 0 ) {
	    /* actual < 0 means erase the player */
	    erase = player;
	} else {
	    /* actual >= 0 means data might be there to mix */
	    iBps = sizeof(signed short) * 2;

	    if ( (player->format & ESD_MASK_BITS) == ESD_BITS8 )
		iBps /= sizeof(signed short);
	    if ( (player->format & ESD_MASK_CHAN) == ESD_MONO )
		iBps /= 2;

	    /* how much data is really there? */
	    translated = ((player->actual_length / iBps) * esd_audio_rate + player->rate - 1) / player->rate;
	    translated *= sizeof(signed short) * 2;

	    if ( translated > length ) {
		ESDBG_TRACE( printf( "(%02d) player translated length doesn't fit [%p]\n",
			player->source_id, player ); );
		translated = length;
	    }

	    if ( min == 0 ) {
		min = translated;
	    } else {
		if ( min > translated ) {
		    min = translated;
		}
	    }
	}

	/* check out the next item in the list */
	player = player->next;

	/* clean up any fished players */
	if ( erase != NULL ) {
	    erase_player( erase );
	    erase = NULL;
	}
    }

    if ( min > 0 ) {
	player = esd_players_list;
	while( player != NULL )
	{
	    if ( player->actual_length > 0 ) {
		/* read_player(): >0 = data, ==0 = no data, <0 = erase it */
 		iBps = sizeof(signed short) * 2;
 
  		if ( (player->format & ESD_MASK_BITS) == ESD_BITS8 )
 		    iBps /= sizeof(signed short);
  		if ( (player->format & ESD_MASK_CHAN) == ESD_MONO )
 		    iBps /= 2;
 
 		actual = player->mix_func( player, min );
  
 		/* but how much data did we use? */
 		used = (actual / (sizeof(signed short) * 2)) * player->rate / esd_audio_rate;
 
 		if ( (player->format & ESD_MASK_BITS) != ESD_BITS8 )
 		    used *= sizeof(signed short);
 		if ( (player->format & ESD_MASK_CHAN) != ESD_MONO )
 		    used *= 2;
 
 		if ( player->actual_length - used > 0 ) {
 		    memmove( player->data_buffer, player->data_buffer + used, player->actual_length - used );
 		    player->actual_length -= used;
 		} else {
 		    /* kill it, it's probably all used anyways */
 		    player->actual_length = 0;
 		}
 		
		if ( actual > max ) {
		    max = actual;
		}
	    }

	    /* check out the next item in the list */
	    player = player->next;
	}

    /* ESDBG_COMMS( printf( "maximum stream length = %d bytes\n", max ); ); */ 

	if ( (esd_audio_format & ESD_MASK_BITS) == ESD_BITS16 ) 
	    clip_mix_to_output_16s( output, max );
	else {
	    clip_mix_to_output_8u( output, max );
	    max /= 2; /* half as many samples as you'd think */
	}
    }

    return max;
}

int refresh_mix_funcs(void)
{
    esd_player_t *filter = esd_filter_list;
    int translations = 0;

    /* walk the filters lists, and refresh the translate functions */
    ESDBG_MIXER( printf( "refreshing mix functions\n" ); );
    
    /* see if we need any translate functions */
    if ( !esd_filter_list ) 
	return 0;

    /* set the translation into the first filter */
    esd_first_filter_func = get_translate_func( esd_audio_format, 
						esd_audio_rate, 
						esd_filter_list->format, 
						esd_filter_list->rate );
    translations++;

    while ( filter ) {
	if ( filter->next ) {
	    filter->translate_func
		= get_translate_func( filter->format, filter->rate, 
				      filter->next->format, 
				      filter->next->rate );
	} else {
	    filter->translate_func 
		= get_translate_func( filter->format, filter->rate, 
				      esd_audio_format, 
				      esd_audio_rate );
	}

	filter = filter->next;
	translations++;
    }
    
    return translations;
}
