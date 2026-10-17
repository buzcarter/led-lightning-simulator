# Copies every line the board prints to the live dashboard (dashboard/), as
# one UDP datagram per line, and hands the text on to the terminal untouched.
#
# UDP rather than a connection so the monitor never waits on, or even notices,
# the dashboard: with nothing listening the datagrams are simply dropped.
#
# Enabled in platformio.ini with   monitor_filters = default, dashboard
# Target overridable with          STORM_DASH_HOST / STORM_DASH_UDP

import os
import socket

from platformio.public import DeviceMonitorFilterBase


class Dashboard(DeviceMonitorFilterBase):
    NAME = "dashboard"

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self._sock = None
        self._buf = ""
        self._addr = (
            os.environ.get("STORM_DASH_HOST", "127.0.0.1"),
            int(os.environ.get("STORM_DASH_UDP", "7777")),
        )

    def __call__(self):
        # only when the monitor actually starts, not whenever filters are listed
        self._sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        print("--- Copying lines to the dashboard at udp://%s:%d" % self._addr)
        return self

    def rx(self, text):
        # serial hands over arbitrary chunks; the dashboard wants whole lines
        self._buf += text
        *lines, self._buf = self._buf.split("\n")
        for line in lines:
            try:
                self._sock.sendto(line.rstrip("\r").encode("utf-8", "replace"), self._addr)
            except OSError:
                pass  # nobody listening, or the network is down: not our problem
        if len(self._buf) > 4096:
            self._buf = ""  # a line that long is noise, not a status line
        return text
