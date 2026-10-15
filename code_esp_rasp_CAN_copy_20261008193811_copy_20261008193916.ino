#include <Arduino.h>
#include "driver/twai.h"

// ============================================================
// CAN PINS - ESP32-C3 SUPER MINI -> SN65HVD230
// ============================================================
//
// GPIO 4 -> SN65HVD230 TXD
// GPIO 5 -> SN65HVD230 RXD
//

#define CAN_TX_PIN 4
#define CAN_RX_PIN 5


// ============================================================
// SPARK MAX CAN IDs
// ============================================================
//
// IMPORTANT:
// Set these IDs using the REV Hardware Client.
//
// Motor 1 SPARK MAX -> ID 1
// Motor 2 SPARK MAX -> ID 2
//

#define MOTOR_1_ID 1
#define MOTOR_2_ID 2


// ============================================================
// MOTOR SPEEDS
// ============================================================
//
// Range:
// -100.0 = full reverse
//    0.0 = stopped
// +100.0 = full forward
//

float motor1Percent = 0.0;
float motor2Percent = 0.0;


// ============================================================
// CAN SPEED
// ============================================================
//
// SPARK MAX CAN bus = 1 Mbps
//

#define CAN_SPEED TWAI_TIMING_CONFIG_1MBITS()


// ============================================================
// SEND CAN FRAME
// ============================================================

bool sendCAN(
    uint32_t canID,
    const uint8_t *data,
    uint8_t length
)
{
    twai_message_t message = {};

    // CAN ID
    message.identifier = canID;

    // SPARK MAX uses extended 29-bit CAN
    message.flags = TWAI_MSG_FLAG_EXTD;

    // Number of data bytes
    message.data_length_code = length;

    // Copy data into CAN message
    for (uint8_t i = 0; i < length; i++)
    {
        message.data[i] = data[i];
    }

    // Send CAN message
    esp_err_t result =
        twai_transmit(
            &message,
            pdMS_TO_TICKS(100)
        );

    return result == ESP_OK;
}


// ============================================================
// SPARK MAX HEARTBEAT
// ============================================================

void sendHeartbeat()
{
    uint8_t data[8] =
    {
        0x02,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00
    };

    sendCAN(
        0x02052C80,
        data,
        8
    );
}


// ============================================================
// SET SPECIFIC MOTOR SPEED
// ============================================================
//
// sparkID:
// CAN ID of the SPARK MAX
//
// percent:
// -100 to +100
//

void setMotorPercent(
    uint8_t sparkID,
    float percent
)
{
    // --------------------------------------------------------
    // Limit motor command
    // --------------------------------------------------------

    if (percent > 100.0)
    {
        percent = 100.0;
    }

    if (percent < -100.0)
    {
        percent = -100.0;
    }


    // --------------------------------------------------------
    // Convert percentage to -1.0 ... +1.0
    // --------------------------------------------------------

    float output =
        percent / 100.0;


    // --------------------------------------------------------
    // Build SPARK MAX CAN ID
    // --------------------------------------------------------
    //
    // Base motor command:
    // 0x02050080
    //
    // Device ID is stored in lower 6 bits.
    //

    uint32_t canID =
        0x02050080 |
        (sparkID & 0x3F);


    // --------------------------------------------------------
    // Build CAN data packet
    // --------------------------------------------------------

    uint8_t data[8];


    // Bytes 0-3 = motor output float
    memcpy(
        &data[0],
        &output,
        sizeof(float)
    );


    // Duty-cycle mode
    data[4] = 0x00;


    // Required trailer from your original command
    data[5] = 0x00;
    data[6] = 0x80;
    data[7] = 0xFD;


    // --------------------------------------------------------
    // Send motor command
    // --------------------------------------------------------

    sendCAN(
        canID,
        data,
        8
    );
}


// ============================================================
// CAN INITIALIZATION
// ============================================================

