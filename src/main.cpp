/**
 * libcec-daemon
 * A simple daemon to connect libcec to uinput. That is, using your TV to control your PC! 
 * by Andrew Brampton
 *
 * TODO
 *
 */
#include "main.h"

#define CEC_NAME    "RaspberryPI"

#include <stdlib.h>

#include <algorithm>
#include <cstdio>
#include <iostream>
#include <sstream>
#include <cstdint>
#include <cstddef>
#include <csignal>
#include <vector>
#include <unistd.h>

#include <log4cplus/logger.h>
#include <log4cplus/loggingmacros.h>
#include <log4cplus/configurator.h>

using namespace CEC;
using namespace log4cplus;

using std::cout;
using std::cerr;
using std::endl;
using std::hex;
using std::min;
using std::string;
using std::stringstream;
using std::vector;

static Logger logger = Logger::getInstance("main");

Main & Main::instance() {
	// Singleton pattern so we can use main from a sighandle
	static Main main;
	return main;
}

Main::Main() : cec(getCecName(), this), repeat_time(0), makeActive(true), running(true) {
	LOG4CPLUS_TRACE_STR(logger, "Main::Main()");

	signal (SIGINT,  &Main::signalHandler);
	signal (SIGTERM, &Main::signalHandler);
}

Main::~Main() {
	LOG4CPLUS_TRACE_STR(logger, "Main::~Main()");
	stop();
}

void Main::loop() {
	LOG4CPLUS_TRACE_STR(logger, "Main::loop()");

	cec.open();

	if (makeActive) {
		cec.makeActive();
	}

	if (mylirc.Open()) {
		while (running) {
			mylirc.main_loop();
		}
	}

	mylirc.Close();
	cec.close();
}

void Main::stop() {
	LOG4CPLUS_TRACE_STR(logger, "Main::stop()");

	mylirc.Close();
	running = false;
}

void Main::listDevices() {
	LOG4CPLUS_TRACE_STR(logger, "Main::listDevices()");
	cec.listDevices(cout);
}

void Main::signalHandler(int sigNum) {
	LOG4CPLUS_DEBUG_STR(logger, "Main::signalHandler()");
	
	Main::instance().stop();
}

char *Main::getCecName() {
	LOG4CPLUS_TRACE_STR(logger, "Main::getCecName()");
	if (gethostname(cec_name,HOST_NAME_MAX) < 0 ) {
		LOG4CPLUS_TRACE_STR(logger, "Main::getCecName()");
		strncpy(cec_name, CEC_NAME, sizeof(HOST_NAME_MAX));
	}

	return cec_name;
}

int Main::onCecLogMessage(const cec_log_message &message) {
	LOG4CPLUS_DEBUG(logger, "Main::onCecLogMessage(" << message << ")");
	return 1;
}

int Main::onCecKeyPress(const cec_keypress &key) {
	LOG4CPLUS_DEBUG(logger, "Main::onCecKeyPress(" << key << ")");

	int repeat = 0;
	stringstream s;
	stringstream code;

	std::map<cec_user_control_code, const char *>::const_iterator it;

        if(key.duration > repeat_time)                                                                                                       
                repeat = 1; 
        else
                repeat = 0;

	code << "@0x" << hex << key.keycode;
                                           
	it = Cec::cecUserControlCodeName.find(key.keycode);
	if (it == Cec::cecUserControlCodeName.end()) {
//		it = Cec::cecUserControlCodeName.find(CEC_USER_CONTROL_CODE_UNKNOWN);
//		assert(it != Cec::cecUserControlCodeName.end());
		s << code << " " << repeat << " " << code << " RPICEC" << endl;
	} else {
		s << code << " " << repeat << " " << string(it->second) << " RPICEC" << endl;
	}	

//	if (key.duration != 0) {
		mylirc.processevent(s.str().c_str());
//	}

	return 1;
}

int Main::onCecCommand(const cec_command & command) {
	LOG4CPLUS_DEBUG(logger, "Main::onCecCommand(" << command << ")");
	stringstream s;

	switch (command.opcode) {
		case CEC_OPCODE_PLAY:
		case CEC_OPCODE_DECK_CONTROL:
		case CEC_OPCODE_STANDBY:
			s << command.opcode << " 0 " << command.opcode << " RPICEC" << endl;
			mylirc.processevent(s.str().c_str());
			break;
		default:
			;
	}	
	return 1;
}


int Main::onCecConfigurationChanged(const libcec_configuration & configuration) {
	LOG4CPLUS_DEBUG(logger, "Main::onCecConfigurationChanged(" << configuration << ")");
	return 1;
}

int main (int argc, char *argv[]) {

    BasicConfigurator config;
    config.configure();

	int opt;
    int loglevel = 2;
	bool foreground = false;
	bool list = false;
	bool dontactivate = false;
	int repeat_time = 500;
	string lircpath;
	
	while((opt = getopt(argc, argv, "hVflv:ari:")) != -1) {
        switch(opt) {
			case 'd':
				lircpath = string(optarg);
				break;		
			case 'f':
				foreground = true;
				break;
			case 'l':
				list = true;
				break;
			case 'v':
				loglevel = atoi(optarg);
				cout << loglevel << endl;
				break;
			case 'a':
				dontactivate = true;
				break;
			case 'r':
				repeat_time = atoi(optarg) * 1000L;
				break;
			case 'V':
			case 'h':
            default:
		cout << "Usage: " << argv[0] << " [options] " << endl << endl;
		cout << "Options:" << endl;
		cout << "\t-d <socket> UNIX socket. The default is /var/run/lirc/lircd." << endl;
		cout << "\t-f Run in the foreground." << endl;
		cout << "\t-l list cec devices" << endl;
		cout << "\t-a do not activate" << endl;
		cout << "\t-r <rate> Repeat rate in ms. (Default is 500ms)" << endl;
		cout << "\t-v <num> log level" << endl;
		cout << "\t-t <path> Path to translation table." << endl;
                return 0;
        }
    }

//	if(argc <= optind) {
//		fprintf(stderr, "Not enough arguments.\n");
//		return 0;
//	}

	Logger root = Logger::getRoot();
	switch (loglevel) {
		case 2:  root.setLogLevel(TRACE_LOG_LEVEL); break;
		case 1:  root.setLogLevel(DEBUG_LOG_LEVEL); break;
		default: root.setLogLevel(INFO_LOG_LEVEL); break;
		case -1: root.setLogLevel(FATAL_LOG_LEVEL); break;
	}

	try {
		// Create the main
		Main & main = Main::instance();

		if (dontactivate) {
			main.setMakeActive(false);
		}

		main.setRepeatTime(repeat_time);
		
		if (!lircpath.empty()) {
			main.setLircPath(lircpath);
		}
		
		if (list) {
			main.listDevices();
			return 0;
		}

		if (!foreground) {
			daemon(0, 0);
		}

		main.loop();

	} catch (std::exception & e) {
		cerr << e.what() << endl;
		return -1;
	}

	return 0;
}

