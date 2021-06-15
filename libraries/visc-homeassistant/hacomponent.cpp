
#include "hacomponent.h"
#include <math.h>
#include <WiFi.h>
#include <ArduinoJson.h>

extern Stream& Debug;

extern const char* g_friendly_name;
extern const char* g_fw_version;

template<Component c> const char* HACompBase<c>::m_component                        = nullptr;
template<>            const char* HACompBase<Component::Sensor>::m_component        = "sensor";
template<>            const char* HACompBase<Component::BinarySensor>::m_component  = "binary_sensor";
template<>            const char* HACompBase<Component::Switch>::m_component        = "switch";

template<SensorClass c> const char* SensorClassString<c>::name = nullptr;
template<>              const char* SensorClassString<SensorClass::Undefined>::name = nullptr;
template<>              const char* SensorClassString<SensorClass::Temperature>::name = "temperature";
template<>              const char* SensorClassString<SensorClass::Humidity>::name = "humidity";
template<>              const char* SensorClassString<SensorClass::Battery>::name = "battery";
template<>              const char* SensorClassString<SensorClass::Illuminance>::name = "illuminance";
template<>              const char* SensorClassString<SensorClass::Pressure>::name = "pressure";

// template<BinarySensorClass c> const char* BinarySensorClass<c>::name = nullptr;
// template<>                    const char* BinarySensorClass<BinarySensorClass::connectivity>::name = "connectivity";
// // TODO...

// Static instantiations
std::vector<HACompItem*>                        HACompItem::m_components;
std::vector<HAComponent<Component::Switch>*>    HAComponent<Component::Switch>::m_switches;
static HAComponent<Component::BinarySensor>*    s_component = nullptr;

// Warning: HomeAssistant is case sensitive! These are the default state values...
const char*                                     HAComponent<Component::Switch>::ON = "ON";
const char*                                     HAComponent<Component::Switch>::OFF = "OFF";

HAAvailabilityComponent*                        HAAvailabilityComponent::inst = nullptr;
const char*                                     HAAvailabilityComponent::ONLINE = "online";
const char*                                     HAAvailabilityComponent::OFFLINE = "offline";


static void getDeviceInfo(JsonObject& json, ComponentContext& context) {
    auto& deviceInfo = json.createNestedObject("device");

    String id = WiFi.macAddress();

    deviceInfo["identifiers"]   = id;
    deviceInfo["name"]          = context.device_name;
    deviceInfo["sw_version"]    = context.fw_version;
    deviceInfo["model"]         = context.model;
    deviceInfo["manufacturer"]  = context.manufacturer;

    // This tells HA if the component is available (connected) or not.
    if (HAAvailabilityComponent::inst != nullptr) {
        // NOTE: This doesn't seem to have any effect as long as you make sure the availability
        // sensor is published with the same device information as other sensors...
        //json["availability_topic"] = HAAvailabilityComponent::inst->getWillTopic();
    }
}

template<Component c>
void HACompBase<c>::getConfigInfo(JsonObject& json)
{
}

// template<Component c>
// String HACompBase<c>::getStatusTopic(JsonObject& json)
// {
//     char state_topic[TOPIC_BUFFER_SIZE];

//     snprintf(state_topic, sizeof(state_topic), 
//         "%s/%s/%s/state\0", 
//         config.device_name,
//         m_component, 
//         m_name);

//     return String(state_topic);
// }

template<Component c>
void HACompBase<c>::Initialize()
{
    char state_topic[TOPIC_BUFFER_SIZE];
    snprintf(state_topic, sizeof(state_topic), 
        "%s/%s/%s/state\0", 
        context.device_name, m_component, m_name);
    m_state_topic = String(state_topic);
}

