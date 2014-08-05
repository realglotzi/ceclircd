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

#include "main.h"
#include "hdmi.h"

#define CEC_NAME    "RaspberryPI"

#include <stdlib.h>

#include <algorithm>
#include <cstdio>
#include <iostream>
#include <sstream>
#include <cstdint>
#include <cstddef>
#include <csignal>
#include <cstdlib>
#include <vector>
#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>

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
using std::queue;
using std::list;

static Logger logger = Logger::getInstance("main");
static pthread_mutex_t libcec_sync = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  libcec_cond = PTHREAD_COND_INITIALIZER;
const vector<list<string>> Main::uinputCecMap = Main::setupUinputMap();

enum
{
	COMMAND_STANDBY,
	COMMAND_ACTIVE,
	COMMAND_INACTIVE,
	COMMAND_RESTART,
	COMMAND_KEYPRESS,
	COMMAND_KEYRELEASE,
	COMMAND_EXIT,
};

Main & Main::instance() {
	// Singleton pattern so we can use main from a sighandle
	static Main main;
	return main;
}

Main::Main() : cec(getCecName(), this), 
	makeActive(true), running(false), lastUInputKeys({ }), logicalAddress(CECDEVICE_UNKNOWN) {
	LOG4CPLUS_TRACE_STR(logger, "Main::Main()");

}

Main::~Main() {
	LOG4CPLUS_TRACE_STR(logger, "Main::~Main()");

//	stop();

	pthread_cond_destroy (&libcec_cond);                                                                                         
	pthread_mutex_destroy (&libcec_sync);                                                                                              
  
}

void Main::loop(const string & device) {
	LOG4CPLUS_TRACE_STR(logger, "Main::loop()");

	struct timeval now;                                                                                                  
	struct timespec timeout;

	struct sigaction action;

	action.sa_handler = &Main::signalHandler;
	action.sa_flags = SA_RESETHAND;
	sigemptyset(&action.sa_mask);

	int restart = false;

	do
	{
		cec.open(device);
		if (!mylirc.Open()) {
				/* reset signals */
			signal (SIGHUP,  SIG_DFL);
			signal (SIGINT,  SIG_DFL);
			signal (SIGTERM, SIG_DFL);
			signal (SIGPIPE, SIG_DFL);
			
			cec.close(!restart);
			
			return;
		}

		running = true;

		/* install signals */
		sigaction (SIGHUP,  &action, NULL);
		sigaction (SIGINT,  &action, NULL);
		sigaction (SIGTERM, &action, NULL);
		sigaction (SIGPIPE, &action, NULL);
		
		if (makeActive) 
		{
			cec.makeActive();
		}
		
		do
		{
			pthread_mutex_lock( &libcec_sync );
			while( running && !commands.empty() )
			{
				Command cmd = commands.front();
				switch( cmd.command )
				{
					case COMMAND_STANDBY:
						if( ! onStandbyCommand.empty() )
						{
							LOG4CPLUS_DEBUG(logger, "Standby: Running \"" << onStandbyCommand << "\"");
							int ret = system(onStandbyCommand.c_str());
                            if( ret )
                                LOG4CPLUS_ERROR(logger, "Standby command failed: " << ret);
							
						}
						else
						{
							onCecKeyPress( CEC_USER_CONTROL_CODE_POWER );
						}
						break;
					case COMMAND_ACTIVE:
						makeActive = true;
						if( ! onActivateCommand.empty() )
						{
							LOG4CPLUS_DEBUG(logger, "Activated: Running \"" << onActivateCommand << "\"");
							int ret = system(onActivateCommand.c_str());
                            if( ret )
                                LOG4CPLUS_ERROR(logger, "Activate command failed: " << ret);
						}
						break;
					case COMMAND_INACTIVE:
						makeActive = false;
						if( ! onDeactivateCommand.empty() )
						{
							LOG4CPLUS_DEBUG(logger, "Deactivated: Running \"" << onDeactivateCommand << "\"");
							int ret = system(onDeactivateCommand.c_str());
                            if( ret )
                                LOG4CPLUS_ERROR(logger, "Deactivate command failed: " << ret);
						}
						break;
					case COMMAND_KEYPRESS:
						onCecKeyPress( cmd.keycode );
						break;
					case COMMAND_RESTART:
						LOG4CPLUS_DEBUG(logger, "COMMAND_RESTART");
						running = false;
						restart = true;
						break;
					case COMMAND_EXIT:
						LOG4CPLUS_DEBUG(logger, "COMMAND_EXIT");
						running = false;
						break;
				}
				commands.pop();
			}

			gettimeofday(&now, NULL);
			timeout.tv_sec = now.tv_sec + 1;
			timeout.tv_nsec = now.tv_usec * 1000;

			pthread_cond_timedwait(&libcec_cond, &libcec_sync, &timeout);
			pthread_mutex_unlock( &libcec_sync );
		}
		while( running );

		/* reset signals */
		signal (SIGHUP,  SIG_DFL);
		signal (SIGINT,  SIG_DFL);
		signal (SIGTERM, SIG_DFL);
		signal (SIGPIPE, SIG_DFL);
		
		cec.close(!restart);
		mylirc.Close();
	}
	while( restart );
}

