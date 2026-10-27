# Xbox controller to SPARK MAX

Xbox controller -- Bluetooth --> Windows laptop -- USB --> ESP32-C3 -- SN65HVD230 CAN --> six SPARK MAX controllers (IDs 1 through 6), each driving its own NEO or NEO 550.

The ESP keeps the motor-command format from your existing sketch and sends the requested output to IDs 1 through 6. GPIO4 is CAN TX and GPIO5 is CAN RX. The heartbeat enables all controllers on the bus; connect only the intended test controllers. The Xbox serial protocol is unchanged: channel `1 <percent>` controls all six motors, not just CAN ID 1. Firmware must be uploaded for the new motor count to take effect.

## Set up six motors

1. Configure each SPARK separately in REV Hardware Client with a unique ID: 1, 2, 3, 4, 5, or 6. Select REV NEO Brushless / Hall Effect for the NEO and NEO 550. Set a Smart Current Limit appropriate for each motor (for a NEO 550, start with 20A), and persist the settings. The ESP firmware does not assign IDs or configure motor current limits. Test each motor in REV with CAN disconnected before testing the group.
2. Stop the REV motor test and Xbox script. Disconnect power and USB before rewiring. Each NEO needs its own SPARK, matching motor phase colors and its own six-pin sensor cable in that SPARK's ENCODER port.
3. Chain CAN H/yellow and L/green from the transceiver through SPARKs 1, 2, 3, 4, 5, and 6. Move the far-end 120-ohm resistor to the last SPARK (ID 6 in this layout). Keep the other 120-ohm resistor at the transceiver (an onboard resistor counts); no terminators at the intermediate SPARKs. About 60 ohms should measure across H/L with all power disconnected. Physical order does not assign IDs.
4. Feed each SPARK's power input from the same 12V source in parallel, with appropriate branch fusing and wiring. All input negatives, ESP ground, and CAN-board ground must be common. The supply must support the combined starting current of six motors; a supply that ran one or two motors is not automatically sufficient. Motor output black is a phase wire, not supply ground.
5. Stop the REV test, disconnect SPARK USB, restore CAN and power, then reset the ESP. Upload this firmware before starting the Xbox script. It has not been tested with six motors on hardware.

All six motors receive the same duty-cycle request; this does not synchronize measured speed, especially between NEO and NEO 550 motors. Set a corresponding entry in `MOTOR_INVERTED` to `true` to reverse that motor's command if mounting requires it. The six entries correspond to IDs 1 through 6. Test direction while unloaded before connecting a shared mechanism.

Commands are staggered one motor every 2 ms (approximately a 12 ms refresh per motor under normal load), rather than sending six motor frames in one burst. Heartbeats are sent every 20 ms. Every outgoing motor frame uses the latest request, including zero after the 500 ms serial timeout. CAN delivery and motor response still require verification on hardware.

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
- Output is limited to 20% in the Python script and ESP firmware, with a 15% stick dead zone. All six motors follow the left stick. This is a duty-cycle command, not measured RPM.
- Disconnecting the controller commands zero and exits. Reconnect and restart to use it again.
- Ctrl+C sends zero and closes the connection. If messages stop or the laptop script fails, the ESP commands zero after 500 ms.
- The script refuses nonzero commands unless the ESP reports that the new timeout firmware is installed.

Zero output can coast depending on your SPARK MAX idle mode; it is not an instantaneous mechanical stop. The timeout is not protection against every possible ESP, CAN bus, or controller failure.

## Validation

ESP32-C3 PlatformIO compilation passed. Controller logic tests cover center-to-enable, output bounds/dead zone, RT threshold/release, B override, disconnection, and ignoring RB. Run `python -m unittest -v test_controller.py` to repeat them. Bluetooth/motor operation must be checked on hardware after pairing and uploading.

References: [Microsoft XInput gamepad fields](https://learn.microsoft.com/en-us/windows/win32/api/xinput/ns-xinput-xinput_gamepad), [Microsoft Bluetooth pairing guide](https://www.microsoft.com/en-us/windows/learning-center/how-to-connect-xbox-controller-to-pc).
