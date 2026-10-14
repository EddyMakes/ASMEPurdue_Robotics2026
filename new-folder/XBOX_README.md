# Xbox controller to SPARK MAX

Xbox controller -- Bluetooth --> Windows laptop -- USB --> ESP32-C3 -- SN65HVD230 CAN --> one SPARK MAX (ID 1).

The ESP keeps the CAN command bytes from your working sketch. GPIO4 is CAN TX and GPIO5 is CAN RX. No motor wiring changes are required.

## Start

1. Pair the controller in Windows Settings > Bluetooth & devices > Add device > Bluetooth. Hold its Pair button until the Xbox light flashes, then select Xbox Wireless Controller.
2. Upload the updated `src/main.cpp` using PlatformIO. This adds a 500 ms command timeout: a manually typed command now expires unless repeated. The Python script repeats commands at approximately 50 Hz.
3. Close Arduino/PlatformIO Serial Monitor so Python can open the ESP port.
4. In a laptop terminal in this project folder, run `python xbox_motor.py --preview` to check the controller without opening any serial port. Exit with Ctrl+C.
5. With the motor secured and power disconnect accessible, run `python xbox_motor.py --port COM5`. Use `python xbox_motor.py --list` to check the port if it changes.

Python needs pySerial. It is already installed in this laptop's PlatformIO Python environment. If your terminal uses a different Python without it, run `python -m pip install pyserial`, or use `& "$env:USERPROFILE\.platformio\penv\Scripts\python.exe" .\xbox_motor.py --port COM5` in PowerShell.

## Controls

- Hold RT (right trigger) at least a quarter down with the left stick centered to enable movement.
- Continue holding RT and push the left stick up/down for positive/negative output. RT enables movement; stick position sets output.
- Release RT, center the stick, or press B to command zero output.
- Output is limited to 20%, with a 15% stick dead zone. This is a duty-cycle command, not measured RPM.
- Disconnecting the controller commands zero and exits. Reconnect and restart to use it again.
- Ctrl+C sends zero and closes the connection. If messages stop or the laptop script fails, the ESP commands zero after 500 ms.
- The script refuses nonzero commands unless the ESP reports that the new timeout firmware is installed.

Zero output can coast depending on your SPARK MAX idle mode; it is not an instantaneous mechanical stop. The timeout is not protection against every possible ESP, CAN bus, or controller failure.

## Validation

ESP32-C3 PlatformIO compilation passed. Controller logic tests cover center-to-enable, output bounds/dead zone, RT threshold/release, B override, disconnection, and ignoring RB. Run `python -m unittest -v test_controller.py` to repeat them. Bluetooth/motor operation must be checked on hardware after pairing and uploading.

References: [Microsoft XInput gamepad fields](https://learn.microsoft.com/en-us/windows/win32/api/xinput/ns-xinput-xinput_gamepad), [Microsoft Bluetooth pairing guide](https://www.microsoft.com/en-us/windows/learning-center/how-to-connect-xbox-controller-to-pc).
