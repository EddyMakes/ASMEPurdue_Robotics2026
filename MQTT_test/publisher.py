import time
import paho.mqtt.client as mqtt

BROKER_ADDRESS = "localhost"
BROKER_PORT = 1883
TOPIC_NAME = "test/number"

mqtt_client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
mqtt_client.connect(BROKER_ADDRESS, BROKER_PORT)
mqtt_client.loop_start()

message_counter = 0

try:
    while True:
        message = message_counter
        mqtt_client.publish(TOPIC_NAME, str(message))
        print(f"Published {message} to {TOPIC_NAME}")
        message_counter += 1
        time.sleep(1)

except KeyboardInterrupt:
    print("stopping publisher")
finally:
    mqtt_client.loop_stop()
    mqtt_client.disconnect()
