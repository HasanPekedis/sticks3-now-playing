# StickS3 Now Playing

A tiny "now playing" display for the **M5Stack StickS3**. Plug it into your Windows PC and it shows the track
currently playing in Spotify (or any app that uses Windows media controls): album art, title, artist and a live
progress bar. The buttons control playback.

- **A (front button):** play / pause
- **B (side button):** next track
- **B (hold, then release):** previous track

Titles in Japanese, Turkish and other non-Latin scripts are rendered with the efont Unicode font.

> Not affiliated with or endorsed by Spotify or M5Stack.

## How it works

```
Spotify / any media app
        │  Windows media session (GlobalSystemMediaTransportControls)
        ▼
host/host_service.py ──USB serial──► StickS3 firmware (M5Unified)
        ▲                                  │
        └──────── button events ───────────┘
```

No Spotify account, API key or login is needed: the host reads the same information Windows shows in its media
overlay. The device needs the PC (and the USB cable) to show anything; without it, it shows a spinning record and
"waiting for computer".

## Hardware

- M5Stack StickS3 (ESP32-S3-PICO-1, 135×240 display)
- A USB-C **data** cable. Some charge-only or worn cables cause `Device Descriptor Request Failed` errors on Windows.

## 1. Flash the firmware

Requires [PlatformIO](https://platformio.org/) (`pip install platformio`).

```sh
cd firmware
pio run -t upload
```

If the device runs other firmware and the upload can't connect, put it into download mode first: with USB connected,
hold the side button for about 2 seconds until the screen goes dark, then run the upload again. Once this firmware is
running, later uploads reset the board automatically.

## 2. Run the host (Windows)

Requires Python 3.10+.

```sh
cd host
pip install -r requirements.txt
python host_service.py
```

The service scans USB serial ports for the device (it announces itself with `HELLO NP1`), reconnects when you unplug
and replug it, and writes a log to `host/host_service.log`.

### Start automatically at login (optional)

```powershell
powershell -ExecutionPolicy Bypass -File host\add_startup.ps1
```

This puts a "M5 Simdi Calan" shortcut in your Startup folder that runs the service hidden with `pythonw`.
To undo it, open `shell:startup` and delete the shortcut.

## Serial protocol

115200 baud over the ESP32-S3's built-in USB Serial/JTAG. All messages are newline-terminated.

| Direction | Message | Meaning |
|---|---|---|
| device → host | `HELLO NP1` | Sent every second while no host is talking to the device |
| device → host | `BTN:play` / `BTN:next` / `BTN:prev` | Button events |
| host → device | `M{json}` | Track info: `title`, `artist`, `playing`, `pos`, `dur` (seconds), `bg` (0xRRGGBB background) |
| host → device | `C<len>` followed by `<len>` raw bytes | 135×135 JPEG album art |
| host → device | `X` | Clear album art |

The host opens the port with DTR/RTS low so the ESP32-S3 isn't reset on connect.

## License

[MIT](LICENSE)
