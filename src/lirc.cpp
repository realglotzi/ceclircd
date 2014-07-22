/*
    irmplircd -- zeroconf LIRC daemon that reads IRMP events from the USB IR Remote Receiver
	             http://www.mikrocontroller.net/articles/USB_IR_Remote_Receiver
    Copyright (C) 2011-2012  Dirk E. Wagner

	based on:
    inputlircd -- zeroconf LIRC daemon that reads from /dev/input/event devices
    Copyright (C) 2006  Guus Sliepen <guus@sliepen.eu.org>

    This program is free software; you can redistribute it and/or modify it
    under the terms of version 2 of the GNU General Public License as published
    by the Free Software Foundation.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
*/

#include <log4cplus/logger.h>                                                                                 
#include <log4cplus/loggingmacros.h>                                                                          

#include "lirc.hpp"                                                                                                               

using namespace log4cplus;                                                                                    
 
static Logger logger = Logger::getInstance("lirc");        

lirc::lirc() {
	device = string("/var/run/lirc/lircd");
	gettimeofday(&previous_input, NULL);
}

lirc::~lirc() {
	lirc::Close();
}

 void* lirc::xalloc(size_t size) {
	void *buf = malloc(size);
	if(!buf) {
		fprintf(stderr, "Could not allocate %zd bytes with malloc(): %s\n", size, strerror(errno));
		exit(EX_OSERR);
	}
	memset(buf, 0, size);
	return buf;
}
	
bool lirc::Open(void) {
	LOG4CPLUS_TRACE_STR(logger, "lirc::Open() " + device);

	const char *lircpath = device.c_str();
	
	struct sockaddr_un sa = {0};
	sockfd = socket(AF_UNIX, SOCK_STREAM, 0);

	if(sockfd < 0) {
		fprintf(stderr, "Unable to create an AF_UNIX socket: %s\n", strerror(errno));
		return false;
	}

	sa.sun_family = AF_UNIX;
	strncpy(sa.sun_path, lircpath, sizeof sa.sun_path - 1);

	unlink(lircpath);

	if(bind(sockfd, (struct sockaddr *)&sa, sizeof sa) < 0) {
		fprintf(stderr, "Unable to bind AF_UNIX socket to %s: %s\n", lircpath, strerror(errno));
		return false;
	}

	chmod(lircpath, 0666);

	if(listen(sockfd, 3) < 0) {
		fprintf(stderr, "Unable to listen on AF_UNIX socket: %s\n", strerror(errno));
		return false;
	}

	return true;
}

bool lirc::Close() {                     
	LOG4CPLUS_TRACE_STR(logger, "lirc::Close()");                                                                          
        if (sockfd >= 0)                                                                                     
                close (sockfd);                                                                              
	
	return true; 
}     

void lirc::processnewclient(void) {
	
	LOG4CPLUS_TRACE_STR(logger, "lirc::processnewclient(void) start");
	
	client_t *newclient = (client_t *)xalloc(sizeof *newclient);

	newclient->fd = accept(sockfd, NULL, NULL);

	if(newclient->fd < 0) {
		free(newclient);
		if(errno == ECONNABORTED || errno == EINTR)
			return;
		syslog(LOG_ERR, "Error during accept(): %s\n", strerror(errno));
		exit(EX_OSERR);
	}

	int flags = fcntl(newclient->fd, F_GETFL);
	fcntl(newclient->fd, F_SETFL, flags | O_NONBLOCK);
	newclient->next = clients;
	clients = newclient;
	
}

long lirc::time_elapsed(struct timeval *last, struct timeval *current) {
	long seconds = current->tv_sec - last->tv_sec;
	return 1000000 * seconds + current->tv_usec - last->tv_usec;
}

void lirc::processevent(const char *message) {
	LOG4CPLUS_TRACE_STR(logger, "processevent start " + string(message));

#ifdef UNUSED
	IRMP_DATA event;
	char hash_key[100];
	char irmp_fulldata[100];
	char message[100];
	int len;
	client_t *client, *prev, *next;

	message[0]=0;
	
	if((len=read(evdev->fd, &event, sizeof event)) <= 0) {
		syslog(LOG_ERR, "Error processing event from %s: %s\n", evdev->name, strerror(errno));
		exit(EX_OSERR);
	}

	DBG ("dummy = 0x%02d, p = %02d, a = 0x%04x, c = 0x%04x, f = 0x%02x\n", event.dummy, event.protocol, event.address, event.command, event.flags);
		
	struct timeval current;
	gettimeofday(&current, NULL);
	if(time_elapsed(&previous_input, &current) < repeat_time)
		repeat++;
	else 
		repeat = 0;

	snprintf (irmp_fulldata, sizeof irmp_fulldata, "%02x%04x%04x%02x", event.protocol, event.address, event.command, event.flags);
	snprintf (hash_key, sizeof hash_key, "%02x%04x%04x%02x", event.protocol, event.address, event.command, 0);

	map_entry_t *map_entry;
	
	if(hashmap_get(mymap, hash_key, (void**)(&map_entry))==MAP_OK) {
		DBG ("MAP_OK irmpd_fulldata=%s lirc=%s\n", irmp_fulldata, map_entry->value);	
		len = snprintf(message, sizeof message, "%s %x %s %s\n",  irmp_fulldata, event.flags, map_entry->value, "IRMP");
	} else {
		DBG ("MAP_ERROR irmpd_fulldata=%s|\n", irmp_fulldata);	
		len = snprintf(message, sizeof message, "%s %x %s %s\n",  irmp_fulldata, repeat, irmp_fulldata, "IRMP");
	}
#endif	

	int len = strlen(message);
	client_t *client, *prev, *next;
	struct timeval current;
	
	previous_input = current;
	for(client = clients; client; client = client->next) {
		printf ("processevent clients\n");
		if(write(client->fd, message, len) != len) {
			close(client->fd);
			client->fd = -1;
		}
	}

	for(prev = NULL, client = clients; client; client = next) {
		printf ("processevent clean\n");
		next = client->next;
		if(client->fd < 0) {
			if(prev)
				prev->next = client->next;
			else
				clients = client->next;
			free(client);
		} else {
			prev = client;
		}
	}

}

void lirc::main_loop(void) {

	LOG4CPLUS_TRACE_STR (logger, "main_loop");
	
	fd_set fdset;
	int maxfd = 0;

	FD_ZERO(&fdset);
	
	FD_SET(sockfd, &fdset);
	if(sockfd > maxfd)
		maxfd = sockfd;

	maxfd++;
	
	while(true) {
		printf ("while(true)\n");
		
		if(select(maxfd, &fdset, NULL, NULL, NULL) < 0) {
			if(errno == EINTR)
				continue;
			syslog(LOG_ERR, "Error during select(): %s\n", strerror(errno));
			exit(EX_OSERR);
		}

		if(FD_ISSET(sockfd, &fdset))
			processnewclient();
			
	}
}

