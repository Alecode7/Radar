import unittest

from app import COMMON_COMMANDS


class CommonCommandTests(unittest.TestCase):
    def test_one_click_test_uses_continuous_reporting(self) -> None:
        commands = dict(COMMON_COMMANDS)

        self.assertEqual(
            commands["一键开始测试"],
            "mmwc stop\nmmwc report points 0\nmmwc start",
        )
        self.assertEqual(commands["配置连续上报"], "mmwc report points 0")
        self.assertNotIn("mmwc report points 100", commands.values())


if __name__ == "__main__":
    unittest.main()
