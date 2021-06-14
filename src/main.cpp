
#include <Arduino.h>
//#include <TelnetSpy.h>
#include <ArduinoOTA.h>
#include <Serial9b.h>
#include <SoftwareSerial.h>

#include "mate.h"

// Debugging is available at port 23 (raw connection)
//static TelnetSpy telnet;
Stream& Debug = Serial;//telnet;

// Generated by version.py
extern const char* GEN_BUILD_VERSION;

boolean g_failsafe = false;

// The ESP32 hardware UARTs unfortunately don't support 9-bit data,
// so we are forced to use a software serial implementation.
SoftwareSerial Serial9b;

#define MATE_TX (19)
#define MATE_RX (23)


void fault() {
    Debug.println("HALTED.");
    while (true) { }
}

void setup() {
    // // Debugging
    // telnet.setWelcomeMsg("Connected!");
    // telnet.setCallbackOnConnect([] {
    //     Serial.print("Telnet connected");
    // });
    // telnet.setCallbackOnDisconnect([] {
    //     Serial.print("Telnet disconnected");
    // });
    // telnet.setDebugOutput(false);
    // telnet.begin(serial_baud);

    Serial.begin(115200);
    //Serial1.begin(115200);

    Debug.println();

    Debug.println("Booting");

    Debug.print("FW Version: ");
    Debug.println(GEN_BUILD_VERSION);

    Debug.print("Chip:       ");
    Debug.println(ESP.getChipModel());

    Debug.print("SDK:        ");
    Debug.println(ESP.getSdkVersion());

    //Debug.print("Chip ID:    0x");
    //Debug.println(ESP.getChipId(), HEX);

    // TODO: Equivalent for ESP32?
    // // Detect if previous reset was due to an Exception,
    // // so we can go into a failsafe mode with only WiFi + OTA.
    // auto rst = ESP.getResetInfoPtr();
    // if (rst->reason == 2) { // 2: EXCEPTION
    //     Debug.println();
    //     Debug.println("!! PREVIOUS RESET WAS CAUSED BY EXCEPTION !!");
    //     g_failsafe = true;
    // }


    Serial9b.begin(9600, MATE_RX, MATE_TX, SWSERIAL_9N1, false);
    Serial9b.enableRx(true);
    Serial9b.enableTx(true);

    Debug.println();

    MateAggregator::setup();
}

void idle_loop() {
    //Led::Process();
    ArduinoOTA.handle();
    yield();
}


void loop() {
    //telnet.handle();
    ArduinoOTA.handle();

    MateAggregator::loop();

    //if (!OTA::is_updating && !g_failsafe) {
    //   bool reconnected = Mqtt::Process(config);
    //   if (reconnected) {
    //   publish();
    //   SetAppStatus(AppStatus::Sensing);
    //   }
    //}
}
