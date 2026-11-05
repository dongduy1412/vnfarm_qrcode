"""Optional verified cloud clock for the local lab when Windows time is wrong.

The deployed Worker always uses Date.now(); this never changes TTL/SKEW policy
or the Windows clock. Sample only a configured trusted HTTPS health endpoint.
"""
import email.utils
import threading
import time
import urllib.request
from urllib.parse import urlsplit

class ClockUnavailable(RuntimeError): pass

class CloudClock:
    def __init__(self, url):
        if urlsplit(url).scheme != 'https': raise ValueError('Clock reference must use HTTPS')
        self.url=url
        self.lock=threading.Lock()
        self.synced=None
        self.epoch=0
        self._sync()

    def _sync(self):
        start=time.monotonic()
        request=urllib.request.Request(self.url,headers={'User-Agent':'Mozilla/5.0 (VNFARM clock test)','Cache-Control':'no-cache'})
        with urllib.request.urlopen(request,timeout=5) as response:
            header=response.headers.get('Date')
            if response.status != 200 or not header: raise ClockUnavailable('Clock reference unavailable')
            epoch=email.utils.parsedate_to_datetime(header).timestamp()
        finished=time.monotonic()
        if finished-start>4: raise ClockUnavailable('Clock reference latency too high')
        self.epoch=epoch
        self.synced=finished

    def __call__(self):
        with self.lock:
            if self.synced is None or time.monotonic()-self.synced>300:
                try: self._sync()
                except (OSError,ValueError,ClockUnavailable):
                    if self.synced is None or time.monotonic()-self.synced>600: raise ClockUnavailable('Clock reference stale')
            return self.epoch+time.monotonic()-self.synced
