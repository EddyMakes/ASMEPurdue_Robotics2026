#include <Arduino.h>
#include <math.h>
#include "driver/twai.h"

constexpr gpio_num_t CAN_TX_PIN = GPIO_NUM_4;
constexpr gpio_num_t CAN_RX_PIN = GPIO_NUM_5;
constexpr uint8_t SPARK_IDS[] = {1, 2, 3, 4, 5, 6};
// One direction flag per CAN ID, in the same order as SPARK_IDS.
constexpr bool MOTOR_INVERTED[] = {false, false, false, false, false, false};
constexpr size_t MOTOR_COUNT = sizeof(SPARK_IDS) / sizeof(SPARK_IDS[0]);
static_assert(MOTOR_COUNT == sizeof(MOTOR_INVERTED) / sizeof(MOTOR_INVERTED[0]),
              "Each motor needs a direction flag");
// Stagger frames instead of bursting six commands into the default TX queue.
constexpr uint32_t COMMAND_SPACING_MS = 2;
constexpr float MAX_PERCENT = 20.0f;

float motorPercent = 0.0f;
uint32_t txQueueErrors = 0;
constexpr uint32_t COMMAND_TIMEOUT_MS = 500;
uint32_t lastInputMs = 0;

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
    // Broadcast enable, as used by sparkcan's SparkBase::Heartbeat.
    // Connect only the intended test controllers to this CAN bus.
    const uint8_t data[8] = {
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
    };

    sendCAN(0x02052C80, data, sizeof(data));
}

void sendMotorCommand(uint8_t id, bool inverted) {
    float output = (inverted ? -motorPercent : motorPercent) / 100.0f;

    // Original sketch's CAN command and trailer.
    uint8_t data[8] = {0, 0, 0, 0, 0, 0, 0x80, 0xFD};
    memcpy(data, &output, sizeof(output));

    sendCAN(0x02050080 | id, data, sizeof(data));
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

    // Bound input work so a flood of bytes cannot starve the watchdog.
    for (int count = 0; count < 128 && Serial.available(); ++count) {
        char c = Serial.read();

        if (c == '\n' || c == '\r') {
            if (used || overflow) {
                line[used] = '\0';

                if (!overflow && strcmp(line, "?") == 0) {
                    Serial.println("XBOX_READY timeout=500");
                    used = 0;
                    overflow = false;
                    continue;
                }

                // Serial channel 1 controls the whole group; it is not a CAN ID.
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
                    requested >= -MAX_PERCENT &&
                    requested <= MAX_PERCENT;

                if (valid) {
                    lastInputMs = millis();
                    motorPercent = requested;

                    Serial.printf(
                        "Requested output for all six motors: %.1f%%\n",
                        motorPercent
                    );
                } else {
                    Serial.println(
                        "Use: 1 <percent>, from -20 to 20; controls all six motors."
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

    Serial.println("Six SPARK MAX controllers: CAN IDs 1, 2, 3, 4, 5, 6");
    Serial.println("Initial requested output: 0%");
    Serial.println("1 10  = request 10% on all six motors");
    Serial.println("1 -10 = request -10% on all six motors");
    Serial.println("1 0   = request zero output on all six motors");
    Serial.println("Command timeout: 500 ms. Repeat commands to maintain output.");
}

void loop() {
    readSerialCommands();

    uint32_t now = millis();
    if (now - lastInputMs >= COMMAND_TIMEOUT_MS) {
        motorPercent = 0.0f;
    }

    static uint32_t lastHeartbeat = 0;
    static uint32_t lastCommand = 0;
    static size_t nextMotor = 0;
    static uint32_t lastReport = 0;

    if (now - lastHeartbeat >= 20) {
        lastHeartbeat = now;
        sendHeartbeat();
    }

    if (now - lastCommand >= COMMAND_SPACING_MS) {
        lastCommand = now;
        // Use the latest requested output, including watchdog zero, each time.
        // Every motor is refreshed approximately every 12 ms under normal load.
        sendMotorCommand(SPARK_IDS[nextMotor], MOTOR_INVERTED[nextMotor]);
        nextMotor = (nextMotor + 1) % MOTOR_COUNT;
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
