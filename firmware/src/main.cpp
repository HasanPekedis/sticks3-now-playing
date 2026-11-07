// M5StickS3 "Simdi Calan" firmware'i.
// Bilgisayardaki host.py USB uzerinden sarki bilgisini ve kapagi gonderir.
// A (on tus): oynat/duraklat   B (yan tus): sonraki   B basili tut: onceki
#include <M5Unified.h>
#include <ArduinoJson.h>

static const int W = 135, H = 240;
M5Canvas cv(&M5.Display);

// --- durum ---
String title = "", artist = "";
bool playing = false;
float posSec = 0, durSec = 0;
uint32_t metaAt = 0, lastRx = 0, lastHello = 0;
uint16_t bg = 0x18C3;
uint8_t *cover = nullptr;
size_t coverLen = 0;
float titleX = 0;
uint32_t bDown = 0;
String flashBtn = "";
uint32_t flashAt = 0;

// --- seri okuma durum makinesi ---
String line;
uint8_t *rxCover = nullptr;
size_t rxCoverLen = 0, rxCoverGot = 0;
bool inCover = false;

static uint16_t rgb(uint32_t c) { return cv.color565((c >> 16) & 255, (c >> 8) & 255, c & 255); }

void handleLine(const String &l) {
  if (l.length() == 0) return;
  char k = l[0];
  if (k == 'M') {
    JsonDocument d;
    if (deserializeJson(d, l.c_str() + 1)) return;
    String nt = d["title"] | "";
    if (nt != title) titleX = 0;
    title = nt;
    artist = (const char *)(d["artist"] | "");
    playing = d["playing"] | false;
    posSec = d["pos"] | 0.0f;
    durSec = d["dur"] | 0.0f;
    bg = rgb(d["bg"] | 0x181818);
    metaAt = millis();
  } else if (k == 'C') {
    size_t n = l.substring(1).toInt();
    if (n == 0 || n > 120000) return;
    rxCover = (uint8_t *)ps_malloc(n);
    if (!rxCover) rxCover = (uint8_t *)malloc(n);
    if (!rxCover) return;
    rxCoverLen = n; rxCoverGot = 0; inCover = true;
  } else if (k == 'X') {
    free(cover); cover = nullptr; coverLen = 0;
  }
}

void readSerial() {
  while (Serial.available()) {
    if (inCover) {
      size_t want = rxCoverLen - rxCoverGot;
      size_t got = Serial.read(rxCover + rxCoverGot, want);
      rxCoverGot += got;
      lastRx = millis();
      if (rxCoverGot >= rxCoverLen) {
        free(cover);
        cover = rxCover; coverLen = rxCoverLen;
        rxCover = nullptr; inCover = false;
      }
      continue;
    }
    char c = Serial.read();
    lastRx = millis();
    if (c == '\n') { handleLine(line); line = ""; }
    else if (c != '\r' && line.length() < 1024) line += c;
  }
}

void sendBtn(const char *ev) {
  Serial.printf("BTN:%s\n", ev);
  flashBtn = ev; flashAt = millis();
}

String fmt(float s) {
  int t = (int)max(0.0f, s);
  char b[12]; snprintf(b, sizeof b, "%d:%02d", t / 60, t % 60);
  return b;
}

void icon(const char *k, int cx, int cy, uint16_t col) {
  String kind = k;
  if (kind == "play") cv.fillTriangle(cx - 6, cy - 8, cx - 6, cy + 8, cx + 8, cy, col);
  else if (kind == "pause") { cv.fillRect(cx - 7, cy - 8, 5, 16, col); cv.fillRect(cx + 2, cy - 8, 5, 16, col); }
  else {
    int d = kind == "next" ? 1 : -1;
    cv.fillTriangle(cx - d * 8, cy - 7, cx - d * 8, cy + 7, cx, cy, col);
    cv.fillTriangle(cx, cy - 7, cx, cy + 7, cx + d * 8, cy, col);
    cv.fillRect(cx + d * 8 - (d > 0 ? 2 : 0), cy - 7, 2, 14, col);
  }
}

void drawIdle(uint32_t now) {
  cv.fillScreen(TFT_BLACK);
  float t = now / 1000.0f;
  // donen plak
  int cx = W / 2, cy = 95;
  cv.fillCircle(cx, cy, 50, 0x2104);
  for (int r = 20; r < 50; r += 6) cv.drawCircle(cx, cy, r, 0x3186);
  cv.fillCircle(cx, cy, 16, cv.color565(29, 185, 84));
  float a = t * 2.0f;
  cv.fillCircle(cx + cosf(a) * 36, cy + sinf(a) * 36, 3, TFT_WHITE);
  cv.fillCircle(cx, cy, 3, TFT_BLACK);
  cv.setFont(&fonts::DejaVu18);
  cv.setTextColor(TFT_WHITE);
  cv.setTextDatum(top_center);
  cv.drawString("Simdi Calan", W / 2, 162);
  cv.setFont(&fonts::DejaVu12);
  cv.setTextColor(0xB596);
  cv.drawString("bilgisayar", W / 2, 190);
  cv.drawString("bekleniyor" + String("...").substring(0, (now / 400) % 4), W / 2, 206);
}

