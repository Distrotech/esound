#include "esd.h"
#include <stdlib.h>
#include <strings.h>

const char*
esd_get_socket_dirname (void) 
{
	char *audiodev;
	static char *dirname = NULL;

	if (dirname == NULL) {
		if (!(audiodev = getenv("AUDIODEV"))) {
			audiodev = "";
		} else {
			audiodev = strrchr(audiodev, '/');
			audiodev++;
		}
		dirname = malloc(strlen(audiodev) +9);
		strcpy(dirname, "/tmp/.esd");
		strcat(dirname, audiodev);
	}

	return dirname;
}

const char*
esd_get_socket_name (void) 
{
	const char *dirname;
	static char *name = NULL;

	if (name == NULL) {
		dirname = esd_get_socket_dirname();
		name = malloc(strlen(dirname) +7);
		strcpy(name, dirname);
		strcat(name, "/socket");
	}

	return name;
}
