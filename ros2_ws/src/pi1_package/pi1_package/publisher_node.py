import rclpy
from rclpy.node import Node

from std_msgs.msg import String


class MinimalPublisher(Node):

    def __init__(self):
        super().__init__('speed_publisher')
        self.publisher_ = self.create_publisher(String, 'motor/vel_cmd', 10)
        timer_period = 0.5  # seconds
        self.timer = self.create_timer(timer_period, self.timer_callback)
        self.left = 0
        self.right = 0

    def timer_callback(self):
        msg = String()
        msg.data = f'{self.left},{self.right}'
        self.publisher_.publish(msg)
        self.get_logger().info('Publishing: "%s"' % msg.data)
        self.left += 1
        self.right += 1


def main(args=None):
    rclpy.init(args=args)

    speed_publisher = MinimalPublisher()

    rclpy.spin(speed_publisher)

    # Destroy the node explicitly
    # (optional - otherwise it will be done automatically
    # when the garbage collector destroys the node object)
    speed_publisher.destroy_node()
    rclpy.shutdown()


if __name__ == '__main__':
    main()
