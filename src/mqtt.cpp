#include "mqtt.h"
#include "debug.h"
#include "main.h"
#include "secrets.h"
#include "hacomponent.h"

#include <ESPmDNS.h>

bool Mqtt::find_remote_broker(IPAddress* addr_out, uint16_t* port_out)
{
    Debug.println();
    Debug.println("Looking for MQTT broker via mDNS");

    int n = MDNS.queryService("mqtt", "tcp");

    if (n <= 0) {
        return false;
    }
    else {
        int i = 0;
        //Debug.println("MQTT broker(s):");
        for (i = 0; i < n; i++) {
            Debug.print(i + 1);
            Debug.print(": ");
            Debug.print(MDNS.hostname(i));
            Debug.print(" (");
            Debug.print(MDNS.IP(i));
            Debug.print(":");
            Debug.print(MDNS.port(i));
            Debug.println(")");
        }

        if (i > 1) {
            Debug.println("WARNING: More than one broker found, using (1)");
        }

        i = 0; // Just use the first entry

        if (addr_out != nullptr)
            *addr_out = MDNS.IP(i);
        if (port_out != nullptr)
            *port_out = MDNS.port(i);

        return true;
    }
}

void Mqtt::autodetectSetup()
{
    IPAddress mqttBrokerHost;
    uint16_t mqttBrokerPort;

    //SetAppStatus(AppStatus::ConnectingMqtt);

    // Keep looking until we find a broker
    bool foundBroker = false;
    do
    {
        foundBroker = find_remote_broker(&mqttBrokerHost, &mqttBrokerPort);
        if (!foundBroker) {
            Debug.println("Could not find MQTT broker");

            // Try again
            delay(5000);

            // Ensure OTA has a chance to run so we can recover if needed
            idle_loop();
        }
    } while (!foundBroker);

    // // Save detected config so we don't have to re-discover on next reboot
    // config.set_mqtt_server(mqttBrokerHost);
    // config.set_mqtt_port(mqttBrokerPort);
    // config.save();

    setup(mqttBrokerHost, mqttBrokerPort);
}

void Mqtt::setup(IPAddress addr, uint16_t port)
{
    Debug.print("MQTT Broker: ");
    Debug.print(addr);
    Debug.print(":");
    Debug.print(port);
    Debug.println();

    client.setServer(addr, port);
    client.setCallback(Mqtt::on_message_received);
}

void Mqtt::setup(const char* host, uint16_t port)
{
    Debug.print("MQTT Broker: ");
    Debug.print(host);
    Debug.print(":");
    Debug.print(port);
    Debug.println();

    client.setServer(host, port);
    client.setCallback(Mqtt::on_message_received);
}

void Mqtt::connect()
{
    // Loop until we're reconnected
    while (!client.connected()) {
        //SetAppStatus(AppStatus::ConnectingMqtt);
        Debug.print("Attempting MQTT connection...");

        // Attempt to connect
        bool connected = false;
        if (HAAvailabilityComponent::inst != nullptr)
        {
            String will_topic       = HAAvailabilityComponent::inst->getWillTopic();
            const char* will_msg    = HAAvailabilityComponent::OFFLINE;
            uint8_t will_qos        = 0;
            bool will_retain        = true;

            Debug.print("Last will: ");
            Debug.print(will_topic);
            Debug.print(" = ");
            Debug.println(will_msg);

            connected = client.connect(
                secrets::device_name,
                secrets::mqtt_username,
                secrets::mqtt_password,
                will_topic.c_str(), will_qos, will_retain, will_msg
            );
        }
        else
        {
            connected = client.connect(
                secrets::device_name,
                secrets::mqtt_username,
                secrets::mqtt_password
            );
        }

        if (connected)
        {
            //SetAppStatus(AppStatus::Connected);
            Debug.println("Connected");

            // Verify server's certificate
            // TODO: Can we verify before passing the user/password?
            /*if (!espClient.verifyCertChain(mqtt_server)) {
            Debug.println("ERROR: Certificate verification failed!");
            return;
            }*/

            //client.subscribe("homeassistant/#");
            //publishSensors();

            // TODO: Report current sensor values immediately

            // Invalidate readings
            /*lastPirState = false;
            temp = 0.0;
            hum = 0.0;*/

        }
        else
        {
            Debug.print("Failed (");
            Debug.print(client.state());
            Debug.println(").");

            // Wait 5 seconds before retrying
            for (int i = 0; i < 5000; i++) {
                delay(1);
                idle_loop();
            }

            idle_loop();
        }
    }
}

bool Mqtt::process()
{
    if (!client.connected()) {
        // Lost connection, attempt to re-establish
        connect();
        
        return true; // Reconnected
    }
    client.loop();
    return false;
}

void Mqtt::on_message_received(char* topic, byte* payload, size_t len)
{
    payload[len] = '\0';
    Debug.println((char*)payload);

    // String topic_s(topic);
    // String payload_s((char*)payload);
    // HAComponent<Component::Switch>::ProcessMqttTopic(topic_s, payload_s);
}
