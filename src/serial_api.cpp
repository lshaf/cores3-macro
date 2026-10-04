#include "serial_api.h"

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#include "config.h"

namespace {

QueueHandle_t lineQueue = nullptr;

void readerTask(void*) {
    char* buf = static_cast<char*>(malloc(cfg::kMaxSerialLine + 1));
    size_t len = 0;
    uint8_t chunk[256];
    for (;;) {
        const size_t n = Serial.available() > 0 ? Serial.read(chunk, sizeof(chunk)) : 0;
        if (n == 0) {
            vTaskDelay(pdMS_TO_TICKS(2));
            continue;
        }
        for (size_t i = 0; i < n; ++i) {
            const char c = static_cast<char>(chunk[i]);
            if (c == '\n') {
                if (len > 0) {
                    String* line = new String();
                    line->reserve(len);
                    line->concat(buf, len);
                    if (xQueueSend(lineQueue, &line, pdMS_TO_TICKS(200)) != pdPASS) delete line;
                }
                len = 0;
            } else if (c != '\r' && len < cfg::kMaxSerialLine) {
                buf[len++] = c;
            }
        }
    }
}

}

void serial_api::begin() {
    Serial.setRxBufferSize(cfg::kSerialRxBuffer);
    Serial.begin(cfg::kSerialBaud);
    Serial.setTxTimeoutMs(cfg::kSerialTxTimeoutMs);
    lineQueue = xQueueCreate(8, sizeof(String*));
    xTaskCreatePinnedToCore(readerTask, "serial_rx", 6144, nullptr, 2, nullptr, 0);
}

bool serial_api::nextLine(String& out) {
    String* line = nullptr;
    if (lineQueue == nullptr || xQueueReceive(lineQueue, &line, 0) != pdPASS) return false;
    out = *line;
    delete line;
    return true;
}

void serial_api::send(JsonDocument& doc) {
    String out;
    serializeJson(doc, out);
    out += '\n';
    Serial.write(reinterpret_cast<const uint8_t*>(out.c_str()), out.length());
}

bool serial_api::hostConnected() {
    return static_cast<bool>(Serial);
}
