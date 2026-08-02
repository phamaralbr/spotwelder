#include <Adafruit_SSD1306.h>
#include <EEPROM.h>

//==================== Hardware ==================//
#define pin_ButtonUp    8
#define pin_ButtonDown  7

#define pin_Trigger     5
#define pin_Triac       9

#define min_Pulse_ms    3
#define max_Pulse_ms    800 //120

//==================== Button Settings ==================//
// Delay before auto-repeat starts
const unsigned long BUTTON_REPEAT_DELAY = 500;

// Time held before speed changes
const unsigned long STAGE2_TIME = 1500;
const unsigned long STAGE3_TIME = 3000;

// Repeat interval (ms)
const unsigned long STAGE1_REPEAT = 80;
const unsigned long STAGE2_REPEAT = 20;
const unsigned long STAGE3_REPEAT = 20;

const unsigned long DISPLAY_REFRESH_RATE = 20;

//Only accept a trigger if the input has been stable for some time
const unsigned long TRIGGER_DEBOUNCE_MS = 20;
// Time before allowing another spot weld
const unsigned long WELD_LOCKOUT_MS = 300;

// Step size
const byte STAGE1_STEP = 1;
const byte STAGE2_STEP = 1;
const byte STAGE3_STEP = 5;

//==================== EEPROM ==================//
const byte EEPROM_ADDR = 0;

//==================== Objects ====================//
Adafruit_SSD1306 Display(128, 64, &Wire, -1, 400000, 400000);

//==================== Global Variables ==================//
// byte aux2 = 0;

// int16_t valorEncoder;
uint16_t lastSavedTime;

uint16_t pulseTime;

//=========================================================
void setup()
{
  Serial.begin(9600);

  pinMode(pin_Triac, OUTPUT);
  digitalWrite(pin_Triac, LOW);

  pinMode(pin_Trigger, INPUT_PULLUP);
  pinMode(pin_ButtonUp, INPUT_PULLUP);
  pinMode(pin_ButtonDown, INPUT_PULLUP);

  // Load saved energy from EEPROM
  pulseTime = EEPROM.read(EEPROM_ADDR);

  if (pulseTime < min_Pulse_ms || pulseTime > max_Pulse_ms)
  {
    pulseTime = min_Pulse_ms;
  }

  lastSavedTime = pulseTime;

  Display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  Display.setTextColor(WHITE);
  screenOne();
}

//=========================================================
void loop()
{
  trigger();
  readButtons();

  static uint32_t lastDisplay = 0;

    if (millis() - lastDisplay >= DISPLAY_REFRESH_RATE)
    {
        screenOne();
        lastDisplay = millis();
    }
}

//=========================================================
void readButtons()
{
  static unsigned long upPressedTime = 0;
  static unsigned long downPressedTime = 0;

  static unsigned long lastUpRepeat = 0;
  static unsigned long lastDownRepeat = 0;

  bool up = !digitalRead(pin_ButtonUp);
  bool down = !digitalRead(pin_ButtonDown);

  // ==================== UP ====================
  if (up)
  {
    if (upPressedTime == 0)
    {
      upPressedTime = millis();
      lastUpRepeat = millis();

      if (pulseTime < max_Pulse_ms)
        pulseTime += STAGE1_STEP;
    }
    else
    {
      unsigned long held = millis() - upPressedTime;

      unsigned long repeat;
      byte step;

      if (held >= STAGE3_TIME)
      {
          repeat = STAGE3_REPEAT;
          step = STAGE3_STEP;
      }
      else if (held >= STAGE2_TIME)
      {
          repeat = STAGE2_REPEAT;
          step = STAGE2_STEP;
      }
      else
      {
          repeat = STAGE1_REPEAT;
          step = STAGE1_STEP;
      }

      if (held >= BUTTON_REPEAT_DELAY &&
          millis() - lastUpRepeat >= repeat)
      {
          lastUpRepeat = millis();

          pulseTime += step;

          if (pulseTime > max_Pulse_ms)
              pulseTime = max_Pulse_ms;
      }
    }
  }
  else
  {
    upPressedTime = 0;
  }

  // ==================== DOWN ====================
  if (down)
  {
    if (downPressedTime == 0)
    {
      downPressedTime = millis();
      lastDownRepeat = millis();

      if (pulseTime > min_Pulse_ms)
        pulseTime -= STAGE1_STEP;
    }
    else
    {
      unsigned long held = millis() - downPressedTime;

      unsigned long repeat;
      byte step;

      if (held >= STAGE3_TIME)
      {
          repeat = STAGE3_REPEAT;
          step = STAGE3_STEP;
      }
      else if (held >= STAGE2_TIME)
      {
          repeat = STAGE2_REPEAT;
          step = STAGE2_STEP;
      }
      else
      {
          repeat = STAGE1_REPEAT;
          step = STAGE1_STEP;
      }

      if (held >= BUTTON_REPEAT_DELAY &&
          millis() - lastDownRepeat >= repeat)
      {
          lastDownRepeat = millis();

          pulseTime -= step;

          if (pulseTime < min_Pulse_ms)
              pulseTime = min_Pulse_ms;
      }
    }
  }
  else
  {
    downPressedTime = 0;
  }

  // pulseTime = map(valorEncoder, 1, 100, min_Pulse_ms, max_Pulse_ms);
}

//=========================================================
void trigger()
{
    static bool lastReading = HIGH;
    static bool stableState = HIGH;

    static unsigned long lastChangeTime = 0;
    static unsigned long lastWeldTime = 0;

    static bool armed = true;

    bool reading = digitalRead(pin_Trigger);

    // Detect any change
    if (reading != lastReading)
    {
        lastReading = reading;
        lastChangeTime = millis();
    }

    // Wait until input is stable
    if (millis() - lastChangeTime < TRIGGER_DEBOUNCE_MS)
        return;

    // State changed after debounce
    if (stableState != reading)
    {
        stableState = reading;

        // Trigger released -> re-arm
        if (stableState == HIGH)
        {
            armed = true;
            return;
        }

        // Trigger pressed
        if (armed &&
            millis() - lastWeldTime >= WELD_LOCKOUT_MS)
        {
            armed = false;
            lastWeldTime = millis();

            digitalWrite(pin_Triac, HIGH);
            delay(pulseTime);
            digitalWrite(pin_Triac, LOW);

            if (pulseTime != lastSavedTime)
            {
                EEPROM.update(EEPROM_ADDR, pulseTime);
                lastSavedTime = pulseTime;
            }
        }
    }
}

//=========================================================
void screenOne()
{
  static int lastValue = -1;

  if (lastValue == pulseTime)
      return;

  lastValue = pulseTime;

  Serial.print("Pulse: ");
  Serial.print(pulseTime);
  Serial.println(" ms");

  Display.clearDisplay();

  Display.setTextSize(2);
  Display.setCursor(22, 0);
  Display.print("Pulse:");

  Display.setTextSize(4);

  if (pulseTime < 10)
  {
    Display.setCursor(38, 25);
  }
  else
  {
    Display.setCursor(20, 25);
  }

  Display.print(pulseTime);

  Display.setTextSize(2);
  Display.print(" ms");

  Display.display();
}
