#include "libcec.h"
#include "lirc.hpp"
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
