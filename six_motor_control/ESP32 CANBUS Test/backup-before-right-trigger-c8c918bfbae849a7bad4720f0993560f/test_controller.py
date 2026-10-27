import unittest
from xbox_motor import B, RB, Control, Gamepad


def pad(y=0, buttons=0):
    value = Gamepad()
    value.left_y = y
    value.buttons = buttons
    return value


class ControlTests(unittest.TestCase):
    def test_requires_center_before_motion(self):
        control = Control()
        self.assertEqual(control.update(pad(32767, RB))[0], 0)
        control.update(pad(0, RB))
        self.assertEqual(control.update(pad(32767, RB))[0], 20)
        self.assertEqual(control.update(pad(-32768, RB))[0], -20)

    def test_release_and_reconnect_require_rearm(self):
        control = Control()
        control.update(pad(0, RB))
        self.assertEqual(control.update(pad(32767))[0], 0)
        self.assertEqual(control.update(pad(32767, RB))[0], 0)
        control.update(pad(0, RB))
        self.assertEqual(control.update(None)[0], 0)
        self.assertEqual(control.update(pad(32767, RB))[0], 0)

    def test_stop_button_overrides_bumper(self):
        control = Control()
        control.update(pad(0, RB))
        self.assertEqual(control.update(pad(32767, RB | B))[0], 0)
        self.assertEqual(control.update(pad(32767, RB))[0], 0)

    def test_deadzone_and_full_axis_range(self):
        control = Control()
        control.update(pad(0, RB))
        for y in range(-32768, 32768, 127):
            output = control.update(pad(y, RB))[0]
            self.assertLessEqual(abs(output), 20)
            if abs(y) < 4000:
                self.assertEqual(output, 0)


if __name__ == "__main__":
    unittest.main()
