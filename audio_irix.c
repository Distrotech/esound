/*
 * Again, completely untested... Use at your own risk =)
 *   code's by Karl Anders Oygard
 */

#include <assert.h>
#include <dmedia/audio.h>

ALport audioport;

int esd_audio_open()
{
    ALport audioport;
    ALconfig audioconfig;
    long pvbuf[] = { AL_OUTPUT_COUNT, 0, AL_MONITOR_CTL, 0, AL_OUTPUT_RATE, 0 };
    char mode[5];
    
    audioconfig = ALnewconfig();
    
    if (!audioconfig) {
	fprintf(stderr,"not enough mem to init audio\n");
	esd_audio_fd = -1;
	return -1;
    }
    
    if (ALgetparams(AL_DEFAULT_DEVICE, pvbuf, 6) < 0)
	if (oserror() == AL_BAD_DEVICE_ACCESS) {
            fprintf(stderr,"AL_DEFAULT_DEVICE no good");
	    esd_audio_fd = -1;
	    return -1;
    }
    
    if (pvbuf[1] == 0 && pvbuf[3] == AL_MONITOR_OFF) {
	long al_params[] = { AL_OUTPUT_RATE, 0 };
    	al_params[1] = esd_audio_rate;
        ALsetparams(AL_DEFAULT_DEVICE, al_params, 2);
    }
    else if (pvbuf[5] != esd_audio_rate) {
	    fprintf(stderr,"audio device already open with incompatible sample rate\n");
            esd_audio_fd = -1;
            return -1;
    }
    
    if ((esd_audio_format & ESD_MASK_CHAN) != ESD_STEREO)
	ALsetchannels(audioconfig, AL_MONO);
    
    if ((esd_audio_format & ESD_MASK_BITS) != ESD_BITS16)
	ALsetwidth(audioconfig, AL_SAMPLE_8); /* FIXME: is this right? */    
    
    ALsetqueuesize(audioconfig, ESD_BUF_SIZE*2);

    /*
     * FIXME: I'm completely guessing here...  - Isaac 
     */
    if ((esd_audio_format & ESD_MASK_FUNC) == ESD_RECORD) 
	strcpy(mode, "rw");
    else
	strcpy(mode, "w"); 
    
    audioport = ALopenport("esd", mode, audioconfig);
    if (audioport == (ALport)0) {
	switch (oserror()) {
	case AL_BAD_NO_PORTS:
	    fprintf(stderr,"System is out of ports\n");
	    break;
	case AL_BAD_DEVICE_ACCESS:
	    fprintf(stderr,"Coupldn't access audio device\n");
	    break;
	case AL_BAD_OUT_OF_MEM:
	    fprintf(stderr,"Out of memory..\n");
	    break;
	default:
	    fprintf(stderr,"Unknown error..  BOOM\n");
	    break;
	}
        esd_audio_fd = -1;
        return -1;
    }

    ALsetfillpoint(audioport, ESD_BUF_SIZE);
    
    esd_audio_fd = ALgetfd(audioport);
    return esd_audio_fd;
}

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

int esd_audio_write(void *buffer, int buf_size)
{
    if (ALwritesamps(audioport, buffer, buf_size / 2) == 0) {
        ALsetfillpoint(audioport, ESD_BUF_SIZE);
        return buf_size;
    }
    else
	return 0;    
}

int esd_audio_read(void *buffer, int buf_size)
{
    if (ALreadsamps(audioport, buffer, buf_size / 2) == 0) {
	ALsetfillpoint(audioport, ESD_BUF_SIZE);
	return buf_size;
    }
    else
	return 0;
}

void esd_audio_flush()
{
}