void drawPlayer(uint32_t now) {
  cv.fillScreen(bg);
  bool ok = false;
  if (cover && coverLen) ok = cv.drawJpg(cover, coverLen, 0, 0, W, W);
  if (!ok) {
    cv.fillRect(0, 0, W, W, 0x2945);
    cv.fillCircle(W / 2, W / 2, 38, 0x1082);
    cv.fillCircle(W / 2, W / 2, 12, cv.color565(29, 185, 84));
  }
  // baslik
  cv.setFont(&fonts::efontJA_16);   // Japonca + Latin + Turkce karakterler
  cv.setTextColor(TFT_WHITE);
  int tw = cv.textWidth(title);
  if (tw > W - 8) {
    titleX -= 1.2f;
    if (titleX < -(tw + 30)) titleX = 0;
    cv.setTextDatum(top_left);
    cv.drawString(title, 4 + (int)titleX, 142);
    cv.drawString(title, 4 + (int)titleX + tw + 30, 142);
  } else {
    cv.setTextDatum(top_center);
    cv.drawString(title, W / 2, 142);
  }
  // sanatci
  cv.setFont(&fonts::efontJA_12);
  cv.setTextColor(0xB596);
  String a = artist;
  while (a.length() && cv.textWidth(a) > W - 8) {   // UTF-8 karakteri bolmeden kirp
    int i = a.length() - 1;
    while (i > 0 && ((uint8_t)a[i] & 0xC0) == 0x80) i--;
    a.remove(i);
  }
  cv.setTextDatum(top_center);
  cv.drawString(a, W / 2, 164);
  // ilerleme
  float p = posSec + (playing ? (now - metaAt) / 1000.0f : 0);
  if (durSec > 0 && p > durSec) p = durSec;
  cv.fillRoundRect(8, 186, W - 16, 4, 2, 0x52AA);
  if (durSec > 0) cv.fillRoundRect(8, 186, max(4, (int)((W - 16) * p / durSec)), 4, 2, TFT_WHITE);
  cv.setFont(&fonts::DejaVu9);
  cv.setTextColor(0xB596);
  cv.setTextDatum(top_left);  cv.drawString(fmt(p), 8, 193);
  cv.setTextDatum(top_right); cv.drawString(fmt(durSec), W - 8, 193);
  // kontroller
  String hi = (flashBtn.length() && now - flashAt < 250) ? flashBtn : "";
  icon("prev", 30, 220, hi == "prev" ? TFT_WHITE : 0xB596);
  cv.fillCircle(W / 2, 220, 15, hi == "play" ? cv.color565(29, 185, 84) : TFT_WHITE);
  icon(playing ? "pause" : "play", W / 2 + (playing ? 0 : 1), 220, TFT_BLACK);
  icon("next", W - 30, 220, hi == "next" ? TFT_WHITE : 0xB596);
}

void setup() {
  Serial.setRxBufferSize(32768);
  Serial.setTxTimeoutMs(0);
  Serial.begin(115200);
  auto cfg = M5.config();
  M5.begin(cfg);
  if (M5.Display.width() > M5.Display.height()) M5.Display.setRotation(M5.Display.getRotation() ^ 1);
  M5.Display.setBrightness(160);
  cv.setColorDepth(16);
  cv.setPsram(false);
  cv.createSprite(W, H);
}

void loop() {
  readSerial();
  M5.update();
  uint32_t now = millis();
  if (M5.BtnA.wasPressed()) sendBtn("play");
  if (M5.BtnB.wasPressed()) bDown = now;
  if (M5.BtnB.wasReleased() && bDown) { sendBtn(now - bDown > 700 ? "prev" : "next"); bDown = 0; }

  bool connected = lastRx && (now - lastRx < 4000);
  if (!connected && now - lastHello > 1000) { Serial.print("HELLO NP1\n"); lastHello = now; }

  static uint32_t lastDraw = 0;
  if (now - lastDraw < 40) { delay(2); return; }
  lastDraw = now;
  if (connected && metaAt) drawPlayer(now); else drawIdle(now);
  cv.pushSprite(0, 0);
}
