import email.utils
import sys
import unittest
from pathlib import Path
from unittest.mock import patch, MagicMock
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'web'))
from test_clock import CloudClock, ClockUnavailable

class LocalClockTests(unittest.TestCase):
    def test_verified_reference_uses_monotonic_elapsed_time(self):
        response=MagicMock()
        response.status=200
        response.headers={'Date':'Thu, 05 Nov 2026 06:30:48 GMT'}
        current=[100.0]
        with patch('test_clock.time.monotonic',side_effect=lambda:current[0]),patch('test_clock.urllib.request.urlopen') as opened:
            opened.return_value.__enter__.return_value=response
            clock=CloudClock('https://example.test/health')
            current[0]=105
            epoch=email.utils.parsedate_to_datetime(response.headers['Date']).timestamp()
            self.assertEqual(clock(),epoch+5)
            current[0]=401
            opened.side_effect=OSError('offline')
            self.assertEqual(clock(),epoch+301)
            current[0]=701
            with self.assertRaises(ClockUnavailable):clock()
    def test_http_reference_rejected(self):
        with self.assertRaises(ValueError):CloudClock('http://example.test/health')
