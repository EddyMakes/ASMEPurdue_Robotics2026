import rclpy
from rclpy.node import Node

from std_msgs.msg import String
import serial

class FakeSerial:
    def write(self, data):
        print(f'UART TEST - would send: {data.decode("utf-8").strip()}')

    def close(self):
        pass

    @property
    def is_open(self):
        return True


class PiSpeedSubscriber(Node):

    def __init__(self):
        super().__init__('speed_subscriber')
        self.serial_port = FakeSerial()
        """self.serial_port = serial.Serial(
                port = '/dev/ttyUSB0',
                baudrate = 115200,
                timeout = 1
                )"""

        self.subscription = self.create_subscription(
                String,
                'motor/vel_cmd',
                self.listener_callback, #function that's called when message arrives
                10)
        self.subscription  # prevent unused variable warning
        self.get_logger().info('speed subscriber is running')

    def listener_callback(self, msg):
        received_text = msg.data.strip()
        self.get_logger().info(f'I heard {received_text}')

        try:
            #expected format: 'value1,value2'
            value1_text, value2_text = received_text.split(',')
            value1 = float(value1_text)
            value2 = float(value2_text)
        except ValueError:
            self.get_logger().error(
                    f'invalid message: "{received_text}".'
                    'Expected format: "value1,value2"'
                    )
            return

    def destroy_node(self):
        if self.serial_port.is_open:
            self.serial_port.close()
        super().destroy_node()


def main(args=None):
    rclpy.init(args=args)

    speed_subscriber = PiSpeedSubscriber()
    try:
        rclpy.spin(speed_subscriber)
    except KeyboardInterrupt:
        pass
    finally:
        Node.destroy_node()
        rclpy.shutdown()
    # Destrorosdep install -i --from-path src --rosdistro jazzy -yy the node explicitly
    # (optional - otherwise it will be done automatically
    # when the garbage collector destroys the node object)
    speed_subscriber.destroy_node()
    rclpy.shutdown()


if __name__ == '__main__':
    main()
