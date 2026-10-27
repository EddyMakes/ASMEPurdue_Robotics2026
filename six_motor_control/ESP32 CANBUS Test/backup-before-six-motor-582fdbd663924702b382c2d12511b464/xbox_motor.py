"""Windows Xbox left-stick control for the single-SPARK-MAX ESP firmware."""
import argparse
import ctypes
import math
import time

import serial
from serial.tools import list_ports

RT_THRESHOLD = 64  # XInput trigger range: 0-255; about a quarter press.
B = 0x2000
DEADZONE = 0.15
MAX_PERCENT = 20.0
READY = b"XBOX_READY timeout=500"


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


def send(uart, percent):
    message = f"1 {percent:.2f}\n".encode("ascii")
    if uart.write(message) != len(message):
        raise serial.SerialTimeoutException("Incomplete serial write")


def check_firmware(uart):
    # Only zero commands are sent before the new watchdog firmware is confirmed.
    uart.reset_input_buffer()
    rx = bytearray()
    deadline = time.monotonic() + 5
    while time.monotonic() < deadline:
        send(uart, 0)
        uart.write(b"?\n")
        time.sleep(0.1)
        rx.extend(uart.read(min(uart.in_waiting, 4096)))
        while b"\n" in rx:
            line, _, rest = rx.partition(b"\n")
            rx = bytearray(rest)
            if line.strip() == READY:
                return
        if len(rx) > 4096:
            rx.clear()
    raise RuntimeError("Upload the updated ESP firmware first: watchdog handshake not received.")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", help="ESP COM port; auto-detects one Espressif USB device")
    parser.add_argument("--controller", type=int, choices=range(4), help="XInput controller index")
    parser.add_argument("--list", action="store_true", help="List devices without opening a serial port")
    parser.add_argument("--preview", action="store_true", help="Show controller commands without connecting to ESP")
    args = parser.parse_args()
    xbox = Xbox()
    ports = list(list_ports.comports())
    connected = [i for i in range(4) if xbox.read(i) is not None]
    if args.list:
        print("Xbox controller indices:", connected or "none detected")
        for port in ports:
            print(f"{port.device}: {port.description}")
        return
    if args.controller is None:
        if len(connected) != 1:
            raise RuntimeError(f"Expected one Xbox controller; found {connected}. Pair it or specify --controller INDEX.")
        args.controller = connected[0]
    elif xbox.read(args.controller) is None:
        raise RuntimeError("Selected Xbox controller is not connected.")
    if not args.preview and not args.port:
        candidates = [p.device for p in ports if p.vid == 0x303A and p.pid == 0x1001]
        if len(candidates) != 1:
            raise RuntimeError("Cannot uniquely identify ESP. Run --list, then specify --port COMx.")
        args.port = candidates[0]

    control = Control()
    uart = None
    try:
        if not args.preview:
            uart = serial.Serial(args.port, 115200, timeout=0, write_timeout=0.1)
            check_firmware(uart)
            print(f"Connected to {args.port}; ESP 500 ms command timeout confirmed.")
        else:
            print("PREVIEW: no serial connection and no motor commands sent.")
        print("Hold RT at least a quarter down with left stick centered to enable; then move stick up/down.")
        print("Release RT or press B for zero output. Limit: 20%. Ctrl+C exits.")
        last_report = 0
        last_loop = time.monotonic()
        feedback = bytearray()
        while True:
            now = time.monotonic()
            if now - last_loop > 0.25:
                control.armed = False
            last_loop = now
            pad = xbox.read(args.controller)
            percent, status = control.update(pad)
            if uart:
                send(uart, percent)
                feedback.extend(uart.read(min(uart.in_waiting, 4096)))
                while b"\n" in feedback:
                    line, _, rest = feedback.partition(b"\n")
                    feedback = bytearray(rest)
                    if b"CAN transmit queue failures" in line:
                        raise RuntimeError(line.decode("ascii", errors="replace"))
                if len(feedback) > 4096:
                    feedback.clear()
            if now - last_report >= 0.25:
                print(f"{status}; requested {percent:+.1f}%", flush=True)
                last_report = now
            if pad is None:
                print("Controller lost. Reconnect and restart this program.")
                break
            time.sleep(0.02)
    except KeyboardInterrupt:
        print("\nStopping.")
    finally:
        if uart is not None:
            try:
                send(uart, 0)
            except (serial.SerialException, OSError):
                pass  # ESP watchdog commands zero if the serial link has failed.
            uart.close()


if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, OSError, serial.SerialException) as error:
        print(f"Error: {error}")
        raise SystemExit(1)
