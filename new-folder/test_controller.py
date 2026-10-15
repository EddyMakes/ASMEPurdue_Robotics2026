import unittest
from xbox_motor import B, RT_THRESHOLD, Control, Gamepad


def pad(y=0, trigger=0, buttons=0):
    value = Gamepad()
    value.left_y = y
    value.buttons = buttons
    value.right_trigger = trigger
    return value


class ControlTests(unittest.TestCase):
    def test_requires_center_before_motion(self):
        control = Control()
        self.assertEqual(control.update(pad(32767, 255))[0], 0)
        control.update(pad(0, 255))
        self.assertEqual(control.update(pad(32767, 255))[0], 20)
        self.assertEqual(control.update(pad(-32768, 255))[0], -20)

    def test_release_and_reconnect_require_rearm(self):
        control = Control()
        control.update(pad(0, 255))
        self.assertEqual(control.update(pad(32767))[0], 0)
        self.assertEqual(control.update(pad(32767, 255))[0], 0)
        control.update(pad(0, 255))
        self.assertEqual(control.update(None)[0], 0)
        self.assertEqual(control.update(pad(32767, 255))[0], 0)

    def test_stop_button_overrides_trigger(self):
        control = Control()
        control.update(pad(0, 255))
        self.assertEqual(control.update(pad(32767, 255, B))[0], 0)
        self.assertEqual(control.update(pad(32767, 255))[0], 0)

    def test_deadzone_and_full_axis_range(self):
        control = Control()
        control.update(pad(0, 255))
        for y in range(-32768, 32768, 127):
            output = control.update(pad(y, 255))[0]
            self.assertLessEqual(abs(output), 20)
            if abs(y) < 4000:
                self.assertEqual(output, 0)

    def test_trigger_threshold_and_release(self):
        control = Control()
        control.update(pad(0, RT_THRESHOLD - 1))
        self.assertFalse(control.armed)
        control.update(pad(0, RT_THRESHOLD))
        self.assertTrue(control.armed)
        self.assertEqual(control.update(pad(32767, RT_THRESHOLD))[0], 20)
        self.assertEqual(control.update(pad(32767, RT_THRESHOLD - 1))[0], 0)
        self.assertFalse(control.armed)
        self.assertEqual(control.update(pad(32767, 255))[0], 0)

    def test_bumper_alone_does_not_enable(self):
        control = Control()
        control.update(pad(0, buttons=0x0200))
        self.assertFalse(control.armed)
        self.assertEqual(control.update(pad(32767, buttons=0x0200))[0], 0)


if __name__ == "__main__":
    unittest.main()
