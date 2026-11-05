# Wi-Fi setup UI — firmware 0.2.1

Redesigned for phone and desktop, using system fonts and inline CSS/JS. The
device serves the complete page without Internet dependencies.

1. Save the raw public key using **Sao chép public key** or **Tải file**.
2. Reconnect to a saved network or enter a new 2.4 GHz network. The form keeps
   password visibility controls and a collapsible QR-interval setting.

While connecting, show progress and disable duplicate submissions. Failed
attempts preserve the form and allow retry. Successful saves explain that the
device restarts and the phone should reconnect to its usual network. Saved
reconnect uses the password already on the board, never sent to the browser.

## Clipboard and download

At the real HTTP origin `http://192.168.4.1`, the secure-context Clipboard API
may be unavailable. Use it where supported, otherwise select the readonly key
and try the browser's legacy copy command in response to the user's click.
If rejected, keep the key selected and display manual copy instructions rather
than claiming success. The file button downloads a local text Blob of the
displayed public key; `/public-key` also serves a text attachment for no-JS use.
No private key is exposed and no Wi-Fi credentials enter local storage.

## Source and firmware build

The source of truth is `web/setup/index.html`. PlatformIO runs
`tools/embed_setup_page.py` before building and generates `SetupPageAsset.h`
under `.pio/build/<env>/generated/`, embedding the asset in program flash.
`src/main.cpp` inserts public key, device ID, interval, endpoint and saved SSIDs.
SSIDs are HTML-escaped and inserted last to prevent HTML injection or recursive
template substitution. Build/nạp through the normal `s3tim` environment.

## Checks

```powershell
python -m pip install -r requirements-ui.txt
python -m playwright install chromium
python tools/test_setup_ui.py
```

Headless Chromium tests the exact HTML asset at a simulated insecure
`http://192.168.4.1` origin and secure localhost. Checks cover 320/390/1120px
layout, HTTP copy and denied-copy fallback, real Clipboard API copy, downloaded
file content, password visibility, escaped saved SSIDs, required fields,
failed-save retry, successful saved reconnect and empty-list state.
Wi-Fi responses are mocked; test credentials never reach hardware.

Preview images in `docs/previews/setup-*.png` contain fixture data and are not
screenshots of private network settings. Physical iPhone/Safari clipboard
behavior remains a user validation step; manual copy fallback is available.

Firmware 0.2.1 build and COM5 upload hashes passed. The board was then restarted
with `SETUP_WIFI`, preserving the key and networks, to expose the redesigned
setup page. Giao thức QR, server URLs and Worker behavior are unchanged.
