/*
    ceclircd -- LIRC daemon that reads CEC events from libcec
                https://github.com/Pulse-Eight/libcec
				
    Copyright (c) 2014 Dirk E. Wagner

    based on:
    inputlircd -- zeroconf LIRC daemon that reads from /dev/input/event devices
    Copyright (c) 2006  Guus Sliepen <guus@sliepen.eu.org>
	
    libcec-daemon -- A Linux daemon for connecting libcec to uinput.
    Copyright (c) 2012-2013, Andrew Brampton
    https://github.com/bramp/libcec-daemon
	
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

#include <pthread.h>

#include <log4cplus/logger.h>                                                                                 
#include <log4cplus/loggingmacros.h>                                                                          

#include "lirc.h"                                                                                                               

using namespace log4cplus;                                                                                    
 
static Logger logger = Logger::getInstance("lirc");        
static pthread_mutex_t lirc_sync = PTHREAD_MUTEX_INITIALIZER;

extern "C" void *extf(void* This) {
	static_cast<lirc*>(This)->main_loop();
}

lirc::lirc() : isRunning(false) {
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
		throw std::runtime_error("Error during malloc()");
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

	if (pthread_create(&lirc_thread, NULL, &extf, this)) {
        	fprintf(stderr, "Can't create lirc thread");
        	return false;
	}
	
	isRunning = true;
	
	return true;
}

bool lirc::Close() {                     
	LOG4CPLUS_TRACE_STR(logger, "lirc::Close()");
	
	isRunning = false;
	
	if (sockfd >= 0) {
		LOG4CPLUS_TRACE_STR(logger, "lirc::Close() sockfd");
		shutdown (sockfd, SHUT_RDWR);
		close (sockfd);
	}

	pthread_mutex_destroy( &lirc_sync);
	pthread_join(lirc_thread, NULL);
	LOG4CPLUS_TRACE_STR(logger, "lirc::Close() lirc_thread terminated");

	return true; 
}     

void lirc::processnewclient(void) {
	
	LOG4CPLUS_TRACE_STR(logger, "lirc::processnewclient(void) start");
	
	pthread_mutex_lock( &lirc_sync );
	
	client_t *newclient = (client_t *)xalloc(sizeof *newclient);

	newclient->fd = accept(sockfd, NULL, NULL);

	if(newclient->fd < 0) {
		free(newclient);
		pthread_mutex_unlock( &lirc_sync );
		LOG4CPLUS_DEBUG_STR(logger, "lirc::processnewclient(void) - Error during accept(): " + string(strerror(errno)));
		return;
	}

	int flags = fcntl(newclient->fd, F_GETFL);
	fcntl(newclient->fd, F_SETFL, flags | O_NONBLOCK);
	newclient->next = clients;
	clients = newclient;
	
	pthread_mutex_unlock( &lirc_sync );
}

void lirc::processevent(const char *message) {
	LOG4CPLUS_TRACE_STR(logger, "lirc::processevent start " + string(message));

	pthread_mutex_lock( &lirc_sync );
	int len = strlen(message);
	client_t *client, *prev, *next;
	struct timeval current;
	
	previous_input = current;
	for(client = clients; client; client = client->next) {
		if(write(client->fd, message, len) != len) {
			close(client->fd);
			client->fd = -1;
		}
	}

	for(prev = NULL, client = clients; client; client = next) {
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

	pthread_mutex_unlock( &lirc_sync );
}

void lirc::main_loop(void) {

	LOG4CPLUS_TRACE_STR (logger, "main_loop start");
	
	fd_set fdset;
	int maxfd = 0;

	FD_ZERO(&fdset);
	
	FD_SET(sockfd, &fdset);
	if(sockfd > maxfd)
		maxfd = sockfd;

	maxfd++;
	
	while(isRunning) {
		LOG4CPLUS_TRACE_STR(logger, "lirc::main_loop() while entered");
		
		if(select(maxfd, &fdset, NULL, NULL, NULL) < 0) {
			if(errno == EINTR)
				continue;
			syslog(LOG_ERR, "Error during select(): %s\n", strerror(errno));
			throw std::runtime_error("Error during select()");
		}

		if(FD_ISSET(sockfd, &fdset))
			processnewclient();
			
	}
	
	LOG4CPLUS_TRACE_STR(logger, "lirc::main_loop() end");
}