void Main::push(Command cmd) {
	pthread_mutex_lock(&libcec_sync);
	if( running )
	{
		commands.push(cmd);
		pthread_cond_signal(&libcec_cond);

	}
	pthread_mutex_unlock( &libcec_sync );
}

void Main::stop() {
	LOG4CPLUS_TRACE_STR(logger, "Main::stop()");
	push(Command(COMMAND_EXIT));
}

void Main::restart() {
	LOG4CPLUS_TRACE_STR(logger, "Main::restart()");
	push(Command(COMMAND_RESTART));
}

void Main::listDevices() {
	LOG4CPLUS_TRACE_STR(logger, "Main::listDevices()");
	cec.listDevices(cout);
}

void Main::signalHandler(int sigNum) {
	LOG4CPLUS_DEBUG_STR(logger, "Main::signalHandler()");
	
	switch( sigNum ) {
		case SIGPIPE:
		case SIGHUP:
			Main::instance().restart();
			break;
		default:
			Main::instance().stop();
			break;
	}
}

char *Main::getCecName() {
	LOG4CPLUS_TRACE_STR(logger, "Main::getCecName()");
	if (gethostname(cec_name,HOST_NAME_MAX) < 0 ) {
		LOG4CPLUS_TRACE_STR(logger, "Main::getCecName()");
		strncpy(cec_name, CEC_NAME, sizeof(HOST_NAME_MAX));
	}

	return cec_name;
}

