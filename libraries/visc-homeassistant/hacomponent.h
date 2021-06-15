#ifndef __HA_COMPONENT_H__
#define __HA_COMPONENT_H__

#include <Arduino.h>
#include <vector>
//#include "debug.h"
#include <ArduinoJson.h>
#include <PubSubClient.h>

#define TOPIC_BUFFER_SIZE (80)
#define JSON_BUFFER_SIZE (MQTT_MAX_PACKET_SIZE)

class ComponentContext {
public:
    PubSubClient& client;
    const char* device_name;
    const char* friendly_name;
    const char* fw_version;
    const char* model;
    const char* manufacturer;

    ComponentContext(PubSubClient& client)
        : client(client)
    { }
};

enum class Component {
    Undefined,
    Sensor,
    BinarySensor,
    Switch
};

template<Component c>
struct ComponentString {
public:
    static const char* name;
};

// See https://www.home-assistant.io/components/sensor/ for supported device classes
// https://github.com/home-assistant/home-assistant/blob/70ce9bb7bc15b1a66bb1d21598efd0bb8b102522/homeassistant/components/sensor/__init__.py#L26
enum class SensorClass {
    Undefined,
    Battery,
    Humidity,
    Illuminance,
    Temperature,
    Pressure,
// Extra ones only used to add units (device_class == null)
    Dust,
    PPM,
    PPB
};

template<SensorClass c>
struct SensorClassString {
public:
    static const char* name;
};

// https://github.com/home-assistant/home-assistant/blob/70ce9bb7bc15b1a66bb1d21598efd0bb8b102522/homeassistant/components/binary_sensor/__init__.py
enum class BinarySensorClass {
    battery,       // On means low, Off means normal
    cold,          // On means cold, Off means normal
    connectivity,  // On means connected, Off means disconnected
    door,          // On means open, Off means closed
    garage_door,   // On means open, Off means closed
    gas,           // On means gas detected, Off means no gas (clear)
    heat,          // On means hot, Off means normal
    light,         // On means light detected, Off means no light
    lock,          // On means open (unlocked), Off means closed (locked)
    moisture,      // On means wet, Off means dry
    motion,        // On means motion detected, Off means no motion (clear)
    moving,        // On means moving, Off means not moving (stopped)
    occupancy,     // On means occupied, Off means not occupied (clear)
    opening,       // On means open, Off means closed
    plug,          // On means plugged in, Off means unplugged
    power,         // On means power detected, Off means no power
    presence,      // On means home, Off means away
    problem,       // On means problem detected, Off means no problem (OK)
    safety,        // On means unsafe, Off means safe
    smoke,         // On means smoke detected, Off means no smoke (clear)
    sound,         // On means sound detected, Off means no sound (clear)
    vibration,     // On means vibration detected, Off means no vibration
    window,        // On means open, Off means closed
    Undefined
};

// template<BinarySensorClass c>
// struct BinarySensorClassString {
// public:
//     static const char* name;
// }

class HACompItem
{
public:
    static std::vector<HACompItem*> m_components;

    virtual void Initialize() = 0;
    static void InitializeAll() {
        for (auto item : m_components) {
            item->Initialize();
        }
    }
};

// Base class to get around templating quirks. Do not use directly.
template<Component c>
class HACompBase : public HACompItem
{
protected:
    const char*     m_device_class;
    const char*     m_name;
    String          m_state_topic;
    ComponentContext& context;

    static const char* m_component;

    virtual void getConfigInfo(JsonObject& json);
   // virtual String getStatusTopic();

public:
    HACompBase(ComponentContext& context, const char* name)
        : m_name(name), context(context)
    {
        m_components.push_back(this);
    }

    virtual void Initialize();
    void PublishConfig(bool present = true);
    void PublishState(const char* value, bool retain = true);
    void ClearState();
};

// Generic Component
template<Component component>
class HAComponent : public HACompBase<component>
{ 
    using HACompBase<component>::HACompBase; // constructor
};

// Specialization of Component of type Sensor
template<>
class HAComponent<Component::Sensor> : public HACompBase<Component::Sensor>
{
protected:
    SensorClass m_sensor_class;

    // Hysteresis
    float m_hysteresis;
    float m_last_value;

    // Averaging
    float m_sum;
    int m_samples;

    // Sampling
    int m_last_ts;
    int m_sample_interval;

    virtual void getConfigInfo(JsonObject& json);
public:
    HAComponent(ComponentContext& context, const char* name, int sample_interval_ms, float hysteresis = 0.0f, SensorClass sclass = SensorClass::Undefined) :
        HACompBase(context, name),
        m_sensor_class(sclass),
        m_hysteresis(hysteresis),
        m_last_value(0.f),
        m_sum(0.f),
        m_samples(0),
        m_last_ts(0),
        m_sample_interval(sample_interval_ms)
    { }

    void Update(float value);
    float GetCurrent();
};

// Specialization of Component of type Switch
template<>
class HAComponent<Component::Switch> : public HACompBase<Component::Switch>
{
protected:
    bool m_state;
    String m_cmd_topic;
    std::function<void(boolean)> m_callback;

    static std::vector<HAComponent<Component::Switch>*> m_switches;

    virtual void getConfigInfo(JsonObject& json);
public:
    HAComponent(ComponentContext& context, const char* name, std::function<void(boolean)> callback);

    void Initialize() override;
    void SetState(bool state);
    void ReportState();

    static const char* ON;
    static const char* OFF;

    static void ProcessMqttTopic(String& topic, String& payload);
};

// Specialization of Component of type BinarySensor
template<>
class HAComponent<Component::BinarySensor> : public HACompBase<Component::BinarySensor>
{
protected:
    BinarySensorClass m_sensor_class;

    virtual void getConfigInfo(JsonObject& json);
public:
    HAComponent(ComponentContext& context, const char* name, BinarySensorClass sensor_class = BinarySensorClass::Undefined) :
        HACompBase(context, name),
        m_sensor_class(sensor_class)
    { }

    void ReportState(bool state);
};

// Device availability component
class HAAvailabilityComponent : public HACompBase<Component::BinarySensor>
{
protected:
    virtual void getConfigInfo(JsonObject& json);
public:
    HAAvailabilityComponent(ComponentContext& context);

    static const char* ONLINE;
    static const char* OFFLINE;

    String getWillTopic();
    void Initialize() override;
    void Connect();

    static HAAvailabilityComponent* inst;
};

#endif /* __HA_COMPONENT_H__ */
