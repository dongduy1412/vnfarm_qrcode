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
