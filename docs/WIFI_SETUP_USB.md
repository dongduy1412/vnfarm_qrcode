# Change Wi-Fi using USB

Firmware 0.1.6 accepts the ASCII command `SETUP_WIFI\n` over native USB CDC
(COM5 on the current PC, 115200 baud). This is a local USB maintenance command.
It marks setup incomplete and restarts into the setup portal, preserving the
signing key and previous settings in NVS. Setup remains available after a power
cycle until new settings are saved successfully.

1. Connect to `VNFARM-b17e60`, password `12345678`.
2. Open `http://192.168.4.1/` and enter the new 2.4 GHz Wi-Fi credentials.
3. Keep the event URL `https://vnfam.999.vn/#/` and interval 30 seconds.
4. Save. The device checks the network before saving and restarting.

Verified on the connected device: upload hashes passed, command caused a normal
restart, setup portal startup was logged, and the public key fingerprint stayed
`3b11e24bc51a553f9376dd74ed3e7cc87f99b23a4085ff6074a40717dfd53fdf`.
Joining the home network still requires the user to enter its credentials.

## Remember multiple networks — firmware 0.1.10

- Store up to five successfully configured networks in NVS `qr-networks/list`.
  The list is a single versioned JSON snapshot. Passwords are never returned
  in the setup page or printed to USB. As before, NVS is not flash-encrypted.
- The last saved network is tried first on normal boot. An unsuccessful attempt
  advances to the next saved network after 15 seconds and repeats cyclically.
  This is sequential connection, not a strongest-signal scan. Hidden SSIDs are
  also attempted. On losing connection, retry the current profile first.
- An existing SSID updates its password and moves to the front; it does not
  consume another slot. Saving a sixth distinct SSID drops the oldest entry.
- Firmware before 0.1.10 retained only the last network. That one is migrated
  automatically, including when USB setup is pending. Networks overwritten
  by previous firmware cannot be recovered and must be entered once again.
- In setup, select a remembered network and press the reconnect button to use
  its stored password, or enter another network to add/update it. The device
  tests connectivity before storing credentials. Selecting a saved network
  preserves the existing event URL and interval. The new-network form is also
  prefilled with those settings.
- `SETUP_WIFI` deliberately keeps setup active across reboots until a network
  is selected/saved successfully. It preserves the remembered list and key.
  After saving, subsequent normal restarts automatically try remembered Wi-Fi.
- When none of the saved networks is reachable, normal operation continues
  retrying them. Use USB `SETUP_WIFI` to add a new network; the setup AP does not
  automatically open on loss of connectivity.

Validation: native profile tests cover retention, password changes without
duplicates, eviction, exact SSID strings, input limits and restoration order.
Existing 23 Python tests pass; firmware build/upload hashes passed. Two boots
of the physical board showed `Saved Wi-Fi profiles: 1` and the unchanged key
fingerprint, confirming legacy migration and persistence. Actual radio failover
between two different saved networks still needs a two-network hardware test.

Run the profile test from the project directory:

```powershell
python -m ziglang c++ -O2 -Iinclude tools/test_wifi_profiles.cpp -o .pio/test-wifi-profiles.exe
.pio\test-wifi-profiles.exe
```