const std::vector<list<string>> & Main::setupUinputMap() {
	static std::vector<list<string>> uinputCecMap;

	if (uinputCecMap.empty()) {
		uinputCecMap.resize(CEC_USER_CONTROL_CODE_MAX + 1, {});
		uinputCecMap[CEC_USER_CONTROL_CODE_SELECT                      ] = { "KEY_OK" };
		uinputCecMap[CEC_USER_CONTROL_CODE_UP                          ] = { "KEY_UP" };
		uinputCecMap[CEC_USER_CONTROL_CODE_DOWN                        ] = { "KEY_DOWN" };
		uinputCecMap[CEC_USER_CONTROL_CODE_LEFT                        ] = { "KEY_LEFT" };
		uinputCecMap[CEC_USER_CONTROL_CODE_RIGHT                       ] = { "KEY_RIGHT" };
		uinputCecMap[CEC_USER_CONTROL_CODE_RIGHT_UP                    ] = { "KEY_RIGHT", "KEY_UP" };
		uinputCecMap[CEC_USER_CONTROL_CODE_RIGHT_DOWN                  ] = { "KEY_RIGHT", "KEY_DOWN" };
		uinputCecMap[CEC_USER_CONTROL_CODE_LEFT_UP                     ] = { "KEY_LEFT", "KEY_UP" };
		uinputCecMap[CEC_USER_CONTROL_CODE_LEFT_DOWN                   ] = { "KEY_RIGHT", "KEY_UP" };
		uinputCecMap[CEC_USER_CONTROL_CODE_ROOT_MENU                   ] = { "KEY_HOME" };
		uinputCecMap[CEC_USER_CONTROL_CODE_SETUP_MENU                  ] = { "KEY_SETUP" };
		uinputCecMap[CEC_USER_CONTROL_CODE_CONTENTS_MENU               ] = { "KEY_MENU" };
		uinputCecMap[CEC_USER_CONTROL_CODE_FAVORITE_MENU               ] = { "KEY_FAVORITES" };
		uinputCecMap[CEC_USER_CONTROL_CODE_EXIT                        ] = { "KEY_EXIT" };
		uinputCecMap[CEC_USER_CONTROL_CODE_NUMBER0                     ] = { "KEY_0" };
		uinputCecMap[CEC_USER_CONTROL_CODE_NUMBER1                     ] = { "KEY_1" };
		uinputCecMap[CEC_USER_CONTROL_CODE_NUMBER2                     ] = { "KEY_2" };
		uinputCecMap[CEC_USER_CONTROL_CODE_NUMBER3                     ] = { "KEY_3" };
		uinputCecMap[CEC_USER_CONTROL_CODE_NUMBER4                     ] = { "KEY_4" };
		uinputCecMap[CEC_USER_CONTROL_CODE_NUMBER5                     ] = { "KEY_5" };
		uinputCecMap[CEC_USER_CONTROL_CODE_NUMBER6                     ] = { "KEY_6" };
		uinputCecMap[CEC_USER_CONTROL_CODE_NUMBER7                     ] = { "KEY_7" };
		uinputCecMap[CEC_USER_CONTROL_CODE_NUMBER8                     ] = { "KEY_8" };
		uinputCecMap[CEC_USER_CONTROL_CODE_NUMBER9                     ] = { "KEY_9" };
		uinputCecMap[CEC_USER_CONTROL_CODE_DOT                         ] = { "KEY_DOT" };
		uinputCecMap[CEC_USER_CONTROL_CODE_ENTER                       ] = { "KEY_ENTER" };
		uinputCecMap[CEC_USER_CONTROL_CODE_CLEAR                       ] = { "KEY_BACKSPACE" };
		uinputCecMap[CEC_USER_CONTROL_CODE_NEXT_FAVORITE               ] = { ""};
		uinputCecMap[CEC_USER_CONTROL_CODE_CHANNEL_UP                  ] = { "KEY_CHANNELUP" };
		uinputCecMap[CEC_USER_CONTROL_CODE_CHANNEL_DOWN                ] = { "KEY_CHANNELDOWN" };
		uinputCecMap[CEC_USER_CONTROL_CODE_PREVIOUS_CHANNEL            ] = { "KEY_PREVIOUS" };
		uinputCecMap[CEC_USER_CONTROL_CODE_SOUND_SELECT                ] = { "KEY_SOUND" };
		uinputCecMap[CEC_USER_CONTROL_CODE_INPUT_SELECT                ] = { "KEY_TUNER" };
		uinputCecMap[CEC_USER_CONTROL_CODE_DISPLAY_INFORMATION         ] = { "KEY_INFO" };
		uinputCecMap[CEC_USER_CONTROL_CODE_HELP                        ] = { "KEY_HELP" };
		uinputCecMap[CEC_USER_CONTROL_CODE_PAGE_UP                     ] = { "KEY_PAGEUP" };
		uinputCecMap[CEC_USER_CONTROL_CODE_PAGE_DOWN                   ] = { "KEY_PAGEDOWN" };
		uinputCecMap[CEC_USER_CONTROL_CODE_POWER                       ] = { "KEY_POWER" };
		uinputCecMap[CEC_USER_CONTROL_CODE_VOLUME_UP                   ] = { "KEY_VOLUMEUP" };
		uinputCecMap[CEC_USER_CONTROL_CODE_VOLUME_DOWN                 ] = { "KEY_VOLUMEDOWN" };
		uinputCecMap[CEC_USER_CONTROL_CODE_MUTE                        ] = { "KEY_MUTE" };
		uinputCecMap[CEC_USER_CONTROL_CODE_PLAY                        ] = { "KEY_PLAY" };
		uinputCecMap[CEC_USER_CONTROL_CODE_STOP                        ] = { "KEY_STOP" };
		uinputCecMap[CEC_USER_CONTROL_CODE_PAUSE                       ] = { "KEY_PAUSE" };
		uinputCecMap[CEC_USER_CONTROL_CODE_RECORD                      ] = { "KEY_RECORD" };
		uinputCecMap[CEC_USER_CONTROL_CODE_REWIND                      ] = { "KEY_REWIND" };
		uinputCecMap[CEC_USER_CONTROL_CODE_FAST_FORWARD                ] = { "KEY_FASTFORWARD" };
		uinputCecMap[CEC_USER_CONTROL_CODE_EJECT                       ] = { "KEY_EJECTCD" };
		uinputCecMap[CEC_USER_CONTROL_CODE_FORWARD                     ] = { "KEY_FORWARD" };
		uinputCecMap[CEC_USER_CONTROL_CODE_BACKWARD                    ] = { "KEY_BACK" };
		uinputCecMap[CEC_USER_CONTROL_CODE_STOP_RECORD                 ] = { ""};
		uinputCecMap[CEC_USER_CONTROL_CODE_PAUSE_RECORD                ] = { ""};
		uinputCecMap[CEC_USER_CONTROL_CODE_ANGLE                       ] = { "KEY_SCREEN" };
		uinputCecMap[CEC_USER_CONTROL_CODE_SUB_PICTURE                 ] = { "KEY_SUBTITLE" };
		uinputCecMap[CEC_USER_CONTROL_CODE_VIDEO_ON_DEMAND             ] = { "KEY_VIDEO" };
		uinputCecMap[CEC_USER_CONTROL_CODE_ELECTRONIC_PROGRAM_GUIDE    ] = { "KEY_EPG" };
		uinputCecMap[CEC_USER_CONTROL_CODE_TIMER_PROGRAMMING           ] = { "KEY_TIME" };
		uinputCecMap[CEC_USER_CONTROL_CODE_INITIAL_CONFIGURATION       ] = { "KEY_CONFIG" };
		uinputCecMap[CEC_USER_CONTROL_CODE_PLAY_FUNCTION               ] = { ""};
		uinputCecMap[CEC_USER_CONTROL_CODE_PAUSE_PLAY_FUNCTION         ] = { ""};
		uinputCecMap[CEC_USER_CONTROL_CODE_RECORD_FUNCTION             ] = { ""};
		uinputCecMap[CEC_USER_CONTROL_CODE_PAUSE_RECORD_FUNCTION       ] = { ""};
		uinputCecMap[CEC_USER_CONTROL_CODE_STOP_FUNCTION               ] = { ""};
		uinputCecMap[CEC_USER_CONTROL_CODE_MUTE_FUNCTION               ] = { ""};
		uinputCecMap[CEC_USER_CONTROL_CODE_RESTORE_VOLUME_FUNCTION     ] = { ""};
		uinputCecMap[CEC_USER_CONTROL_CODE_TUNE_FUNCTION               ] = { ""};
		uinputCecMap[CEC_USER_CONTROL_CODE_SELECT_MEDIA_FUNCTION       ] = { "KEY_MEDIA" };
		uinputCecMap[CEC_USER_CONTROL_CODE_SELECT_AV_INPUT_FUNCTION    ] = { ""};
		uinputCecMap[CEC_USER_CONTROL_CODE_SELECT_AUDIO_INPUT_FUNCTION ] = { ""};
		uinputCecMap[CEC_USER_CONTROL_CODE_POWER_TOGGLE_FUNCTION       ] = { ""};
		uinputCecMap[CEC_USER_CONTROL_CODE_POWER_OFF_FUNCTION          ] = { ""};
		uinputCecMap[CEC_USER_CONTROL_CODE_POWER_ON_FUNCTION           ] = { ""};
		uinputCecMap[CEC_USER_CONTROL_CODE_F1_BLUE                     ] = { "KEY_BLUE" };
		uinputCecMap[CEC_USER_CONTROL_CODE_F2_RED                      ] = { "KEY_RED" };
		uinputCecMap[CEC_USER_CONTROL_CODE_F3_GREEN                    ] = { "KEY_GREEN" };
		uinputCecMap[CEC_USER_CONTROL_CODE_F4_YELLOW                   ] = { "KEY_YELLOW" };
		uinputCecMap[CEC_USER_CONTROL_CODE_F5                          ] = { ""};
		uinputCecMap[CEC_USER_CONTROL_CODE_DATA                        ] = { "KEY_TEXT" };
		uinputCecMap[CEC_USER_CONTROL_CODE_AN_RETURN                   ] = { "KEY_ESC" };
		uinputCecMap[CEC_USER_CONTROL_CODE_AN_CHANNELS_LIST            ] = { "KEY_LIST" };
	}

	return uinputCecMap;
}

