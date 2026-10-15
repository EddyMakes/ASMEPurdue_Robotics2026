import rclpy
from rclpy.node import Node
from std_msgs.msg import String

import argparse
import ctypes
import math
import time
import serial
from serial.tools import list_ports


#put ROS code at bottom, controller stuff at top

#define constants
RT_THRESHOLD = 64  # XInput trigger range: 0-255; about a quarter press.
B = 0x2000
DEADZONE = 0.15
MAX_PERCENT = 20.0
READY = b"XBOX_READY timeout=500"
TOPIC = "/spark_max/command_percent"

class Gamepad(ctypes.Structure):
    _fields_ = [
        ("buttons", ctypes.c_uint16),
        ("left_trigger", ctypes.c_uint8),
        ("right_trigger", ctypes.c_uint8),
        ("left_x", ctypes.c_int16), ("left_y", ctypes.c_int16),
        ("right_x", ctypes.c_int16), ("right_y", ctypes.c_int16),
    ]

class State(ctypes.Structure):
    _fields_ = [("packet", ctypes.c_uint32), ("gamepad", Gamepad)]

class Xbox:
    def __init__(self):
        self.dll = ctypes.WinDLL("xinput1_4.dll")
        self.get_state = self.dll.XInputGetState
        self.get_state.argtypes = [ctypes.c_uint32, ctypes.POINTER(State)]
        self.get_state.restype = ctypes.c_uint32

    def read(self, index):
        state = State()
        result = self.get_state(index, ctypes.byref(state))
        if result == 1167:  # ERROR_DEVICE_NOT_CONNECTED
            return None
        if result != 0:
            raise OSError(f"XInputGetState failed: {result}")
        return state.gamepad

class Control:
    def __init__(self):
        self.armed = False

    def update(self, pad):
        if pad is None:
            self.armed = False
            return 0.0, "Disconnected: zero output"
        axis = pad.left_y / (32767.0 if pad.left_y >= 0 else 32768.0)

        if pad.buttons & B:
            self.armed = False
            return 0.0, "B pressed: zero output"
        
        if pad.right_trigger < RT_THRESHOLD:
            self.armed = False
            return 0.0, "Hold RT with stick centered to enable"
        
        if not self.armed:
            if abs(axis) > DEADZONE:
                return 0.0, "Center stick while holding RT to enable"
            self.armed = True

        if abs(axis) <= DEADZONE:
            return 0.0, "Enabled: stick centered"
        
        scaled = (abs(axis) - DEADZONE) / (1.0 - DEADZONE)
        return math.copysign(min(scaled, 1.0) * MAX_PERCENT, axis), "Enabled"



class SparkPublisher(Node):

    def __init__(self):
        super().__init__('xbox_spark_max_controller')
        self.publisher_ = self.create_publisher(String, TOPIC, 10)
        self.left
        self.right
        

    def publish_percent(self, percent):
        msg = String()
        msg.data = f'{self.left},{self.right}'
        self.publisher_.publish(msg)
        self.get_logger().info('Publishing: "%s"' % msg.data)
        self.left += 1
        self.right += 1


def main(args=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--controller", type=int, choices=range(4),
        help="XInput controller index"
    )
    parser.add_argument(
        "--list", action="store_true",
        help="List connected Xbox controllers"
    )
    parser.add_argument(
        "--preview", action="store_true",
        help="Show controller commands without publishing to ROS 2"
    )
    args = parser.parse_args()

    xbox = Xbox()
    connected = [
        i for i in range(4) if xbox.read(i) is not None
    ]

    if args.list:
        print("Xbox controller indices:", connected or "none detected")
        return

    if args.controller is None:
        if len(connected) != 1:
            raise RuntimeError(
                f"Expected one Xbox controller; found {connected}. "
                "Pair it or specify --controller INDEX."
            )
        args.controller = connected[0]
    elif xbox.read(args.controller) is None:
        raise RuntimeError("Selected Xbox controller is not connected.")

    control = Control()
    node = None

    try:
        if not args.preview:
            rclpy.init()
            node = SparkPublisher()
            print(f"Publishing motor percentage to {TOPIC}")
        else:
            print("PREVIEW: no ROS 2 messages will be published.")

        print(
            "Hold RT at least a quarter down with left stick centered "
            "to enable; then move stick up/down."
        )
        print("Release RT or press B for zero output. Limit: 20%. Ctrl+C exits.")

        last_report = 0
        last_loop = time.monotonic()

        while True:
            now = time.monotonic()

            if now - last_loop > 0.25:
                control.armed = False
            last_loop = now

            pad = xbox.read(args.controller)
            percent, status = control.update(pad)

            if node is not None:
                node.publish_percent(percent)
                rclpy.spin_once(node, timeout_sec=0)

            if now - last_report >= 0.25:
                print(
                    f"{status}; requested {percent:+.1f}%",
                    flush=True,
                )
                last_report = now

            if pad is None:
                print("Controller lost. Reconnect and restart this program.")
                break

            time.sleep(0.02)

    except KeyboardInterrupt:
        print("\nStopping.")

    finally:
        if node is not None:
            try:
                node.publish_percent(0.0)
                rclpy.spin_once(node, timeout_sec=0)
            except Exception:
                pass
            node.destroy_node()

        if rclpy.ok():
            rclpy.shutdown()


if __name__ == '__main__':
    main()
