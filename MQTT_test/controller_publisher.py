import pygame
import time
import paho.mqtt.client as mqtt

BROKER_ADDRESS = "localhost"
BROKER_PORT = 1883
TOPIC_NAME = "test/number"

LEFT_STICK = 1
RIGHT_STICK = 3

mqtt_client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
mqtt_client.connect(BROKER_ADDRESS, BROKER_PORT)
mqtt_client.loop_start()

pygame.init()
controller = pygame.joystick.Joystick(0)

try:
    while True:
        pygame.event.pump()
        left_value = -1 * controller.get_axis(LEFT_STICK)
        right_value = -1 * controller.get_axis(RIGHT_STICK)
        mqtt_client.publish(TOPIC_NAME, f"{left_value:.3f},{right_value:.3f}")
        time.sleep(0.02)
except KeyboardInterrupt:
    pass
finally:
    mqtt_client.loop_stop()
    mqtt_client.disconnect()
    pygame.quit()
