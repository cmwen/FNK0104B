import importlib.util
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location('capture', Path(__file__).resolve().parents[2] / 'scripts/capture_monitor_screen.py')
capture = importlib.util.module_from_spec(spec)
spec.loader.exec_module(capture)

class MonitorCaptureTest(unittest.TestCase):
    def rows(self):
        return ['monitor_screenshot begin width=320 height=240 format=rgb888'] + [
            f'monitor_screenshot row={y} data=' + 'fc8000' * 320 for y in range(240)]

    def test_decodes_complete_capture_amid_status_logs(self):
        lines = self.rows()
        lines.insert(12, 'monitor_speech state=wake afe_frames=10')
        pixels = capture.decode_rows(lines + ['monitor_screenshot end'])
        self.assertEqual(bytes([255, 130, 0]) * 320 * 240, pixels)

    def test_rejects_incomplete_capture(self):
        with self.assertRaises(ValueError):
            capture.decode_rows(self.rows()[:-1] + ['monitor_screenshot end'])

    def test_rejects_duplicate_row(self):
        lines = self.rows()
        with self.assertRaises(ValueError):
            capture.decode_rows(lines + [lines[1], 'monitor_screenshot end'])
