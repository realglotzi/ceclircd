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

#include <cstddef>
#include <libcec/cec.h>

#include <memory>
#include <map>
#include <string>

namespace HDMI {
	class physical_address;
	class address;
}

class CecCallback {
	public:
		virtual ~CecCallback() {}

		// Virtual methods to handle callbacks
		virtual int onCecLogMessage(const CEC::cec_log_message & message) = 0;
		virtual int onCecKeyPress  (const CEC::cec_keypress & key) = 0;
		virtual int onCecCommand   (const CEC::cec_command & command) = 0;
		virtual int onCecConfigurationChanged(const CEC::libcec_configuration & configuration) = 0;
		virtual int onCecAlert(const CEC::libcec_alert alert, const CEC::libcec_parameter & param) = 0;
		virtual int onCecMenuStateChanged(const CEC::cec_menu_state & menu_state) = 0;
		virtual void onCecSourceActivated(const CEC::cec_logical_address & address, bool bActivated) = 0;
};

/**
 * Simple wrapper class around libcec
 */
class Cec {

	private:

		static std::map<CEC::cec_user_control_code, const char *> & setupUserControlCodeName();

		// Members for the libcec interface
		CEC::ICECCallbacks callbacks;
		CEC::libcec_configuration config;

		std::unique_ptr<CEC::ICECAdapter> cec;

		// Inits the CECAdapter 
		void init();

	public:

		const static std::map<CEC::cec_user_control_code, const char *> cecUserControlCodeName;

		Cec(const char *name, CecCallback *callback);
		virtual ~Cec();

		/**
		 * List all found adapters and prints them out
		 */
		std::ostream & listDevices(std::ostream & out);

		/**
		 * Opens the first adapter it finds
		 */
		void open(const std::string &adapter = "");

		/**
		 * Closes the open adapter
		 */
		void close(bool makeInactive = true);

		void makeActive();
		void setTargetAddress(const HDMI::address & address);
		bool ping();

	// These are just wrapper functions, to map C callbacks to C++
	friend int cecLogMessage (void *cbParam, const CEC::cec_log_message &message);
	friend int cecKeyPress   (void *cbParam, const CEC::cec_keypress &key);
	friend int cecCommand    (void *cbParam, const CEC::cec_command &command);
	friend int cecConfigurationChanged (void *cbParam, const CEC::libcec_configuration & configuration);
	friend int cecAlert(void *cbParam, const CEC::libcec_alert alert, const CEC::libcec_parameter & param);
	friend int cecMenuStateChanged(void *cbParam, const CEC::cec_menu_state & menu_state);
	friend void cecSourceActivated(void *cbParam, const CEC::cec_logical_address & address, const uint8_t bActivated);
};


// Some helper << methods
std::ostream& operator<<(std::ostream &out, const CEC::cec_log_message & message);
std::ostream& operator<<(std::ostream &out, const CEC::cec_keypress & key);
std::ostream& operator<<(std::ostream &out, const CEC::cec_command & command);
std::ostream& operator<<(std::ostream &out, const CEC::libcec_configuration & configuration);
