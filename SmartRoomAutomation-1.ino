// ============================================================
//  Smart Room Automation System — Minimal Edition
//  ESP32 | DHT11 | PIR | LDR Module | SSD1306 OLED | Relay x2
// ============================================================

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>

// ── Pins ─────────────────────────────────────────────────────
#define DHT_PIN         4
#define DHT_TYPE        DHT11
#define PIR_PIN         27
#define LDR_PIN         34        // LDR module D0 pin (digital)
#define RELAY_FAN       23
#define RELAY_LIGHT     19
#define BTN_UP          25
#define BTN_DOWN        33

// ── OLED ─────────────────────────────────────────────────────
#define SCREEN_WIDTH    128
#define SCREEN_HEIGHT   64
#define OLED_RESET      -1
#define OLED_ADDRESS    0x3C

// ── Settings ─────────────────────────────────────────────────
#define TEMP_DEFAULT    28
#define TEMP_MIN        18
#define TEMP_MAX        40
#define DEBOUNCE_MS     200
#define PIR_HOLDOFF     10000   // ms to keep motion flag after last trigger
#define SENSOR_INTERVAL 2000

// ── Objects ──────────────────────────────────────────────────
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
DHT dht(DHT_PIN, DHT_TYPE);

// ── State ────────────────────────────────────────────────────
float   temperature   = 0.0;
float   humidity      = 0.0;
bool    dhtOK         = false;
bool    isDark        = false;
bool    motion        = false;
bool    fanON         = false;
bool    lightON       = false;
int     setTemp       = TEMP_DEFAULT;

unsigned long lastMotionTime  = 0;
unsigned long lastSensor      = 0;
unsigned long lastDisplay     = 0;
unsigned long lastBtnUp       = 0;
unsigned long lastBtnDown     = 0;

// ════════════════════════════════════════════════════════════
void setup() {
  Serial.begin(115200);

  pinMode(RELAY_FAN,   OUTPUT); digitalWrite(RELAY_FAN,   HIGH); // OFF
  pinMode(RELAY_LIGHT, OUTPUT); digitalWrite(RELAY_LIGHT, HIGH); // OFF
  pinMode(PIR_PIN,     INPUT);
  pinMode(LDR_PIN,     INPUT);
  pinMode(BTN_UP,      INPUT_PULLUP);
  pinMode(BTN_DOWN,    INPUT_PULLUP);

  dht.begin();
  Wire.begin();

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    Serial.println(F("OLED not found!"));
    while (true);
  }

  showBoot();
}

// ════════════════════════════════════════════════════════════
void loop() {
  unsigned long now = millis();
  handleButtons(now);
  readSensors(now);
  applyLogic(now);
  updateOLED(now);
}

// ── Buttons ──────────────────────────────────────────────────
void handleButtons(unsigned long now) {
  if (digitalRead(BTN_UP) == LOW && now - lastBtnUp > DEBOUNCE_MS) {
    lastBtnUp = now;
    if (setTemp < TEMP_MAX) setTemp++;
  }
  if (digitalRead(BTN_DOWN) == LOW && now - lastBtnDown > DEBOUNCE_MS) {
    lastBtnDown = now;
    if (setTemp > TEMP_MIN) setTemp--;
  }
}

// ── Sensors ──────────────────────────────────────────────────
void readSensors(unsigned long now) {
  // PIR
  if (digitalRead(PIR_PIN)) lastMotionTime = now;
  motion = (now - lastMotionTime < PIR_HOLDOFF);

  // DHT + LDR on interval
  if (now - lastSensor >= SENSOR_INTERVAL) {
    lastSensor = now;
    float t = dht.readTemperature();
    float h = dht.readHumidity();
    if (!isnan(t) && !isnan(h)) { temperature = t; humidity = h; dhtOK = true; }
    else dhtOK = false;

    // LDR module: INVERTED -> HIGH = dark, LOW = bright
    isDark = (digitalRead(LDR_PIN) == HIGH);
  }
}

