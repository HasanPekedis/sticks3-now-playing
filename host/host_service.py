# Host service for the StickS3 Now Playing firmware (Windows).
# Finds the device as soon as it is plugged in, sends the track playing in Spotify
# (or any app using Windows media controls) and forwards button presses back.
# Runs in the background with pythonw; logs to host_service.log next to this file.
import asyncio, base64, io, json, logging, os, sys, time, unicodedata
import serial
from serial.tools import list_ports
from PIL import Image
from winrt.windows.media.control import GlobalSystemMediaTransportControlsSessionManager as Manager
from winrt.windows.storage.streams import Buffer, InputStreamOptions, DataReader

LOG = os.path.join(os.path.dirname(os.path.abspath(__file__)), "host_service.log")
logging.basicConfig(filename=LOG, level=logging.INFO, format="%(asctime)s %(message)s", encoding="utf-8")
log = logging.info

PLAYING = 4


# The device font (efont JA) has Japanese and most Latin letters but lacks a few Turkish ones.
MISSING_GLYPHS = str.maketrans("ıİğĞ", "iIgG")


def clean_text(s):
    s = unicodedata.normalize("NFC", s or "").translate(MISSING_GLYPHS)
    return "".join(c for c in s if c.isprintable()).strip()


def pick_session(mgr):
    sessions = list(mgr.get_sessions())
    for s in sessions:
        if "spotify" in (s.source_app_user_model_id or "").lower():
            return s
    return mgr.get_current_session() or (sessions[0] if sessions else None)


async def read_thumb(props):
    if props.thumbnail is None:
        return None
    stream = await props.thumbnail.open_read_async()
    buf = Buffer(stream.size)
    await stream.read_async(buf, buf.capacity, InputStreamOptions.READ_AHEAD)
    try:
        return bytes(memoryview(buf))
    except TypeError:
        r = DataReader.from_buffer(buf); out = bytearray(buf.length); r.read_bytes(out); return bytes(out)


def prep_cover(raw):
    im = Image.open(io.BytesIO(raw)).convert("RGB")
    w, h = im.size; s = min(w, h)
    im = im.crop(((w - s) // 2, (h - s) // 2, (w + s) // 2, (h + s) // 2)).resize((135, 135), Image.LANCZOS)
    r, g, b = im.resize((1, 1), Image.BOX).getpixel((0, 0))
    bg = (int(r * 0.35) << 16) | (int(g * 0.35) << 8) | int(b * 0.35)
    out = io.BytesIO(); im.save(out, "JPEG", quality=85)
    return out.getvalue(), bg


def open_port(dev):
    s = serial.Serial(); s.port = dev; s.baudrate = 115200; s.timeout = 0
    s.dtr = False; s.rts = False          # ESP32-S3'u resetlememek icin
    s.open()
    return s


def find_device():
    """VID 303A portlarini dener, 'HELLO NP1' diyeni dondurur."""
    for p in list_ports.comports():
        if p.vid != 0x303A:
            continue
        try:
            s = open_port(p.device)
        except Exception:
            continue
        buf, end = b"", time.time() + 2.5
        while time.time() < end:
            buf += s.read(256)
            if b"HELLO NP1" in buf:
                return s
            time.sleep(0.05)
        s.close()
    return None


async def serve(ser, mgr):
    last_key, bg, rx, last_meta = None, 0x181818, b"", 0.0
    while True:
        s = pick_session(mgr)
        rx += ser.read(ser.in_waiting or 1)
        while b"\n" in rx:
            line, rx = rx.split(b"\n", 1)
            ev = line.decode(errors="ignore").strip()
            if ev == "HELLO NP1":
                last_key = None            # cihaz yeniden basladi: kapagi tekrar gonder
            elif ev.startswith("BTN:") and s:
                cmd = ev[4:]; log("tus: %s", cmd)
                if cmd == "play": await s.try_toggle_play_pause_async()
                elif cmd == "next": await s.try_skip_next_async()
                elif cmd == "prev": await s.try_skip_previous_async()
                last_meta = 0
        if time.time() - last_meta >= 1.0:
            last_meta = time.time()
            if s is None:
                if last_key != "none":
                    ser.write(b"X\n"); last_key = "none"
                m = {"title": "Sessizlik", "artist": "Hicbir sey calmiyor", "playing": False, "pos": 0, "dur": 0,
                     "bg": 0x181818}
            else:
                p = await s.try_get_media_properties_async()
                tl, pb = s.get_timeline_properties(), s.get_playback_info()
                playing = pb.playback_status == PLAYING
                pos = tl.position.total_seconds()
                if playing:
                    pos += max(0.0, time.time() - tl.last_updated_time.timestamp())
                dur = tl.end_time.total_seconds()
                key = (p.title, p.artist)
                if key != last_key:
                    last_key = key; log("calan: %s - %s", p.artist, p.title)
                    try:
                        raw = await read_thumb(p)
                        if raw:
                            jpg, bg = prep_cover(raw)
                            ser.write(b"C%d\n" % len(jpg) + jpg)
                        else:
                            ser.write(b"X\n"); bg = 0x181818
                    except Exception as e:
                        log("kapak alinamadi: %s", e); ser.write(b"X\n"); bg = 0x181818
                m = {"title": clean_text(p.title), "artist": clean_text(p.artist), "playing": playing,
                     "pos": round(min(pos, dur) if dur else pos, 1), "dur": round(dur, 1), "bg": bg}
            ser.write(b"M" + json.dumps(m, ensure_ascii=False).encode("utf-8") + b"\n")
        await asyncio.sleep(0.05)


async def main():
    mgr = await Manager.request_async()
    log("servis basladi")
    while True:
        ser = find_device()
        if not ser:
            await asyncio.sleep(2)
            continue
        log("cihaz bulundu: %s", ser.port)
        try:
            await serve(ser, mgr)
        except Exception as e:
            log("baglanti koptu: %s", e)
        try:
            ser.close()
        except Exception:
            pass
        await asyncio.sleep(1)


if __name__ == "__main__":
    asyncio.run(main())