int Main::onCecLogMessage(const cec_log_message &message) {
	LOG4CPLUS_DEBUG(logger, "Main::onCecLogMessage(" << message << ")");
	return 1;
}

void Main::writeLirc(const cec_keypress &key, const string &keyString, const bool &repeat) {
	stringstream s;

	s << "" << hex << key.keycode << " " << repeat << " " << keyString << " RPICEC" << endl;
	mylirc.processevent(s.str().c_str());

}

int Main::onCecKeyPress(const cec_keypress &key) {
	LOG4CPLUS_DEBUG(logger, "Main::onCecKeyPress(" << key << ")");

	// Check bounds and find uinput code for this cec keypress
	if (key.keycode >= 0 && key.keycode <= CEC_USER_CONTROL_CODE_MAX) {
		const list<string> & uinputKeys = uinputCecMap[key.keycode];

		if ( !uinputKeys.empty() ) {
			if( key.duration == 0 || key.keycode == CEC_USER_CONTROL_CODE_AN_CHANNELS_LIST || key.keycode == CEC_USER_CONTROL_CODE_AN_RETURN) {
				/*
				** KEY PRESSED
				*/
				for (std::list<string>::const_iterator ukeys = uinputKeys.begin(); ukeys != uinputKeys.end(); ++ukeys) {
					string ukey = *ukeys;

					LOG4CPLUS_DEBUG(logger, "pressed " << ukey);
					writeLirc(key, ukey, 0);

				}
				lastUInputKeys = uinputKeys;
				
			}
		}
	}

	return 1;
}

