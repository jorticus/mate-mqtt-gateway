#include <mate-collector.h>

void DcCollector::process(uint32_t now)
{ 
    // Rollover-safe timestamp check
    if ((now - tPrevStatus) >= statusIntervalMs) {
        Debug.println("Collect DC Status");

        uint8_t status[STATUS_RESP_SIZE * 6];
        uint8_t* curr_status = status;
        for (int i = 0x0A; i <= 0x0F; i++) {
            if (!dev.read_status(curr_status, STATUS_RESP_SIZE, i)) {
                Debug.println("ERROR: Cannot read complete DC status");
                return;
            }

            curr_status += STATUS_RESP_SIZE;
        }

        publishStatus(now, status, sizeof(status));

        tPrevStatus = now;
    }
}

void DcCollector::publishStatus(uint32_t now, uint8_t* status, size_t size)
{
    // mate/dc-1/stat/raw
    // mate/dc-1/stat/ts
    publishTopic("stat/raw", status, size, false);

    // TODO: Real timestamp
    char ts_str[20];
    snprintf(ts_str, sizeof(ts_str), "%u", now);
    publishTopic("stat/ts", ts_str, false);
}