// Generic publish implementation used for all component types
template<Component c>
void HACompBase<c>::PublishConfig(bool present)
{
    // Generic implementation
    char topic[TOPIC_BUFFER_SIZE];

    snprintf(topic, sizeof(topic), 
        "homeassistant/%s/%s/%s/config\0", 
        m_component, context.device_name, m_name);

    if (present) {
        StaticJsonBuffer<JSON_BUFFER_SIZE> jsonBuffer;
        JsonObject& json = jsonBuffer.createObject();

        //Led::SetBuiltin(true);

        json["name"]    = m_name;
        json["stat_t"]  = m_state_topic;

        // For a complete list of JSON parameters you can set, see:
        // https://www.home-assistant.io/docs/mqtt/discovery/
        getConfigInfo(json);

        // // Prepend the AirNode friendly name to the sensor name
        // char name[TOPIC_BUFFER_SIZE];
        // snprintf(name, sizeof(name),
        //     "%s %s",
        //     context.friendly_name,
        //     json.get<char*>("name"));
        // json["name"] = String((const char*)name);

        // Add unique ID for component
        char uid[TOPIC_BUFFER_SIZE];
        snprintf(uid, sizeof(uid),
            "%s_%s\0",
            context.device_name, m_name);
        json["unique_id"] = uid;

        // Use the unique name instead of friendly name for entity IDs
        json["name"] = String(uid);

        // Add device information
        getDeviceInfo(json, context);

        String payload;
        json.printTo(payload);

        Debug.print("publish: ");
        Debug.print(topic);
        if (!present) {
            Debug.print(" (not present)");
        } else {
            //Debug.print(" -> ");
            //Debug.print(payload.c_str());
        }
        Debug.println();

        if (!context.client.publish(topic, payload.c_str(), true)) {
            Debug.println("ERROR PUBLISHING TOPIC");
        }
    } 
    else {
        // If not present, we should unpublish the topic
        Debug.print("unpublish: ");
        Debug.println(topic);

        // IMPORTANT: Use 4-arg overload. The 2 & 3-arg overloads try to call strlen() on payload
        context.client.publish(topic, nullptr, 0, true);

        // Also unpublish the parent node
        snprintf(topic, sizeof(topic), 
            "homeassistant/%s/%s/%s\0", 
            m_component, context.device_name, m_name);
        context.client.publish(topic, nullptr, 0, true);

        // And finally clear the current state
        // (so it's clear the last retained sensor value is no longer valid)
        ClearState();
    }

    //Led::SetBuiltin(false);
}

HAComponent<Component::Switch>::HAComponent(ComponentContext& context, const char* name, std::function<void(boolean)> callback) :
    HACompBase(context, name),
    m_state(false),
    m_callback(callback)
{
    HAComponent<Component::Switch>::m_switches.push_back(this);
}

void HAComponent<Component::Switch>::Initialize()
{
    HACompBase<Component::Switch>::Initialize();

    char cmd_topic[TOPIC_BUFFER_SIZE];
    snprintf(cmd_topic, sizeof(cmd_topic), 
        "%s/%s/%s/ctrl\0", 
        context.device_name, m_component, m_name);
    m_cmd_topic = String(cmd_topic);
}

// Topic specialization for switch components
void HAComponent<Component::Switch>::getConfigInfo(JsonObject& json)
{
    // https://www.home-assistant.io/components/switch.mqtt/

    json["cmd_t"]   = m_cmd_topic; // "command_topic"

    context.client.subscribe(m_cmd_topic.c_str());

    ReportState();
}

void HAComponent<Component::Switch>::SetState(bool state)
{
    m_state = state;
    m_callback(state);

    ReportState();
}

void HAComponent<Component::Switch>::ReportState()
{
    context.client.publish(m_state_topic.c_str(), (m_state ? ON : OFF), true);
}

void HAComponent<Component::Switch>::ProcessMqttTopic(String& topic, String& value)
{
    for (auto* sw : m_switches) {
        Debug.print("CHECK: "); Debug.println(sw->m_cmd_topic);
        if (sw->m_cmd_topic == topic) {
            if (value.equalsIgnoreCase(ON)) {
                sw->SetState(true);
            }
            else if (value.equalsIgnoreCase(OFF)) {
                sw->SetState(false);
            }
            else {
                Debug.println("Invalid payload received for switch");
            }

            break;
        }
    }
}


// Topic specialization for sensor components
void HAComponent<Component::Sensor>::getConfigInfo(JsonObject& json)
{
    // https://www.home-assistant.io/components/sensor.mqtt/

    // Sensor component topic
    const char* units = "";
    const char* device_class = nullptr;

    // Update sensor state even if value hasn't changed.
    // This ensures Graphite/Grafana get regularly spaced samples!
    json["frc_upd"] = true; // "force_update"

    switch (m_sensor_class) {
        case SensorClass::Temperature:
            device_class = SensorClassString<SensorClass::Temperature>::name;
            units = "°C";
            break;
        case SensorClass::Humidity:
            device_class = SensorClassString<SensorClass::Humidity>::name;
            units = "%";
            break;
        case SensorClass::Battery:
            device_class = SensorClassString<SensorClass::Battery>::name;
            units = "%";
            break;
        case SensorClass::Illuminance:
            device_class = SensorClassString<SensorClass::Illuminance>::name;
            units = "lx";
            break;
        case SensorClass::Pressure:
            device_class = SensorClassString<SensorClass::Pressure>::name;
            units = "mbar"; // "hPa"
            break;
        case SensorClass::Dust:
            device_class = nullptr;
            units = "ug/m³";
            break;
        case SensorClass::PPM:
            device_class = nullptr;
            units = "ppm";
            break;
        case SensorClass::PPB:
            device_class = nullptr;
            units = "ppb";
            break;
        default:
            break;
    }

    json["unit_of_meas"] = units; // "unit_of_measurement"
    if (device_class != nullptr) {
        json["dev_cla"] = device_class;
    }
}

