import paho.mqtt.client as mqtt

BROKER_ADDRESS = "localhost"
BROKER_PORT = 1883
TOPIC_NAME = "test/number"

def on_connect(mqtt_client, user_data, connect_flags, reason_code, properties):
    if reason_code == 0:
        print(f"connected to broker, subscribing to {TOPIC_NAME}")
        mqtt_client.subscribe(TOPIC_NAME)
    else:
        print(f"connection failed: {reason_code}")


def on_message(mqtt_client, user_data, incoming_message):
    payload_text = incoming_message.payload.decode()
    try:
        received_number = incoming_message.payload.decode()
    except ValueError:
        print(f"ignoring message on {incoming_message.topic}: {payload_text!r}")
        return
    print(f"received {received_number} on {incoming_message.topic}")


mqtt_client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
mqtt_client.on_connect = on_connect
mqtt_client.on_message = on_message

mqtt_client.connect(BROKER_ADDRESS, BROKER_PORT)

try:
    mqtt_client.loop_forever()
except KeyboardInterrupt:
    print("Stopping subscriber")
finally:
    mqtt_client.disconnect()