// ── Logic ────────────────────────────────────────────────────
void applyLogic(unsigned long now) {
  if (!motion) {
    // No motion → everything OFF
    lightON = false;
    fanON   = false;
  } else {
    // Motion detected
    lightON = isDark;                              // light only if dark
    fanON   = dhtOK && (temperature > setTemp);    // fan only if hot
  }

  digitalWrite(RELAY_LIGHT, lightON ? LOW : HIGH);
  digitalWrite(RELAY_FAN,   fanON   ? LOW : HIGH);
}

// ── OLED ─────────────────────────────────────────────────────
void updateOLED(unsigned long now) {
  if (now - lastDisplay < 500) return;
  lastDisplay = now;

  display.clearDisplay();
  drawUI();
  display.display();
}

// ── UI Layout ─────────────────────────────────────────────────
//
//  ┌────────────────────────────┐
//  │ SMART ROOM                 │  ← header (inverted)
//  ├───────────┬────────────────┤
//  │ TMP 28.4° │ MOT  YES / NO  │
//  │ HUM  63%  │ LDR  DRK / BRT │
//  │ SET  28°  │                │
//  ├───────────┴────────────────┤
//  │  FAN  [ ON ]  LGT  [ ON ] │  ← relay bar (inverted if ON)
//  └────────────────────────────┘
//
void drawUI() {
  // ── Header ──────────────────────────────────────────────
  display.fillRect(0, 0, 128, 12, SSD1306_WHITE);
  display.setTextColor(SSD1306_BLACK);
  display.setTextSize(1);
  display.setCursor(28, 2);
  display.print(F("SMART  ROOM"));

  display.setTextColor(SSD1306_WHITE);

  // ── Divider vertical ────────────────────────────────────
  display.drawFastVLine(63, 13, 37, SSD1306_WHITE);

  // ── LEFT COLUMN: Temp / Hum / SetTemp ───────────────────
  // Temp
  display.setCursor(0, 14);
  display.print(F("TMP"));
  display.setCursor(24, 14);
  if (dhtOK) { display.print(temperature, 1); display.print(F("C")); }
  else          display.print(F("--.-C"));

  // Humidity
  display.setCursor(0, 26);
  display.print(F("HUM"));
  display.setCursor(24, 26);
  if (dhtOK) { display.print((int)humidity); display.print(F("%")); }
  else          display.print(F("--%"));

  // Set temp
  display.setCursor(0, 38);
  display.print(F("SET"));
  display.setCursor(24, 38);
  display.print(setTemp);
  display.print(F("C"));

  // ── RIGHT COLUMN: Motion / LDR ──────────────────────────
  // Motion
  display.setCursor(68, 14);
  display.print(F("MOT"));
  display.setCursor(92, 14);
  display.print(motion ? F("YES") : F("NO "));

  // LDR
  display.setCursor(68, 26);
  display.print(F("LDR"));
  display.setCursor(92, 26);
  display.print(isDark ? F("DRK") : F("BRT"));

  // ── Relay Status Bar ────────────────────────────────────
  display.drawFastHLine(0, 51, 128, SSD1306_WHITE);

  // FAN block
  if (fanON) {
    display.fillRect(0, 53, 62, 11, SSD1306_WHITE);
    display.setTextColor(SSD1306_BLACK);
  } else {
    display.setTextColor(SSD1306_WHITE);
  }
  display.setCursor(8, 55);
  display.print(F("FAN "));
  display.print(fanON ? F("[ON] ") : F("[OFF]"));

  // LIGHT block
  if (lightON) {
    display.fillRect(64, 53, 64, 11, SSD1306_WHITE);
    display.setTextColor(SSD1306_BLACK);
  } else {
    display.setTextColor(SSD1306_WHITE);
  }
  display.setCursor(70, 55);
  display.print(F("LGT "));
  display.print(lightON ? F("[ON] ") : F("[OFF]"));

  display.setTextColor(SSD1306_WHITE); // reset
}

// ── Boot Screen ──────────────────────────────────────────────
void showBoot() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(22, 10);
  display.print(F("SMART  ROOM"));
  display.drawFastHLine(0, 22, 128, SSD1306_WHITE);
  display.setCursor(30, 30);
  display.print(F("Starting..."));
  display.display();
  delay(1500);
}
