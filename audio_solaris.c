/*
 * Taken mainly from xmp, (C) 1996-98 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * Completely untested.. so fix it
 */

#ifdef HAVE_SYS_AUDIOIO_H
#include <sys/audioio.h>
#elif defined(HAVE_SYS_AUDIO_IO_H)
#include <sys/audio.io.h>
#elif defined(HAVE_SUN_AUDIOIO_H)
#include <sun/audioio.h>
#endif

int esd_audio_open()
{
    const char *device = "/dev/audio";
    
    audio_info_t ainfo;
    int gain = 128;
    int port = AUDIO_SPEAKER;
    int bsize = (0x0100 << 16);
    int afd = -1;
    int mode = O_WRONLY;
    
    if ((esd_audio_format & ESD_MASK_FUNC) == ESD_RECORD) {
	fprintf(stderr, "No idea how to record audio on solaris, FIXME\n");
	esd_audio_fd = -1;
	return -1;
    }
    
    if ((afd = open(device, mode)) == -1) {
       perror(device);
       esd_audio_fd = -1;
       return -1;
    }
    
    AUDIO_INITINFO(&ainfo);
    
    ainfo.play.sample_rate = esd_audio_rate;
    
    if ((esd_audio_format & ESD_MASK_CHAN) == ESD_STEREO)
        ainfo.play.channels = 2;
    else
	ainfo.play.channels = 1;
    
    if ((esd_audio_format & ESD_MASK_BITS) == ESD_BITS16)
        ainfo.precision = 16;
    else
	ainfo.precision = 8;
    
    ainfo.play.encoding = AUDIO_ENCODING_LINEAR;
    ainfo.play.gain = gain;
    ainfo.play.port = port;
    ainfo.play.buffer_size = bsize;
    ainfo.play.balance = AUDIO_MID_BALANCE;
   
    if (ioctl(afd, AUDIO_SETINFO, &ainfo) == -1) {
	perror("AUDIO_SETINFO");
        close(afd);
	esd_audio_fd = -1;
	return -1;
    }
    
    esd_audio_fd = afd;
    return afd;
}	    
