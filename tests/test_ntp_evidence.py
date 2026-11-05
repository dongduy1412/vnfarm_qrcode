import sys
import unittest
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from ntp_evidence import sanitize, validate


class NtpEvidenceTests(unittest.TestCase):
    boot = ['QR firmware 0.3.1 | device fixture']
    sync = 'NTP sync confirmed: epoch=1793866400 uptime_ms=5600 interval_ms=3600000'
    qr = 'QR refreshed: t=1793866380, interval=30 seconds'

    def test_boot_order(self):
        self.assertIn('PASS', validate(self.boot + [self.sync, self.qr]))
        for lines in ([self.sync, self.qr], self.boot + [self.qr, self.sync], self.boot + [self.qr]):
            with self.assertRaises(ValueError):
                validate(lines)

    def test_recovery(self):
        lines = self.boot + ['NTP not synchronized; QR blocked, SNTP continues retrying', self.sync, self.qr]
        self.assertIn('PASS', validate(lines, 'recovery'))
        with self.assertRaises(ValueError):
            validate(self.boot + [self.sync, self.qr], 'recovery')

    def test_periodic_same_boot(self):
        lines = self.boot + [self.sync, self.qr]
        with self.assertRaises(ValueError):
            validate(lines + [self.sync], 'periodic')
        hourly = 'NTP sync confirmed: epoch=1793870000 uptime_ms=3605600 interval_ms=3600000'
        self.assertIn('PASS', validate(lines + [hourly], 'periodic'))
        with self.assertRaises(ValueError):
            validate(lines + self.boot + [self.sync, self.qr, hourly], 'periodic')

    def test_redaction(self):
        self.assertNotIn('secret', sanitize('QR test URL: secret'))
        self.assertNotIn('fixture', sanitize('QR public key: fixture'))
        self.assertEqual(sanitize(self.sync), self.sync)
