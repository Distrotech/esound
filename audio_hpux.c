/*
 * Taken mainly from xmp, Cc( 1996-98 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * Who knows if it works, not I, not I...
 *  -- Isaac
 */

#include <sys/audio.h>

#define ARCH_esd_audio_open
int esd_audio_open()
{
    const char *device = "/dev/audio";

    int afd = -1;
    int mode = O_WRONLY;
    int gain = AUDIO_MAX_GAIN;
    int bsize;
    int port = AUDIO_OUT_SPEAKER;
    int flags, test;
    struct audio_gains agains;
    struct audio_describe adescribe;
    struct audio_limits alimit;

    if ((esd_audio_format & ESD_MASK_FUNC) == ESD_RECORD) {
	fprintf(stderr, "No idea how to record audio on hp-ux boxen, FIXME\n");
	esd_audio_fd = -1;
	return -1;
    }
    
    if ((afd = open(device, mode)) == -1) {
	perror(device);
	esd_audio_fd = -1;
	return -1;
    }
    
    flags = fcntl(afd, F_GETFL, 0);
    if (flags < 0) {
	perror("F_GETFL");    
	esd_audio_fd = -1;
	return -1;
    }
    flags |= O_NDELAY;
    fcntl(afd, F_SETFL, flags);
    if (flags < 0) {
	perror("F_SETFL");    
	esd_audio_fd = -1;
	return -1;
    }
    
    if ((esd_audio_format & ESD_MASK_BITS) == ESD_BITS16)
        flags = AUDIO_FORMAT_LINEAR16BIT;
    else
	flags = AUDIO_FORMAT_ULAW; /* FIXME: is this right? */
    /* TODO: probably need a linear2ulaw before sending the mix to the audio device */
    if (ioctl(afd, AUDIO_SET_DATA_FORMAT, flags) == -1) {
	perror("AUDIO_SET_DATA_FORMAT");
	close(afd);
	esd_audio_fd = -1;
	return -1;
    }
    
    if ((esd_audio_format & ESD_MASK_CHAN) == ESD_STEREO)
	flags = 2;
    else
	flags = 1;
    if (ioctl(afd, AUDIO_SET_CHANNELS, flags) == -1) {
	perror("AUDIO_SET_CHANNELS");
	close(afd);
	esd_audio_fd = -1;
	return -1;
    }
    
    if (ioctl(afd, AUDIO_SET_OUTPUT, port) == -1) {
	perror("AUDIO_SET_PORT");
	close(afd);
	esd_audio_fd = -1;
	return -1;
    }
    
    test = esd_audio_rate;
    if (ioctl(afd, AUDIO_SET_SAMPLE_RATE, test) == -1) {
	perror("AUDIO_SET_SAMPLE_RATE");
	close(afd);
	esd_audio_fd = -1;
	return -1;
    }
    if (fabs(test - esd_audio_rate) > esd_audio_rate * 0.05) {
	fprintf(stderr,"unsupported playback rate: %d\n", esd_audio_rate);
	close(afd);
	esd_audio_fd = -1;
	return -1;
    }
    
    if (ioctl(afd, AUDIO_DESCRIBE, &adescribe) == -1) {
	perror("AUDIO_DESCRIBE");
        close(afd);
        esd_audio_fd = -1;
        return -1;
    }

    if (ioctl(afd, AUDIO_GET_GAINS, &agains) == -1) {
	perror("AUDIO_GET_GAINS");
	close(afd);
	esd_audio_fd = -1;
	return -1;
    }
    
    agains.transmit_gain = adescribe.min_transmit_gain +
	(adescribe.max_transmit_gain - adescribe.min_transmit_gain) *
	gain / 256;

#if 0    
    if (ioctl(afd, AUDIO_SET_GAINS, &agains) == -1) {
	perror("AUDIO_SET_GAINS");
	close(afd);
	esd_audio_fd = -1;
	return -1;
    }
#else
    if (ioctl(afd, AUDIO_SET_GAINS, AUDIO_MAX_GAIN)) {
	perror("AUDIO_SET_GAINS");
	close(afd);
	esd_audio_fd = -1;
	return -1;
    }
#endif

    if (!ioctl(afd, AUDIO_GET_LIMITS, &alimit)) {
	bsize = (0x0100 << 16);
    } else {
	bsize = alimit.max_transmit_buffer_size;
    }
    
    while (bsize) {
	if (ioctl(afd, AUDIO_SET_TXBUFSIZE, bsize) != -1) {
	    break;
	}
	bsize >>= 1;
    }
    if (!bsize) {
	close(afd);
	esd_audio_fd = -1;
	return -1;
    }

    if (ioctl(afd, AUDIO_SET_TXBUFSIZE, bsize) == -1) {
	perror("AUDIO_SET_TXBUFSIZE");
	close(afd);
	esd_audio_fd = -1;
	return -1;
    }
    
    esd_audio_fd = afd;
    return afd;
}

