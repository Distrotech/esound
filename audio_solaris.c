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
#include <errno.h>


/* if you want to confirm proper device setup, uncomment the following line */
/* #define ESDBG_DRIVER */

#define ARCH_esd_audio_devices
const char *esd_audio_devices()
{
    return "speaker, lineout, headphone";
}


void dump_audio_info(audio_info_t *t, int play)
{
    if( play )
    {
	char *enc, aports[100], ports[100];

	switch( t->play.encoding )
	{
	case AUDIO_ENCODING_NONE:
	    enc = "AUDIO_ENCODING_NONE";
	    break;
	case AUDIO_ENCODING_ULAW:
	    enc = "AUDIO_ENCODING_ULAW";
	    break;
	case AUDIO_ENCODING_ALAW:
	    enc = "AUDIO_ENCODING_ALAW";
	    break;
	case AUDIO_ENCODING_LINEAR:
	    enc = "AUDIO_ENCODING_LINEAR";
	    break;
	}

	ports[0] = 0;
	if( t->play.port & AUDIO_SPEAKER )
	    strcat(ports, "AUDIO_SPEAKER &");
	if( t->play.port & AUDIO_HEADPHONE )
	    strcat(ports, " AUDIO_HEADPHONE &");
	if( t->play.port & AUDIO_LINE_OUT)
	    strcat(ports, " AUDIO_LINE_OUT");

	aports[0] = 0;
	if( t->play.port & AUDIO_SPEAKER )
	    strcat(aports, "AUDIO_SPEAKER &");
	if( t->play.port & AUDIO_HEADPHONE )
	    strcat(aports, " AUDIO_HEADPHONE &");
	if( t->play.port & AUDIO_LINE_OUT)
	    strcat(aports, " AUDIO_LINE_OUT");
               
	printf("Play Info:\n");
	printf(" Sample Rate: %d\n", t->play.sample_rate);
	printf(" Channels:    %d\n", t->play.channels);
	printf(" Bits/sample: %d\n", t->play.precision);
	printf(" Encoding:    %s\n", enc);
	printf(" Gain:        %d\n", t->play.gain);
	printf(" Port:        %s\n", ports);
	printf(" availPorts:  %s\n", aports);
	printf(" Mon Gain:    %d\n", t->monitor_gain);
	printf(" o/p Muted:   %d\n", t->output_muted);
	printf(" Balance:     %d\n", t->play.balance);
    }

    return;
}


