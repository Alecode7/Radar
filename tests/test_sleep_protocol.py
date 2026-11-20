import unittest

from host_algorithm.sleep_protocol import parse_sleep_line


class SleepProtocolTests(unittest.TestCase):
    def test_parses_core_sleep_fields(self) -> None:
        status = parse_sleep_line(
            "SLEEP P=1 A=0 BED=1 BV=1 BR=18 SS=3 MI=1 TE=0 TC=2 STC=1 "
            "BT=1500 OT=0 BVR=91 RVR=84 Bbin=12 Bph=120 Bpd=-8"
        )

        self.assertIsNotNone(status)
        assert status is not None
        self.assertTrue(status.has_person)
        self.assertTrue(status.in_bed)
        self.assertEqual(status.breath_rate, 18)
        self.assertEqual(status.sleep_state, 3)
        self.assertEqual(status.sleep_turn_count, 1)
        self.assertEqual(status.breath_target_bin, 12)

    def test_rejects_incomplete_line(self) -> None:
        self.assertIsNone(parse_sleep_line("SLEEP P=1 A=0"))


if __name__ == "__main__":
    unittest.main()
