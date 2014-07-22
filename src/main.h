#include "libcec.h"
#include "lirc.hpp"
#include <limits.h>

class Main : public CecCallback {

	private:

		lirc mylirc;
		
		// Main controls
		Cec cec;
		char cec_name[HOST_NAME_MAX];

		unsigned int repeat_time;
		
		// Some config params
		bool makeActive;

		//
		bool running; // TODO Change this to be threadsafe!. Voiatile or better

		//
		Main();
		virtual ~Main();

		// Not implemented to avoid copying the singleton
		Main(Main const&);
		void operator=(Main const&);

		static void signalHandler(int sigNum);

		char *getCecName();

	public:

		int onCecLogMessage(const CEC::cec_log_message &message);
		int onCecKeyPress(const CEC::cec_keypress &key);
		int onCecCommand(const CEC::cec_command &command);
		int onCecConfigurationChanged(const CEC::libcec_configuration & configuration);

		static Main & instance();

		void loop();
		void stop();

		void listDevices();

		void setMakeActive(bool active) {this->makeActive = active;};
		void setRepeatTime(int time) {this->repeat_time = time;};
		void setLircPath(string lircpath) {this->mylirc.device = lircpath;};
};
