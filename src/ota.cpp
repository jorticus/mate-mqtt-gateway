
#include "ota.h"
#include "debug.h"
#include <ArduinoOTA.h>

#if 0

bool OTA::is_updating = false;

void OTA::Setup(Config& config)
{
	// Port defaults to 8266
	// ArduinoOTA.setPort(8266);

	// Hostname defaults to esp8266-[ChipID]
	ArduinoOTA.setHostname(config.device_name);
	//ArduinoOTA.setHostname("myesp82662");
	Debug.print("Host Name:");
	Debug.println(config.device_name);

	// No authentication by default
	// ArduinoOTA.setPassword((const char *)"123");

	// Password can be set with it's md5 value as well
	// MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
	// ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

	ArduinoOTA.onStart([]() {
		is_updating = true;

		// Set system to a safe state
		setup_platform();

		SetAppStatus(AppStatus::Programming);

		Debug.println("OTA Initiated");
		Debug.println("Flashing...");
	});

	ArduinoOTA.onEnd([]() {
		Debug.println("\nOTA Done!");
		is_updating = false;

		//Led::Clear();
		SetAppStatus(AppStatus::ProgrammingSuccess);
		delay(1000);

		// WARNING:
		// OTA may hang here if you have just flashed via UART.
		// To avoid this, please reset the ESP after flashing directly.
	});

	ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
		// static int last_x = 0;
		// int x = (progress / (total / 20));
		// if (x != last_x) {
		// 	Debug.print("#");
		// 	last_x = x;
		// }
		// if (progress == total) {
		// 	Debug.println();
		// }

		Led::SetProgress(Colors::Blue, (progress * 100) / total);
	});

	ArduinoOTA.onError([](ota_error_t error) {
		Debug.println();
		Debug.printf("Error[%u]: ", error);

		//Led::Clear();
		SetAppStatus(AppStatus::Faulted);

		if (error == OTA_AUTH_ERROR) Debug.println("Auth Failed");
		else if (error == OTA_BEGIN_ERROR) Debug.println("Begin Failed");
		else if (error == OTA_CONNECT_ERROR) Debug.println("Connect Failed");
		else if (error == OTA_RECEIVE_ERROR) Debug.println("Receive Failed");
		else if (error == OTA_END_ERROR) Debug.println("End Failed");
		is_updating = false;

		delay(1000);
		ESP.restart();
	});

	// Enable OTA
	ArduinoOTA.begin();
}

#endif