#ifdef OLD
int Main::onCecKeyPress(const cec_keypress &key) {
	LOG4CPLUS_DEBUG(logger, "Main::onCecKeyPress(" << key << ")");

	// Check bounds and find uinput code for this cec keypress
	if (key.keycode >= 0 && key.keycode <= CEC_USER_CONTROL_CODE_MAX) {
		const list<string> & uinputKeys = uinputCecMap[key.keycode];

		if ( !uinputKeys.empty() ) {
			if( key.duration == 0 ) {
				if( uinputKeys == lastUInputKeys )
				{
					/*
					** KEY REPEAT
					*/
					for (std::list<string>::const_iterator ukeys = uinputKeys.begin(); ukeys != uinputKeys.end(); ++ukeys) {
						string ukey = *ukeys;

						LOG4CPLUS_DEBUG(logger, "repeat " << ukey);

						// uinput.send_event(EV_KEY, ukey, EV_KEY_REPEAT);
						writeLirc(key, ukey, 1);
					}
				} else {
					/*
					** KEY PRESSED
					*/
					if( ! lastUInputKeys.empty() ) {
						/* what happened with the last key release ? */
						for (std::list<string>::const_iterator ukeys = lastUInputKeys.begin(); ukeys != lastUInputKeys.end(); ++ukeys) {
							string ukey = *ukeys;

							LOG4CPLUS_DEBUG(logger, "release " << ukey);

//							uinput.send_event(EV_KEY, ukey, EV_KEY_RELEASED);
							writeLirc(key, ukey, 0);
						}
					}
					for (std::list<string>::const_iterator ukeys = uinputKeys.begin(); ukeys != uinputKeys.end(); ++ukeys) {
						string ukey = *ukeys;

						LOG4CPLUS_DEBUG(logger, "send " << ukey);

//						uinput.send_event(EV_KEY, ukey, EV_KEY_PRESSED);
						writeLirc(key, ukey, 0);

					}
					lastUInputKeys = uinputKeys;
				}
			} else {
				if( lastUInputKeys != uinputKeys ) {
					if( ! lastUInputKeys.empty() ) {
						/* what happened with the last key release ? */
						for (std::list<string>::const_iterator ukeys = lastUInputKeys.begin(); ukeys != lastUInputKeys.end(); ++ukeys) {
							string ukey = *ukeys;

							LOG4CPLUS_DEBUG(logger, "release " << ukey);

//							uinput.send_event(EV_KEY, ukey, EV_KEY_RELEASED);
							writeLirc(key, ukey, 0);
						}
					}
					for (std::list<string>::const_iterator ukeys = uinputKeys.begin(); ukeys != uinputKeys.end(); ++ukeys) {
						string ukey = *ukeys;

						LOG4CPLUS_DEBUG(logger, "send " << ukey);

//						uinput.send_event(EV_KEY, ukey, EV_KEY_PRESSED);
						writeLirc(key, ukey, 0);

					}
//					boost::this_thread::sleep(boost::posix_time::milliseconds(100));
					usleep(100);
				}
				/*
				** KEY RELEASED
				*/
				for (std::list<string>::const_iterator ukeys = uinputKeys.begin(); ukeys != uinputKeys.end(); ++ukeys) {
					string ukey = *ukeys;

					LOG4CPLUS_DEBUG(logger, "release " << ukey);

//					uinput.send_event(EV_KEY, ukey, EV_KEY_RELEASED);
					writeLirc(key, ukey, 0);

				}
				lastUInputKeys.clear();
			}
		}
	}

	return 1;
}
#endif
int Main::onCecKeyPress(const cec_user_control_code & keycode) {
	cec_keypress key = { .keycode=keycode };

	/* PUSH KEY */
	key.duration = 0;
	onCecKeyPress( key );

	/* simulate delay */
	key.duration = 100;
	usleep(key.duration);

	/* RELEASE KEY */
	onCecKeyPress( key );

	return 1;
}