// Generic publish implementation for sending sensor readings
template<Component c>
void HACompBase<c>::PublishState(const char* value, bool retain)
{
    //Led::SetBuiltin(true);

    Debug.print("publish: ");
    Debug.print(m_state_topic);
    Debug.print(" ");
    Debug.print("=");
    Debug.println(value);

    context.client.publish(m_state_topic.c_str(), value, retain);

    //Led::SetBuiltin(false);
}

template<Component c>
void HACompBase<c>::ClearState()
{
    // Un-publish the state topic
    // IMPORTANT: Use 4-arg overload. The 2 & 3-arg overloads try to call strlen() on payload
    context.client.publish(m_state_topic.c_str(), nullptr, 0, true);
}

// Sensor reading publish implementation
void HAComponent<Component::Sensor>::Update(float value)
{
    // Ensure value is not NaN or Infinity
    if (!std::isfinite(value)) {
        return;
    }

    // Accumulate value
    m_samples++;
    m_sum += value;

    long ts = millis();
    if (ts - m_last_ts > m_sample_interval) {
        m_last_ts = ts;

        // Compute mean over the sample window
        float avg_value = (m_samples > 0) ? (m_sum / (float)m_samples) : m_sum;
        m_samples = 0;
        m_sum = 0.f;

        Debug.print(m_name); Debug.print(": "); Debug.println(avg_value);

        // Only publish if the value is significant
        if (std::isfinite(avg_value) && ((m_hysteresis == 0.0f) || (avg_value <= m_last_value - m_hysteresis || avg_value >= m_last_value + m_hysteresis))) {
            m_last_value = avg_value;

            String value_s(avg_value);
            HACompBase<Component::Sensor>::PublishState(value_s.c_str());
        }
    }
}

float HAComponent<Component::Sensor>::GetCurrent()
{
    return m_last_value;
}


// Topic specialization for sensor components
void HAComponent<Component::BinarySensor>::getConfigInfo(JsonObject& json)
{
    // https://www.home-assistant.io/components/binary_sensor.mqtt/

    // Sensor component topic
    const char* device_class = nullptr;

    switch (m_sensor_class) {
        // TODO...
        default:
            Debug.println("WARNING: Device class unsupported!");
    }

    if (device_class != nullptr) {
        json["dev_cla"] = device_class;
    }
}

void HAComponent<Component::BinarySensor>::ReportState(bool state)
{
    PublishState(state ? "on" : "off");
}

HAAvailabilityComponent::HAAvailabilityComponent(ComponentContext& context)
    : HACompBase(context, "status")
{
    HAAvailabilityComponent::inst = this;
}

void HAAvailabilityComponent::Initialize()
{
    char state_topic[TOPIC_BUFFER_SIZE];
    snprintf(state_topic, sizeof(state_topic), 
        "%s/%s\0", 
        context.device_name, m_name);
    m_state_topic = String(state_topic);
}

void HAAvailabilityComponent::getConfigInfo(JsonObject& json)
{
    // TODO: values should be "online" and "offline"
    json["payload_on"]  = ONLINE;
    json["payload_off"] = OFFLINE;

    json["dev_cla"] = "connectivity";
}

String HAAvailabilityComponent::getWillTopic()
{
    return m_state_topic;
}

void HAAvailabilityComponent::Connect()
{
    PublishState(ONLINE);
}

// Explicit template instantiations. Required to make the CPP linker happy
template class HACompBase<Component::Sensor>;
template class HAComponent<Component::Sensor>;
template class HACompBase<Component::Switch>;
template class HAComponent<Component::Switch>;
template class HACompBase<Component::BinarySensor>;
template class HAComponent<Component::BinarySensor>;