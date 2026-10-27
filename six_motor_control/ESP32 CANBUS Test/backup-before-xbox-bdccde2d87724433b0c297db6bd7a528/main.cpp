#include <Arduino.h>
#include <math.h>
#include "driver/twai.h"

constexpr gpio_num_t CAN_TX_PIN = GPIO_NUM_4;
constexpr gpio_num_t CAN_RX_PIN = GPIO_NUM_5;
constexpr uint8_t SPARK_ID = 1;

float motorPercent = 0.0f;
uint32_t txQueueErrors = 0;

bool sendCAN(uint32_t id, const uint8_t *data, uint8_t length) {
    if (length > 8) return false;

    twai_message_t message = {};
    message.identifier = id;
    message.flags = TWAI_MSG_FLAG_EXTD;
    message.data_length_code = length;
    memcpy(message.data, data, length);

    // Queue without blocking command input.
    if (twai_transmit(&message, 0) != ESP_OK) {
        ++txQueueErrors;
        return false;
    }

    // Queued does not mean the motor controller acted on it.
    return true;
}

void sendHeartbeat() {
    // Original sketch's heartbeat; firmware compatibility unverified.
    const uint8_t data[8] = {0x02, 0, 0, 0, 0, 0, 0, 0};

    sendCAN(0x02052C80, data, sizeof(data));
}

void sendMotorCommand() {
    float output = motorPercent / 100.0f;

    // Original sketch's CAN command and trailer.
    uint8_t data[8] = {0, 0, 0, 0, 0, 0, 0x80, 0xFD};
    memcpy(data, &output, sizeof(output));

    sendCAN(0x02050080 | SPARK_ID, data, sizeof(data));
}

bool setupCAN() {
    twai_general_config_t general = TWAI_GENERAL_CONFIG_DEFAULT(
        CAN_TX_PIN,
        CAN_RX_PIN,
        TWAI_MODE_NORMAL
    );

    twai_timing_config_t timing = TWAI_TIMING_CONFIG_1MBITS();
    twai_filter_config_t filter = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    esp_err_t result = twai_driver_install(
        &general, &timing, &filter
    );

    if (result == ESP_OK) {
        result = twai_start();
    }

    if (result != ESP_OK) {
        Serial.printf(
            "CAN initialization failed: %s\n",
            esp_err_to_name(result)
        );
        return false;
    }

    return true;
}

void readSerialCommands() {
    static char line[64];
    static size_t used = 0;
    static bool overflow = false;

    while (Serial.available()) {
        char c = Serial.read();

        if (c == '\n' || c == '\r') {
            if (used || overflow) {
                line[used] = '\0';

                int motorNumber = 0;
                float requested = 0;
                char extra = 0;

                bool valid =
                    !overflow &&
                    sscanf(
                        line, "%d %f %c",
                        &motorNumber, &requested, &extra
                    ) == 2 &&
                    motorNumber == 1 &&
                    isfinite(requested) &&
                    requested >= -100 &&
                    requested <= 100;

                if (valid) {
                    motorPercent = requested;

                    Serial.printf(
                        "Requested output: %.1f%%\n",
                        motorPercent
                    );
                } else {
                    Serial.println(
                        "Use: 1 <percent>, from -100 to 100."
                    );
                }
            }

            used = 0;
            overflow = false;
        } else if (used < sizeof(line) - 1) {
            line[used++] = c;
        } else {
            overflow = true;
        }
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    if (!setupCAN()) {
        while (true) {
            delay(1000);
        }
    }

    Serial.println("One SPARK MAX: CAN ID 1");
    Serial.println("Initial requested output: 0%");
    Serial.println("1 10  = request 10% forward");
    Serial.println("1 -10 = request 10% reverse");
    Serial.println("1 0   = request zero output");
    Serial.println("Last command persists until changed or reset.");
}

void loop() {
    readSerialCommands();

    uint32_t now = millis();

    static uint32_t lastHeartbeat = 0;
    static uint32_t lastCommand = 0;
    static uint32_t lastReport = 0;

    if (now - lastHeartbeat >= 20) {
        lastHeartbeat = now;
        sendHeartbeat();
    }

    if (now - lastCommand >= 10) {
        lastCommand = now;
        sendMotorCommand();
    }

    // Drain incoming status frames.
    twai_message_t received;
    for (
        int i = 0;
        i < 16 && twai_receive(&received, 0) == ESP_OK;
        ++i
    ) {}

    if (now - lastReport >= 1000) {
        lastReport = now;

        if (txQueueErrors) {
            Serial.printf(
                "CAN transmit queue failures: %lu\n",
                (unsigned long)txQueueErrors
            );
            txQueueErrors = 0;
        }
    }

    delay(1);
}