int Main::onCecCommand(const cec_command & command) {
	LOG4CPLUS_DEBUG(logger, "Main::onCecCommand(" << command << ")");
	switch( command.opcode )
	{
		case CEC_OPCODE_STANDBY:
//			if( (command.initiator == CECDEVICE_TV)
//                         && ( (command.destination == CECDEVICE_BROADCAST) || (command.destination == logicalAddress))  )
//			{
				push(Command(COMMAND_STANDBY));
//			}
			break;
		case CEC_OPCODE_REQUEST_ACTIVE_SOURCE:
//			if( (command.initiator == CECDEVICE_TV)
//                        && ( (command.destination == CECDEVICE_BROADCAST) || (command.destination == logicalAddress))  )
//			{
                if( makeActive )
                {
                    /* remind TV we are active */
                    push(Command(COMMAND_ACTIVE));
                }
//			}
		case CEC_OPCODE_SET_MENU_LANGUAGE:
			if( (command.initiator == CECDEVICE_TV) && (command.parameters.size == 3)
                         && ( (command.destination == CECDEVICE_BROADCAST) || (command.destination == logicalAddress))  )
			{
				/* TODO */
			}
			break;
		case CEC_OPCODE_DECK_CONTROL:
//			if( (command.initiator == CECDEVICE_TV) && (command.parameters.size == 1)
//                         && ( (command.destination == CECDEVICE_BROADCAST) || (command.destination == logicalAddress))  )
//			{
				if( command.parameters[0] == CEC_DECK_CONTROL_MODE_STOP ) {
					push(Command(COMMAND_KEYPRESS, CEC_USER_CONTROL_CODE_STOP));
				}
//			}
			break;
		case CEC_OPCODE_PLAY:
			LOG4CPLUS_DEBUG(logger, "Main::onCecCommand(CEC_OPCODE_PLAY)");
//			if( (command.initiator == CECDEVICE_TV) && (command.parameters.size == 1)
//                         && ( (command.destination == CECDEVICE_BROADCAST) || (command.destination == logicalAddress))  )
//			{
				if( command.parameters[0] == CEC_PLAY_MODE_PLAY_FORWARD ) {
					push(Command(COMMAND_KEYPRESS, CEC_USER_CONTROL_CODE_PLAY));
				}
				else if( command.parameters[0] == CEC_PLAY_MODE_PLAY_STILL ) {
					push(Command(COMMAND_KEYPRESS, CEC_USER_CONTROL_CODE_PAUSE));
				} else
					push(Command(COMMAND_KEYPRESS, CEC_USER_CONTROL_CODE_PLAY));
//			}
			break;
		case CEC_OPCODE_VENDOR_REMOTE_BUTTON_UP:
			LOG4CPLUS_DEBUG(logger, "Main::onCecCommand(CEC_OPCODE_VENDOR_REMOTE_BUTTON_UP)"); 
			break;
		default:
			LOG4CPLUS_DEBUG(logger, "Main::onCecCommand(UNKONWN) opcode=" << hex << command.opcode); 
			break;
	}
	return 1;
}

