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
#pragma once

#include <string>

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/time.h>
#include <sysexits.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <syslog.h>
#include <pwd.h>
#include <ctype.h>

using std::string;

typedef struct client {
	int fd;
	struct client *next;
} client_t;

class lirc {

private:
	client_t *clients = NULL;

	struct timeval previous_input;
	int repeat = 0;
	
	void* xalloc(size_t size);
	long time_elapsed(struct timeval *last, struct timeval *current);
	
public:
	bool grab = false;
	string device;
	long repeat_time = 0L;
	int sockfd = -1;

	lirc();
	virtual ~lirc();
	bool Open(void);
	bool Close(void);
	void processnewclient(void);
	void processevent(const char *);
	void main_loop(void);
};
