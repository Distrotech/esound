/*
 * Again, completely untested... Use at your own risk =)
 *   code's by Karl Anders Oygard
 */

#include <assert.h>
#include <dmedia/audio.h>

ALport audioport;

#define ARCH_esd_audio_open
int esd_audio_open()
{
    ALconfig audioconfig;
    audioconfig = ALnewconfig();
  
    if (!audioconfig) {
	esd_audio_fd = -1;
	return esd_audio_fd;
    } else {
	long pvbuf[] = { AL_OUTPUT_COUNT, 0, AL_MONITOR_CTL, 0, AL_OUTPUT_RATE, 0};
    
	if (ALgetparams(AL_DEFAULT_DEVICE, pvbuf, 6) < 0)
	    if (oserror() == AL_BAD_DEVICE_ACCESS) {
		esd_audio_fd = -1;
		return esd_audio_fd;
	    }
    
	if (pvbuf[1] == 0 && pvbuf[3] == AL_MONITOR_OFF) {
	    long al_params[] = { AL_OUTPUT_RATE, 0};
      
	    al_params[1] = esd_audio_rate;
	    ALsetparams(AL_DEFAULT_DEVICE, al_params, 2);
	} else
	    if (pvbuf[5] != esd_audio_rate) {
		printf("audio device is already in use with wrong sample output rate\n");
		esd_audio_fd = -1;
		return esd_audio_fd;
	
	    }
    
	/* ALsetsampfmt(audioconfig, AL_SAMPFMT_TWOSCOMP); this is the default */
	/* ALsetwidth(audioconfig, AL_SAMPLE_16); this is the default */
    
	if ( (esd_audio_format & ESD_MASK_CHAN) == ESD_MONO)
	    ALsetchannels(audioconfig, AL_MONO);
	/* else ALsetchannels(audioconfig, AL_STEREO); this is the default */

	ALsetqueuesize(audioconfig, ESD_BUF_SIZE * 2);
    
	audioport = ALopenport("esd", "w", audioconfig);
	if (audioport == (ALport) 0) {
	    switch (oserror()) {
	    case AL_BAD_NO_PORTS:
		printf( "system is out of ports\n");
		esd_audio_fd = -1;
		return esd_audio_fd;
		break;
	
	    case AL_BAD_DEVICE_ACCESS:
		printf("couldn't access audio device\n");
		esd_audio_fd = -1;
		return esd_audio_fd;
		break;
	
	    case AL_BAD_OUT_OF_MEM:
		printf("out of memory\n");
		esd_audio_fd = -1;
		return esd_audio_fd;
		break;
	    }
	    /* don't know how we got here, but it must be bad */
	    esd_audio_fd = -1;
	    return esd_audio_fd;
	}
	ALsetfillpoint(audioport, ESD_BUF_SIZE);
    }

    esd_audio_fd = ALgetfd(audioport);
    return esd_audio_fd;
}

#define ARCH_esd_audio_close
void esd_audio_close()
{
    if (esd_audio_fd >= 0) {
	fd_set write_fds;
	FD_ZERO(&write_fds);
	FD_SET(esd_audio_fd, &write_fds);	
    
	ALsetfillpoint(audioport, ESD_BUF_SIZE * 2);
	select(esd_audio_fd + 1, NULL, &write_fds, NULL, NULL);
    }
    
    ALcloseport(audioport);
}

#define ARCH_esd_audio_write
int esd_audio_write(void *buffer, int buf_size)
{
    if (ALwritesamps(audioport, buffer, buf_size / 2) == 0) {
	ALsetfillpoint(audioport, ESD_BUF_SIZE);
	return buf_size;
    }
    else
	return 0;    
}

#define ARCH_esd_audio_read
int esd_audio_read(void *buffer, int buf_size)
{
    if (ALreadsamps(audioport, buffer, buf_size / 2) == 0) {
	ALsetfillpoint(audioport, ESD_BUF_SIZE);
	return buf_size;
    }
    else
	return 0;
}

#define ARCH_esd_audio_flush
void esd_audio_flush()
{
}