#define ARCH_esd_audio_open
int esd_audio_open()
{
    const char *const default_device = "/dev/audio";
    const char *const default_devicectl = "/dev/audioctl";
    int afd = -1, cafd = -1;
    int mode = O_WRONLY;
    audio_device_t adev;
    const char *device;
    
    if ((device = getenv("AUDIODEV")) == NULL)
      device = default_device;
    
    if ((esd_audio_format & ESD_MASK_FUNC) == ESD_RECORD) {
	audio_info_t ainfo;
               
	AUDIO_INITINFO(&ainfo);
	if( (cafd = open(default_devicectl, O_RDWR)) == -1 )
	{
	    fprintf(stderr,"Could not open ctl device for recording\n");
	    esd_audio_fd = -1;
	    return -1;
	}
	if (ioctl(cafd, AUDIO_GETDEV, &adev) == -1) {
	    fprintf(stderr, "Couldn't get info on audioctl device, FIXME\n");
	    perror(device);
	    close(cafd);
	    esd_audio_fd = -1;
	    return -1;
	}
	if ( (strcmp(adev.name, "SUNW,CS4231") != 0) 
	     && (strcmp(adev.name, "SUNW,sb16")  != 0)
	     && (strcmp(adev.name, "SUNW,dbri") != 0)  ) 
	{
	    fprintf(stderr, "No idea how to handle device `%s', FIXME\n", adev.name);
	    esd_audio_fd = -1;
	    return -1;
	}
	/* SUNW,CS4231 */
	{
	    int gain = 255; /* Range: 0 - 255 */
	    int port = AUDIO_MICROPHONE;
	    int bsize = 8180;
                       
	    ainfo.record.sample_rate = esd_audio_rate;
	    
	    if ((esd_audio_format & ESD_MASK_CHAN) == ESD_STEREO)
		ainfo.record.channels = 2;
	    else
		ainfo.record.channels = 1;
                       
	    if ((esd_audio_format & ESD_MASK_BITS) == ESD_BITS16)
		ainfo.record.precision = 16;
	    else
		ainfo.record.precision = 8;
                       
	    ainfo.record.encoding = AUDIO_ENCODING_LINEAR;
	    ainfo.record.gain = gain;
	    ainfo.record.port = port;
	    ainfo.record.balance = AUDIO_MID_BALANCE;
	    ainfo.record.buffer_size = bsize;
	    /* actually, it doesn't look like we need to set any
	       settings here-- they always seem to be the default, no
	       matter what else was spec.  
	    fprintf( stderr, "record set up: "
	             "rate=%d, channels=%d, precision=%d, gain=%d, port=%x\n",
		     ainfo.record.sample_rate, ainfo.record.channels, 
		     ainfo.record.precision, ainfo.record.gain, ainfo.record.port); 
	    */
	}
               
	if ((afd = open(device, O_RDONLY)) == -1) {
	    perror(device);
	    esd_audio_fd = -1;
	    return -1;
	}
               
	if (ioctl(afd, AUDIO_SETINFO, &ainfo) == -1)
	{
	    perror("AUDIO_SETINFO");
	    esd_audio_fd = -1;
	    return -1;
	}
	esd_audio_fd = afd;
	return afd;
    }
    /* implied else: if ( (esd_audio_format & ESD_MASK_FUNC) == ESD_RECORD ) */
    
    if ((afd = open(device, mode)) == -1) {
       if(errno != EACCES && errno != ENOENT)
           perror(device);
       esd_audio_fd = -1;
       return -2;
    }

    if (ioctl(afd, AUDIO_GETDEV, &adev) == -1) {
      perror(device);
      close(afd);
      esd_audio_fd = -1;
      return -1;
    }

    if ( (strcmp(adev.name, "SUNW,CS4231") != 0)
	&& (strcmp(adev.name, "SUNW,sb16")  != 0)
	&& (strcmp(adev.name, "SUNW,dbri") != 0)  ) 
    {
      fprintf(stderr, "No idea how to handle device `%s', FIXME\n", adev.name);
      close(afd);
      esd_audio_fd = -1;
      return -1;
    }

    /* SUNW,CS4231 and compatible drivers */
    {
        /* Volume, balance and output device should be controlled by
	   an external program - that way the user can his preferences
	   for all players */
	/* int gain = 64;	/* Range: 0 - 255 */
	int port;
	int bsize = 8180;
	audio_info_t ainfo;
      
	if ( esd_audio_device == NULL )
	    /* Don't change the output device unless specificaly requested */
	    port = 0;
	else if ( !strcmp( esd_audio_device, "lineout" ) )
	    port = AUDIO_LINE_OUT;
	else if ( !strcmp( esd_audio_device, "speaker" ) )
	    port = AUDIO_SPEAKER;
	else if ( !strcmp( esd_audio_device, "headphone" ) )
	    port = AUDIO_HEADPHONE;
	else {
	    fprintf(stderr, "Unknown output device `%s'\n", esd_audio_device);
	    close(afd);
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
	    ainfo.play.precision = 16;
	else
	    ainfo.play.precision = 8;
      
	ainfo.play.encoding = AUDIO_ENCODING_LINEAR;
	/* ainfo.play.gain = gain; */
	if(port)
	    ainfo.play.port = port;
	/* ainfo.play.balance = AUDIO_MID_BALANCE; */
	ainfo.play.buffer_size = bsize;
	ainfo.output_muted = 0;
      
#ifdef ESDBG_DRIVER
	dump_audio_info(&ainfo,1);
#endif
      
	if (ioctl(afd, AUDIO_SETINFO, &ainfo) == -1)
	{
	    perror("AUDIO_SETINFO");
	    close(afd);
	    esd_audio_fd = -1;
	    return -1;
	}
    }
    
    esd_audio_fd = afd;
    return afd;
}	    
