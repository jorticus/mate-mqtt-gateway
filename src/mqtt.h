#pragma once

#include <PubSubClient.h>
#include <hacomponent.h>

class Mqtt
{
public:
    static void autodetectSetup();
    static bool find_remote_broker(IPAddress* addr_out, uint16_t* port_out);
    static void setup(IPAddress addr, uint16_t port);
    static bool process();
    static void connect();

    static PubSubClient client;
    static ComponentContext context;
private:
    static void on_message_received(char* topic, byte* payload, size_t len);
    


};
