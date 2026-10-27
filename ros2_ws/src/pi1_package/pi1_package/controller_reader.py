import rclpy
from rclpy.node import Node
from std_msgs.msg import String

import time
import pygame

#pygame setup
pygame.init()
pygame.joystick.init()

if pygame.joystick.get_count() == 0:
    print("no controller detected.")
    pygame.quit()
    exit()

controller = pygame.joystick.Joystick(0)
controller.init()

print(f"Connected to: {controller.get_name()}")



#ROS Publisher Class
class SpeedPublisher(Node):

    def __init__(self):
        super().__init__('joysticks_publisher')
        self.publisher_ = self.create_publisher(String, '/motor_cmds/vel', 10)
        timer_period = 0.1
        self.timer = self.create_timer(timer_period, self.publish_speed)
        self.left = 0.0
        self.right = 0.0
    
    def publish_speed(self):
        msg = String()
        msg.data = f'{self.left},{self.right}'
        self.publisher_.publish(msg)
        self.get_logger().info('Publishing "%s"' % msg.data)



def main(args=none):
    rclpy.init(args=args)

    joysticks_publisher = SpeedPublisher()

    left_axis = 0.0
    right_axis = 0.0
    running = True

    try:
        while running and rclpy.ok():
            #quit commands
            for event in pygame.events.get():
                if event.type == pygame.QUIT():
                    running = False
                elif event.type == pygame.KEYDOWN and event.key == pygame.K_ESCAPE:
                    running = False
            
            #read joystick y-axes
            left_axis = controller.get_axis(1)
            right_axis = controller.get_axis(3)

            #update values used by publisher
            joysticks_publisher.left = left_axis
            joysticks_publisher.right = right_axis

            rclpy.spin_once(joysticks_publihser, timeout_sec=0)

            time.sleep(0.01)

    except KeyboardInterrupt:
        pass
    finally:
        joysticks_publisher.destroy_node()
        rclpy.shutdown()
        pygame.quit()



if __name__ == 'main':
    main()