int Main::onCecAlert(const CEC::libcec_alert alert, const CEC::libcec_parameter & param) {
	LOG4CPLUS_ERROR(logger, "Main::onCecAlert(alert=" << alert << ")");
	switch( alert )
	{
		case CEC_ALERT_SERVICE_DEVICE:
			break;
		case CEC_ALERT_CONNECTION_LOST:
		case CEC_ALERT_PERMISSION_ERROR:
		case CEC_ALERT_PORT_BUSY:
		case CEC_ALERT_PHYSICAL_ADDRESS_ERROR:
		case CEC_ALERT_TV_POLL_FAILED:
			Main::instance().restart();
			break;
		default:
			break;
	}
	return 1;
}

int Main::onCecConfigurationChanged(const libcec_configuration & configuration) {
	//LOG4CPLUS_DEBUG(logger, "Main::onCecConfigurationChanged(" << configuration << ")");
	LOG4CPLUS_DEBUG(logger, "Main::onCecConfigurationChanged(logicalAddress=" << configuration.logicalAddresses.primary << ")");
	logicalAddress = configuration.logicalAddresses.primary;
	return 1;
}


int Main::onCecMenuStateChanged(const cec_menu_state & menu_state) {
	LOG4CPLUS_DEBUG(logger, "Main::onCecMenuStateChanged(" << menu_state << ")");

	return onCecKeyPress(CEC_USER_CONTROL_CODE_CONTENTS_MENU);
}

void Main::onCecSourceActivated(const cec_logical_address & address, bool bActivated) {
	LOG4CPLUS_DEBUG(logger, "Main::onCecSourceActivated(logicalAddress " << address << " = " << bActivated << ")");
	if( logicalAddress == address )
	{
		if( bActivated )
		{
			push(Command(COMMAND_ACTIVE));
		}
		else
		{	
			push(Command(COMMAND_INACTIVE));
		}
	}
}


int main (int argc, char *argv[]) {

    BasicConfigurator config;
    config.configure();

	int opt;
    	int loglevel = -1;
	bool foreground = false;
	bool list = false;
	bool dontactivate = false;
	string lircpath;
	
	while((opt = getopt(argc, argv, "hVflv:ai:")) != -1) {
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
			case 'V':
			case 'h':
            default:
		cout << "Usage: " << argv[0] << " [options] " << endl << endl;
		cout << "Options:" << endl;
		cout << "\t-d <socket> UNIX socket. The default is /var/run/lirc/lircd." << endl;
		cout << "\t-f Run in the foreground." << endl;
		cout << "\t-l list cec devices" << endl;
		cout << "\t-a do not activate" << endl;
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
        string device = "";

		if (dontactivate) {
			main.setMakeActive(false);
		}

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

		main.loop(device);

	} catch (std::exception & e) {
		cerr << e.what() << endl;
		return -1;
	}

	return 0;
}

