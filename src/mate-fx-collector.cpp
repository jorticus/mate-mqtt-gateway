#include <mate-collector.h>

void FxCollector::process(uint32_t now)
{ 
    // Rollover-safe timestamp check
    if ((now - tPrevStatus) >= statusIntervalMs) {
        Debug.println("Collect Status");

        uint8_t status[STATUS_RESP_SIZE];
        if (dev.read_status(status, sizeof(status))) {
            publishStatus(now, status, sizeof(status));
        }

        tPrevStatus = now;
        Debug.print("ts:");
        Debug.println(now);
    }

    // TODO: Retrieve logpage
}

void FxCollector::publishStatus(uint32_t now, uint8_t* status, size_t size)
{
    // mate/fx-1/stat/raw
    // mate/fx-1/stat/ts
    publishTopic("stat/raw", status, size, false);

    // TODO: Real timestamp
    char ts_str[20];
    snprintf(ts_str, sizeof(ts_str), "%u", now);
    publishTopic("stat/ts", ts_str, false);

    // TODO: Get 230/110v flag from status packet
}
