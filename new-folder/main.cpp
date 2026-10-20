#include <Arduino.h>
#include "driver/twai.h"

constexpr uint8_t MOTOR_ID = 2;
constexpr float TEST_DUTY = 0.10f;  // 10%
constexpr uint32_t RUN_MS = 2000;

bool running = false;
bool faulted = false;
uint32_t startedAt = 0;
uint32_t lastSend = 0;

void stopOnFault() {
    running = false;
    faulted = true;

    // Stop transmission so queued commands cannot resume later.
    // Heartbeats also stop; the controller must time out.
    twai_stop();

    Serial.println(
        "CAN fault: test cancelled. Check wiring/power, then reset ESP."
    );
}

bool sendFrame(uint32_t id, const uint8_t *data) {
    twai_message_t frame = {};
    frame.identifier = id;
    frame.extd = 1;
    frame.data_length_code = 8;
    memcpy(frame.data, data, 8);

    esp_err_t result = twai_transmit(
        &frame, pdMS_TO_TICKS(10)
    );

    if (result != ESP_OK) {
        Serial.printf("CAN send error: %s\n",
                      esp_err_to_name(result));
        stopOnFault();
        return false;
    }

    return true;
}

bool sendMotor(float duty) {
    // Same command format as your existing motor test.
    uint8_t data[8] = {0, 0, 0, 0, 0, 0, 0x80, 0xFD};
    memcpy(data, &duty, sizeof(duty));

    return sendFrame(0x02050080UL | MOTOR_ID, data);
}

bool sendHeartbeat() {
    // Broadcast enable: connect only the intended test SPARK.
    const uint8_t data[8] = {
        255, 255, 255, 255, 255, 255, 255, 255
    };
    return sendFrame(0x02052C80UL, data);
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    twai_general_config_t general =
        TWAI_GENERAL_CONFIG_DEFAULT(
            GPIO_NUM_4, GPIO_NUM_5, TWAI_MODE_NORMAL
        );

    twai_timing_config_t timing =
        TWAI_TIMING_CONFIG_1MBITS();

    twai_filter_config_t filter =
        TWAI_FILTER_CONFIG_ACCEPT_ALL();

    esp_err_t result =
        twai_driver_install(&general, &timing, &filter);

    if (result == ESP_OK) {
        result = twai_start();
    }

    if (result != ESP_OK) {
        faulted = true;
        Serial.printf("CAN setup failed: %s\n",
                      esp_err_to_name(result));
        return;
    }

    Serial.println("Single motor: SPARK CAN ID 1");
    Serial.println("g = 10% for 2 seconds; s = stop");
}

void loop() {
    if (faulted) {
        delay(10);
        return;
    }

    twai_status_info_t status = {};
    if (twai_get_status_info(&status) != ESP_OK ||
        status.state != TWAI_STATE_RUNNING ||
        status.tx_error_counter >= 128 ||
        status.rx_error_counter >= 128) {
        stopOnFault();
        return;
    }

    // Drain incoming messages.
    twai_message_t received;
    for (int i = 0;
         i < 32 && twai_receive(&received, 0) == ESP_OK;
         ++i) {
    }

    // Bounded, nonblocking serial input.
    for (int i = 0; i < 64 && Serial.available(); ++i) {
        char c = Serial.read();

        if (c == 'g' && !running) {
            running = true;
            startedAt = millis();
            Serial.println("Starting 10% test.");
        } else if (c == 's') {
            running = false;
            Serial.println("Commanding stop.");
        }
    }

    uint32_t now = millis();

    if (running && now - startedAt >= RUN_MS) {
        running = false;
        Serial.println("Two seconds finished; commanding stop.");
    }

    if (now - lastSend >= 20) {
        lastSend = now;

        if (!sendMotor(running ? TEST_DUTY : 0.0f)) {
            return;
        }

        if (running) {
            if (!sendHeartbeat()) {
                return;
            }
        }
    }

    delay(1);
}