bool setupCAN()
{
    // General CAN configuration
    twai_general_config_t generalConfig =
        TWAI_GENERAL_CONFIG_DEFAULT(
            (gpio_num_t)CAN_TX_PIN,
            (gpio_num_t)CAN_RX_PIN,
            TWAI_MODE_NORMAL
        );


    // 1 Mbps timing
    twai_timing_config_t timingConfig =
        CAN_SPEED;


    // Receive all CAN messages
    twai_filter_config_t filterConfig =
        TWAI_FILTER_CONFIG_ACCEPT_ALL();


    // --------------------------------------------------------
    // Install TWAI driver
    // --------------------------------------------------------

    esp_err_t result =
        twai_driver_install(
            &generalConfig,
            &timingConfig,
            &filterConfig
        );


    if (result != ESP_OK)
    {
        Serial.println("TWAI driver installation failed");

        return false;
    }


    // --------------------------------------------------------
    // Start CAN
    // --------------------------------------------------------

    result =
        twai_start();


    if (result != ESP_OK)
    {
        Serial.println("TWAI failed to start");

        return false;
    }


    Serial.println("CAN started successfully");

    return true;
}


// ============================================================
// READ COMMAND FROM SERIAL MONITOR
// ============================================================
//
// Type:
//
// 1 25
//
// Motor 1 -> 25%
//
// Type:
//
// 2 -40
//
// Motor 2 -> 40% reverse
//
// Type:
//
// 1 0
//
// Motor 1 -> stop
//
// ============================================================

void readSerialCommands()
{
    if (Serial.available())
    {
        // Read motor number
        int motorNumber =
            Serial.parseInt();


        // Read requested speed
        float requestedPercent =
            Serial.parseFloat();


        // Clear remaining serial characters
        while (Serial.available())
        {
            Serial.read();
        }


        // ----------------------------------------------------
        // Motor 1
        // ----------------------------------------------------

        if (motorNumber == 1)
        {
            motor1Percent =
                requestedPercent;

            Serial.print("Motor 1 = ");
            Serial.print(motor1Percent);
            Serial.println("%");
        }


        // ----------------------------------------------------
        // Motor 2
        // ----------------------------------------------------

        else if (motorNumber == 2)
        {
            motor2Percent =
                requestedPercent;

            Serial.print("Motor 2 = ");
            Serial.print(motor2Percent);
            Serial.println("%");
        }


        // ----------------------------------------------------
        // Invalid motor number
        // ----------------------------------------------------

        else
        {
            Serial.println(
                "Invalid motor. Use motor 1 or motor 2."
            );
        }
    }
}


// ============================================================
// SETUP
// ============================================================

void setup()
{
    // USB Serial Monitor
    Serial.begin(115200);

    delay(1000);


    Serial.println();
    Serial.println(
        "ESP32-C3 SPARK MAX CAN Controller"
    );


    // --------------------------------------------------------
    // Start CAN
    // --------------------------------------------------------

    if (!setupCAN())
    {
        Serial.println(
            "CAN initialization FAILED"
        );

        while (true)
        {
            delay(1000);
        }
    }


    // --------------------------------------------------------
    // Start motors stopped
    // --------------------------------------------------------

    motor1Percent = 0.0;
    motor2Percent = 0.0;


    setMotorPercent(
        MOTOR_1_ID,
        0.0
    );


    setMotorPercent(
        MOTOR_2_ID,
        0.0
    );


    Serial.println(
        "Both motors stopped"
    );


    Serial.println();
    Serial.println(
        "Enter motor number and speed:"
    );

    Serial.println(
        "Example: 1 20"
    );

    Serial.println(
        "Example: 2 -30"
    );
}


// ============================================================
// MAIN LOOP
// ============================================================

void loop()
{
    // --------------------------------------------------------
    // Read computer commands
    // --------------------------------------------------------

    readSerialCommands();


    // --------------------------------------------------------
    // Send SPARK MAX heartbeat every 20 ms
    // --------------------------------------------------------

    static unsigned long lastHeartbeat = 0;

    unsigned long now =
        millis();


    if (
        now - lastHeartbeat >= 20
    )
    {
        sendHeartbeat();

        lastHeartbeat = now;
    }


    // --------------------------------------------------------
    // Send Motor 1 command
    // --------------------------------------------------------

    setMotorPercent(
        MOTOR_1_ID,
        motor1Percent
    );


    // --------------------------------------------------------
    // Send Motor 2 command
    // --------------------------------------------------------

    setMotorPercent(
        MOTOR_2_ID,
        motor2Percent
    );


    // Send commands about every 10 ms
    delay(10);
}