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

#include "libcec.h"
#include "lirc.h"
#include <limits.h>
#include <string>
#include <queue>
#include <list>

class Command
{
	public:
		Command(int command, CEC::cec_user_control_code keycode=CEC::CEC_USER_CONTROL_CODE_UNKNOWN) : command(command), keycode(keycode) {};
		~Command() {};

		const int command;
		union
		{
			const CEC::cec_user_control_code keycode;
		};

};

class Main : public CecCallback {

	private:

		lirc mylirc;
		
		// Main controls
		Cec cec;
		char cec_name[HOST_NAME_MAX];

		// Some config params
		bool makeActive;
		bool running;

		//
		std::list<string> lastUInputKeys; // for key(s) repetition

		//
		Main();
		virtual ~Main();

		// Not implemented to avoid copying the singleton
		Main(Main const&);
		void operator=(Main const&);

		static void signalHandler(int sigNum);

		static const std::vector<std::list<string>> & setupUinputMap();
		std::queue<Command> commands;

		std::string onStandbyCommand;
		std::string onActivateCommand;
		std::string onDeactivateCommand;

		CEC::cec_logical_address logicalAddress;

		char *getCecName();

		void push(Command command);

		void writeLirc(const CEC::cec_keypress &key, const string &keyString, const bool &repeat);
	public:

		static const std::vector<std::list<string>> uinputCecMap;

		int onCecLogMessage(const CEC::cec_log_message &message);
		int onCecKeyPress(const CEC::cec_keypress &key);
		int onCecKeyPress(const CEC::cec_user_control_code & keycode);
		int onCecCommand(const CEC::cec_command &command);
		int onCecConfigurationChanged(const CEC::libcec_configuration & configuration);
		int onCecAlert(const CEC::libcec_alert alert, const CEC::libcec_parameter & param);
		int onCecMenuStateChanged(const CEC::cec_menu_state & menu_state);
		void onCecSourceActivated(const CEC::cec_logical_address & address, bool isActivated);

		static Main & instance();

		void loop(const std::string &device = "");
		void stop();
		void restart();

		void listDevices();

		void setMakeActive(bool active) {this->makeActive = active;};
		void setOnStandbyCommand(const std::string &cmd) {this->onStandbyCommand = cmd;};
		void setOnActivateCommand(const std::string &cmd) {this->onActivateCommand = cmd;};
		void setOnDeactivateCommand(const std::string &cmd) {this->onDeactivateCommand = cmd;};
		void setTargetAddress(const HDMI::address & address) {cec.setTargetAddress(address);};

		void setLircPath(string lircpath) {this->mylirc.device = lircpath;};
};
