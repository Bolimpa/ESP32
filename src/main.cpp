#include <Arduino.h>
#include <TFT_eSPI.h> // Hardware-specific library
#include <SPI.h>
#include "PerlinNoise.h"
#include "OneButton.h"
#include "Interpolator.h"
#include "IBMPlexMonoBold10.h"
#include "IBMPlexMonoMedium12.h"
#include "BLEDevice.h"

#include "BleKeyboard.h"

char str[20];

const float VOLTAGE_DIVIDER_RATIO = 2.0;  // voltage divider ratio
const float VOLTAGE_DIVIDER_OFFSET = 0.0; // voltage divider offset
const float ADC_RESOLUTION = 4095.0;      // ADC resolution
const float VOLTAGE_REFERENCE = 3.3;      // ADC voltage reference
const float min_voltage = 3.0;            // minimum battery voltage
const float max_voltage = 4.2;            // maximum battery voltage

BleKeyboard bleKeyboard;

#define Bold10 IBMPlexMonoBold10
#define Medium12 IBMPlexMonoMedium12

using namespace Interpolator;

// ICONS
//
// ICONS
//
// ICONS
//
//
// Define a macro to generate the enum and the array
#define ICON_LIST(X) \
  X(PRESENTATION)    \
  X(MEDIACONTROL)    \
  X(WIFI)            \
  X(SETTINGS)        \
  X(INFO)            \
  X(BLUETOOTH)       \
  X(BRIGHTNESS)      \
  X(VOLUME)          \
  X(SLEEP)           \
  X(ALARM)           \
  X(TIMER)           \
  X(CALENDAR)        \
  X(WEATHER)         \
  X(MUSIC)           \
  X(RADIO)           \
  X(SOUND)           \
  X(LIGHT)           \
  X(CAMERA)          \
  X(VIDEO)           \
  X(GAMES)           \
  X(APPS)

// Generate the enumeration constants
enum Icon
{
#define X(name) name,
  ICON_LIST(X)
#undef X
};

// Generate the array initialization code
const std::array<std::string, static_cast<int>(Icon::APPS) + 1> icons = {
#define X(name) #name,
    ICON_LIST(X)
#undef X
};

const int iconCount = sizeof(icons) / sizeof(icons[0]);
uint8_t iconTextSize[iconCount];
uint8_t currentIconIndex = 0;

//
//
// ICONS
//
// ICONS
//
// ICONS

struct Particle
{
  float x;
  float y;
  float vx;
  float vy;
  float radius;
};

struct Point
{
  float x;
  float y;
};

unsigned long firstButtonClickTime = 0;
const unsigned long timeThreshold = 100;

bool rightClicked = false;
bool leftClicked = false;

void resetButtonStates();
void updateFirstButtonClickTime();

void rightClick();
void leftClick();

float progress = 0.0;

#define PIN_BTN_L 0
#define PIN_BTN_R 47

PerlinNoise perlin(43);

OneButton btn_left(PIN_BTN_L, true);
OneButton btn_right(PIN_BTN_R, true);

OneButton menu_left(PIN_BTN_L, true);
OneButton menu_right(PIN_BTN_R, true);

void drawIcon(uint8_t icon, int16_t x, int16_t y);
void titleText(String title, int16_t x, int16_t boxWidth);
Point pointOnRotatedEllipse(float a, float b, float t, float r);
void drawGraph(int16_t xOffset, int16_t yOffset);
bool loading();
uint8_t scanNetworks();
uint16_t lerpRGB565(uint32_t c, uint32_t d, float t);

TFT_eSPI tft = TFT_eSPI();              // Invoke library, pins defined in User_Setup.h
TFT_eSprite sprite = TFT_eSprite(&tft); // Sprite object "spr" with pointer to "tft" object

void boxBlurH_4(uint8_t *scl, uint8_t *tcl, int w, int h, int r)
{
  float iarr = 1.0 / (r + r + 1);
  for (int i = 0; i < h; i++)
  {
    int ti = i * w, li = ti, ri = ti + r;
    uint8_t fv = scl[ti], lv = scl[ti + w - 1];
    float val = (r + 1) * fv;
    for (int j = 0; j < r; j++)
      val += scl[ti + j];
    for (int j = 0; j <= r; j++)
    {
      val += scl[ri++] - fv;
      tcl[ti++] = round(val * iarr);
    }
    for (int j = r + 1; j < w - r; j++)
    {
      val += scl[ri++] - scl[li++];
      tcl[ti++] = round(val * iarr);
    }
    for (int j = w - r; j < w; j++)
    {
      val += lv - scl[li++];
      tcl[ti++] = round(val * iarr);
    }
  }
}

void boxBlurT_4(uint8_t *scl, uint8_t *tcl, int w, int h, int r)
{
  float iarr = 1.0 / (r + r + 1);
  for (int i = 0; i < w; i++)
  {
    int ti = i, li = ti, ri = ti + r * w;
    uint8_t fv = scl[ti], lv = scl[ti + w * (h - 1)];
    float val = (r + 1) * fv;
    for (int j = 0; j < r; j++)
      val += scl[ti + j * w];
    for (int j = 0; j <= r; j++)
    {
      val += scl[ri] - fv;
      tcl[ti] = round(val * iarr);
      ri += w;
      ti += w;
    }
    for (int j = r + 1; j < h - r; j++)
    {
      val += scl[ri] - scl[li];
      tcl[ti] = round(val * iarr);
      li += w;
      ri += w;
      ti += w;
    }
    for (int j = h - r; j < h; j++)
    {
      val += lv - scl[li];
      tcl[ti] = round(val * iarr);
      li += w;
      ti += w;
    }
  }
}

void boxBlur_4(uint8_t *scl, uint8_t *tcl, int w, int h, int r)
{
  for (int i = 0; i < w * h; i++)
    tcl[i] = scl[i];
  boxBlurH_4(tcl, scl, w, h, r);
  boxBlurT_4(scl, tcl, w, h, r);
}

void boxesForGauss(float sigma, int n, int *sizes)
{
  float wIdeal = sqrt((12 * sigma * sigma / n) + 1);
  int wl = floor(wIdeal);
  if (wl % 2 == 0)
    wl--;
  int wu = wl + 2;

  float mIdeal = (12 * sigma * sigma - n * wl * wl - 4 * n * wl - 3 * n) / (-4 * wl - 4);
  int m = round(mIdeal);

  for (int i = 0; i < n; i++)
    sizes[i] = i < m ? wl : wu;
}

void gaussBlur_4(uint8_t *scl, uint8_t *tcl, int w, int h, float sigma)
{
  int bxs[3];
  boxesForGauss(sigma, 3, bxs);

  boxBlur_4(scl, tcl, w, h, (bxs[0] - 1) / 2);
  boxBlur_4(tcl, scl, w, h, (bxs[1] - 1) / 2);
  boxBlur_4(scl, tcl, w, h, (bxs[2] - 1) / 2);
}

void applyGaussianBlurToSprite(TFT_eSprite &sprite, float sigma)
{
  int16_t w = sprite.width();
  int16_t h = sprite.height();

  uint8_t *srcBufferR = new uint8_t[w * h];
  uint8_t *srcBufferG = new uint8_t[w * h];
  uint8_t *srcBufferB = new uint8_t[w * h];
  uint8_t *dstBufferR = new uint8_t[w * h];
  uint8_t *dstBufferG = new uint8_t[w * h];
  uint8_t *dstBufferB = new uint8_t[w * h];

  for (int16_t y = 0; y < h; y++)
  {
    for (int16_t x = 0; x < w; x++)
    {
      uint16_t pixel = sprite.readPixel(x, y);
      srcBufferR[y * w + x] = ((pixel >> 11) & 0x1F) * 255 / 31;
      srcBufferG[y * w + x] = ((pixel >> 5) & 0x3F) * 255 / 63;
      srcBufferB[y * w + x] = (pixel & 0x1F) * 255 / 31;
    }
  }

  gaussBlur_4(srcBufferR, dstBufferR, w, h, sigma);
  gaussBlur_4(srcBufferG, dstBufferG, w, h, sigma);
  gaussBlur_4(srcBufferB, dstBufferB, w, h, sigma);

  for (int16_t y = 0; y < h; y++)
  {
    for (int16_t x = 0; x < w; x++)
    {
      uint8_t r = dstBufferR[y * w + x] * 31 / 255;
      uint8_t g = dstBufferG[y * w + x] * 63 / 255;
      uint8_t b = dstBufferB[y * w + x] * 31 / 255;
      sprite.drawPixel(x, y, ((r & 0x1F) << 11) | ((g & 0x3F) << 5) | (b & 0x1F));
    }
  }

  delete[] srcBufferR;
  delete[] srcBufferG;
  delete[] srcBufferB;
  delete[] dstBufferR;
  delete[] dstBufferG;
  delete[] dstBufferB;
}

unsigned long drawTime = 0;

uint32_t last_time_display = millis();
uint32_t loading_time = millis();
uint32_t target_time = millis();

float noiseScale = 0.1;

unsigned long sleepTimer = 0;

uint32_t colors[] = {TFT_RED, TFT_BLUE, TFT_GREEN, TFT_YELLOW, TFT_WHITE};

class Timer
{
public:
  int stopWatch;
  void startTime();
  int returnTime();
  void displayTime();
};

void Timer::startTime()
{
  stopWatch = micros();
}
int Timer::returnTime()
{
  return micros() - stopWatch;
}
void Timer::displayTime()
{
  sprite.setCursor(0, 0);
  sprite.print(micros() - stopWatch);
}

Timer timer;
const uint8_t planetRadius = 20;
const uint8_t planetPoints = 20;
Point point[planetPoints];
Point point2[planetPoints];

Particle starParticles[50];

void setup()
{
  pinMode(4, INPUT);
  // btn_left.setPressTicks(100);
  // btn_right.setPressTicks(100);

  for (int i = 0; i < planetPoints; i++)
  {
    point[i] = pointOnRotatedEllipse(planetRadius * 1.5f, planetRadius * 0.5f, (M_TWOPI / planetPoints) * i, M_PI * 0.9);
    point2[i] = pointOnRotatedEllipse(planetRadius * 1.5f, planetRadius * 0.5f, (M_TWOPI / planetPoints) * i, M_PI * 1.1);
  }

  for (int i = 0; i < 50; i++)
  {
    starParticles[i].x = random(0, 128 + 20);
    starParticles[i].y = random(0, 128 + 20);
    starParticles[i].vx = random(2, 10) / 20.0f;
    // starParticles[i].vy = random(-1, 1);
    starParticles[i].radius = random(1, 3) / 2.0f;
  }

  Serial.begin(921600);

  tft.init();
  tft.setRotation(2);
  sprite.createSprite(128, 128); // Create a sprite 128 x 128 pixels

  sprite.fillSprite(TFT_BLACK);
  sprite.loadFont(Medium12);
  sprite.pushSprite(0, 0);

  for (int i = 0; i < iconCount; i++)
  {
    iconTextSize[i] = sprite.drawCentreString(icons[i].c_str(), 64, 120, 8);
  }

  sleepTimer = millis();

  menu_right.attachClick(rightClick);
  menu_left.attachClick(leftClick);
}

bool isMenu = true;

float iconCurrentPositionX = 0;

float iconTargetX = 0;

float iconSpeedX = 0;

float planetCurrentPositionX = 0;
float planetCurrentPositionY = 0;

float planetTargetX = 0;
float planetTargetY = 0;

float planetSpeedX = 0;
float planetSpeedY = 0;

bool isNewIcon = true;

void voidFunction()
{
}

void MEDIACONTROL_RightHold()
{
  if (btn_left.isIdle())
  {
    bleKeyboard.write(KEY_MEDIA_MUTE);
  }
}

void MEDIACONTROL_RightClick()
{
  bleKeyboard.write(KEY_MEDIA_PLAY_PAUSE);
}
void MEDIACONTROL_RightDoubleClick()
{
  bleKeyboard.write(KEY_MEDIA_NEXT_TRACK);
}
void MEDIACONTROL_LeftClick()
{
  bleKeyboard.write(KEY_MEDIA_PLAY_PAUSE);
}
void MEDIACONTROL_LeftDoubleClick()
{
  bleKeyboard.write(KEY_MEDIA_PREVIOUS_TRACK);
}

unsigned long previousMillis = 0;
float menuAnimationProgress = 0;
float menuAnimationSpeed = 1.0f / 100.0f;

TaskHandle_t TurnOffBluetoothTask;
TaskHandle_t TurnOnBluetoothTask;

void TurnOffBluetooth(void *parameter)
{
  BLEDevice::deinit();
  Serial.print("TurnOffBluetoothTask running on core ");
  Serial.println(xPortGetCoreID());
  vTaskDelete(TurnOffBluetoothTask);
}

void TurnOnBluetooth(void *parameter)
{
  BLEDevice::init("Amognis");
  bleKeyboard.setName("Amognis");
  bleKeyboard.begin();
  Serial.print("TurnOnBluetoothTask running on core ");
  Serial.println(xPortGetCoreID());
  vTaskDelete(TurnOnBluetoothTask);
}

void loop()
{
  menu_left.tick();
  menu_right.tick();
  btn_left.tick();
  btn_right.tick();
  if (!btn_left.isIdle() || !btn_right.isIdle())
  {
    sleepTimer = millis();
  }
  else if (millis() - sleepTimer > 60000 && isMenu)
  {
    tft.writecommand(0x10);

    esp_sleep_enable_ext1_wakeup(GPIO_SEL_0, ESP_EXT1_WAKEUP_ALL_LOW);

    esp_deep_sleep_start();
  }

  if (rightClicked || leftClicked)
  {

    if (millis() - firstButtonClickTime > timeThreshold)
    {

      if (rightClicked)
      {
        currentIconIndex++;
        iconTargetX -= 128;
        // bleKeyboard.write(KEY_MEDIA_PLAY_PAUSE);
      }
      if (leftClicked)
      {
        if (!currentIconIndex == 0)
        {
          currentIconIndex--;
          iconTargetX += 128;
        }
      }
      resetButtonStates();
    }
  }

  if (rightClicked && leftClicked && isMenu)
  {
    Serial.println("Both Clicked");
    isNewIcon = true;
    isMenu = false;
    tft.fillScreen(TFT_BLACK);
    resetButtonStates();
  }
  else if (menu_left.isLongPressed() && menu_right.isLongPressed() && !isMenu)
  {
    isNewIcon = true;
    isMenu = true;
  }

  if (isNewIcon && !isMenu && menuAnimationProgress >= 1)
  {
    Serial.println(menuAnimationProgress);
    switch (currentIconIndex)
    {
    case MEDIACONTROL:
      xTaskCreatePinnedToCore(TurnOnBluetooth, "TurnOnBluetoothTask", 10000, NULL, 1, &TurnOnBluetoothTask, 0);

      btn_right.attachLongPressStart(MEDIACONTROL_RightHold);
      btn_right.attachClick(MEDIACONTROL_RightClick);
      btn_right.attachDoubleClick(MEDIACONTROL_RightDoubleClick);
      btn_left.attachClick(MEDIACONTROL_LeftClick);
      btn_left.attachDoubleClick(MEDIACONTROL_LeftDoubleClick);
      break;

    default:

      break;
    }

    isNewIcon = false;
  }
  else if (isNewIcon && isMenu && menuAnimationProgress <= 0)
  {
    xTaskCreatePinnedToCore(TurnOffBluetooth, "TurnOffBluetoothTask", 10000, NULL, 1, &TurnOffBluetoothTask, 0);
    Serial.println(menuAnimationProgress);
    menu_right.attachClick(rightClick);
    menu_left.attachClick(leftClick);

    isNewIcon = false;
  }
  else if (isNewIcon)
  {
    menu_right.attachClick(voidFunction);
    menu_left.attachClick(voidFunction);
    btn_right.attachClick(voidFunction);
    btn_left.attachClick(voidFunction);
  }

  /*
  if (millis() - last_time_display > 5000)
  {
    scanNetworks();
    last_time_display = millis();
  }

  if (millis() - target_time > 500)
  {
    planetTargetX = random(-4, 4);
    planetTargetY = random(-4, 4);
    target_time = millis();
  }
  */
  if (millis() - loading_time > 16)
  {

    timer.startTime();
    sprite.fillScreen(TFT_BLACK);

    progress = (millis() % 4000) / 4000.0f;

    if (isMenu)
    {
      if (menuAnimationProgress > 0)
      {
        menuAnimationProgress -= menuAnimationSpeed;
      }

      sprite.setCursor(50, 0);

      sprite.print(progress);
      sprite.setCursor(90, 0);
      sprite.print(iconCurrentPositionX);
      sprite.setCursor(0, 20);

      planetSpeedX = lerp(planetSpeedX, (planetTargetX - planetCurrentPositionX) * 0.2f, 0.3f);
      planetCurrentPositionX += planetSpeedX;
      planetSpeedY = lerp(planetSpeedY, (planetTargetY - planetCurrentPositionY) * 0.2f, 0.3f);
      planetCurrentPositionY += planetSpeedY;

      iconSpeedX = lerp(iconSpeedX, (iconTargetX - iconCurrentPositionX) * 0.2f, 0.3f);
      iconCurrentPositionX += iconSpeedX;

      for (int i = 0; i < 50; i++)
      {
        starParticles[i].x += starParticles[i].vx - iconSpeedX;
        if (starParticles[i].x > 128 + 20)
        {
          starParticles[i].x = random(0, 11);
          starParticles[i].y = random(0, 128 + 20);
          starParticles[i].vx = random(2, 5) / 20.0f;
        }
        else if (starParticles[i].x < 0)
        {
          starParticles[i].x = random(128 + 10, 128 + 20);
          starParticles[i].y = random(0, 128 + 20);
          starParticles[i].vx = random(2, 5) / 20.0f;
        }
        sprite.drawSpot(starParticles[i].x - 10 - planetCurrentPositionX * 0.5, starParticles[i].y - 10 - planetCurrentPositionY * 0.5, starParticles[i].radius, TFT_WHITE);
      }

      if (iconCurrentPositionX > currentIconIndex * -128 + 40)
      {
        drawIcon(currentIconIndex - 1, (currentIconIndex - 1) * 128 + planetCurrentPositionX + iconCurrentPositionX, planetCurrentPositionY);

        titleText(icons[currentIconIndex - 1].c_str(), (currentIconIndex - 1) * 128 + iconCurrentPositionX, iconTextSize[currentIconIndex - 1]);
      }

      drawIcon(currentIconIndex, currentIconIndex * 128 + planetCurrentPositionX + iconCurrentPositionX, planetCurrentPositionY);

      titleText(icons[currentIconIndex].c_str(), currentIconIndex * 128 + iconCurrentPositionX, iconTextSize[currentIconIndex]);

      if (iconCurrentPositionX < (currentIconIndex * -128 - 40))
      {
        drawIcon(currentIconIndex + 1, (currentIconIndex + 1) * 128 + planetCurrentPositionX + iconCurrentPositionX, planetCurrentPositionY);

        titleText(icons[currentIconIndex + 1].c_str(), (currentIconIndex + 1) * 128 + iconCurrentPositionX, iconTextSize[currentIconIndex + 1]);
      }

      loading();
      sprite.fillSmoothCircle(64, 64, menuAnimationProgress * 50, TFT_WHITE);
    }
    else
    {
      if (menuAnimationProgress < 1)
      {
        menuAnimationProgress += menuAnimationSpeed;
      }
      sprite.fillSmoothCircle(64, 64, menuAnimationProgress * 50, TFT_WHITE);
    }

    // applyGaussianBlurToSprite(sprite, 2);
    timer.displayTime();
    float battery_voltage = analogRead(4) / ADC_RESOLUTION * VOLTAGE_REFERENCE * VOLTAGE_DIVIDER_RATIO + VOLTAGE_DIVIDER_OFFSET;
    uint8_t battery_percentage = ((battery_voltage - min_voltage) / (max_voltage - min_voltage)) * 100;
    sprite.setTextColor(TFT_BLACK);
    sprite.setCursor(2, 20);
    sprite.fillSmoothRoundRect(0, 20, 19, 10, 2, TFT_DARKGREY);
    sprite.fillSmoothRoundRect(0, 20, 19 * battery_percentage / 100, 10, 2, TFT_LIGHTGREY);

    sprite.fillSmoothRoundRect(17, 23, 5, 4, 1, TFT_DARKGREY);
    sprite.drawFastVLine(19, 20, 10, TFT_BLACK);
    sprintf(str, "%d", battery_percentage);
    sprite.drawCentreString(str, 9, 20, 2);
    sprite.setTextColor(TFT_WHITE);

    /*

    sprite.println(millis());
    sprite.println(bleKeyboard.isConnected());*/
    // memcpy(spriteBuffer2,sprite._img,  128 * 128 * 2);
    sprite.pushSprite(0, 0);

    loading_time = millis();
  }
}

uint32_t graph_time = millis();

const uint8_t graphPoints = 10;
uint8_t graphCurrentPositionY[graphPoints];
uint8_t graphTargetY[graphPoints];
float graphY[graphPoints];

void drawGraph(int16_t xOffset, int16_t yOffset, uint8_t width, uint8_t height, uint16_t color)
{
  width /= graphPoints;
  if (millis() - graph_time > 500)
  {
    for (uint8_t i = 0; i < graphPoints; i++)
    {
      graphTargetY[i] = random(height - height / 4, height + height / 4);
      constrain(graphTargetY[i],height - height / 4, height + height / 4);
    }
    graph_time = millis();
  }
  for (uint8_t i = 0; i < graphPoints; i++)
  {
    graphY[i] = lerp(graphY[i], (graphTargetY[i] - graphCurrentPositionY[i]) * 0.9f, 0.2f);
    graphCurrentPositionY[i] += graphY[i];

    // sprite.drawSpot(40 + 20 * i, graphCurrentPositionY[i], 7, TFT_WHITE);
  }
  for (uint8_t i = 0; i < graphPoints - 1; i++)
  {
    sprite.drawWideLine(width * i + xOffset, graphCurrentPositionY[i] + yOffset, width + width * i + xOffset, graphCurrentPositionY[i + 1] + yOffset, 1, color); // tft.color565(255 * i / 9, 255 * i / 9, 255 * i / 9));
  }
}

void drawCenteredTriangle(int16_t x, int16_t y, int16_t size, uint16_t color, uint16_t bgcolor)
{
  sprite.fillTriangle(x - size / 2, y - size / 1.75f, x - size / 2, y + size / 1.75f, x + size / 2, y, bgcolor);
  sprite.drawWideLine(x - size / 2, y - size / 1.75f, x - size / 2, y + size / 1.75f, 1, color);
  sprite.drawWideLine(x - size / 2, y - size / 1.75f, x + size / 2, y, 1, color);
  sprite.drawWideLine(x - size / 2, y + size / 1.75f, x + size / 2, y, 1, color);
}

void drawSkip(int16_t x, int16_t y, int16_t size, uint16_t color, uint16_t bgcolor)
{
  drawCenteredTriangle(x, y, size, color, bgcolor);
  drawCenteredTriangle(x + size, y, size, color, bgcolor);
}

void drawCenteredRectangle(int16_t x, int16_t y, int16_t width, int16_t height, uint16_t color, uint16_t bgcolor)
{
  sprite.fillRect(x - width / 2, y - height / 2, width, height, bgcolor);
  sprite.drawRect(x - width / 2, y - height / 2, width, height, color);
}

void drawCenteredPause(int16_t x, int16_t y, int16_t size, uint16_t color, uint16_t bgcolor)
{
  drawCenteredRectangle(x - size / 4, y, size / 3, size, color, bgcolor);
  drawCenteredRectangle(x + size / 4, y, size / 3, size, color, bgcolor);
}

void drawGear(int32_t centerX, int32_t centerY, int32_t outerRadius, int32_t innerRadius, int32_t centerRadius, int32_t numTeeth, float rotation, uint32_t color)
{
  centerX += 64;
  centerY += 64;

  const float toothAngle = 2 * M_PI / numTeeth;
  const float toothAngleDegrees = 360.0f / numTeeth;

  sprite.fillCircle(centerX, centerY, innerRadius + 2, TFT_BLACK);
  sprite.fillCircle(centerX, centerY, centerRadius, TFT_BLACK);

  sprite.drawSmoothRoundRect(centerX - innerRadius, centerY - innerRadius, innerRadius, innerRadius, 0, 0, TFT_WHITE, TFT_BLACK);

  int16_t arcStartAngle = int16_t((1) / (numTeeth)*360.0f + (toothAngleDegrees / 6.0f) + 360 * rotation) % 360;
  int16_t arcEndAngle = int16_t(((1) / (numTeeth)) * 360.0f - (toothAngleDegrees / 6.0f) + 360 * rotation) % 360;
  // sprite.drawSmoothArc(centerX, centerY, innerRadius, innerRadius, arcStartAngle, arcEndAngle, TFT_WHITE, false);
  for (int32_t i = 0; i < numTeeth; i++)
  {

    // sprite.drawSmoothArc(centerX, centerY, innerRadius, innerRadius, int32_t((i * 2.0f) / (numTeeth * 2.0f) * 360.0f - 720 / numTeeth + 360 * rotation) % 360, int32_t(((i * 2.0f + 1.0f) / (numTeeth * 2.0f)) * 360.0f + 720 / numTeeth + 5 + 360 * rotation) % 360, TFT_WHITE, false);

    float x0 = innerRadius * cos(toothAngle * i + toothAngle / 3 + 2 * M_PI * rotation);
    float y0 = innerRadius * sin(toothAngle * i + toothAngle / 3 + 2 * M_PI * rotation);
    float x1 = outerRadius * cos(toothAngle * i + toothAngle / 3 * (innerRadius / float(outerRadius)) + 2 * M_PI * rotation);
    float y1 = outerRadius * sin(toothAngle * i + toothAngle / 3 * (innerRadius / float(outerRadius)) + 2 * M_PI * rotation);

    float x2 = innerRadius * cos(toothAngle * i - toothAngle / 3 + 2 * M_PI * rotation);
    float y2 = innerRadius * sin(toothAngle * i - toothAngle / 3 + 2 * M_PI * rotation);
    float x3 = outerRadius * cos(toothAngle * i - toothAngle / 3 * (innerRadius / float(outerRadius)) + 2 * M_PI * rotation);
    float y3 = outerRadius * sin(toothAngle * i - toothAngle / 3 * (innerRadius / float(outerRadius)) + 2 * M_PI * rotation);

    sprite.fillTriangle(x0 * 0.8 + centerX, y0 * 0.8 + centerY, x1 + centerX, y1 + centerY, x2 + centerX, y2 + centerY, TFT_BLACK);
    sprite.fillTriangle(x1 + centerX, y1 + centerY, x2 * 0.8 + centerX, y2 * 0.8 + centerY, x3 + centerX, y3 + centerY, TFT_BLACK);

    sprite.drawWideLine(x0 + centerX, y0 + centerY, x1 + centerX, y1 + centerY, 1, color);
    sprite.drawWideLine(x2 + centerX, y2 + centerY, x3 + centerX, y3 + centerY, 1, color);
    sprite.drawWideLine(x3 + centerX, y3 + centerY, x1 + centerX, y1 + centerY, 1, color);
  }
  sprite.drawSmoothRoundRect(centerX - centerRadius, centerY - centerRadius, centerRadius, centerRadius, 0, 0, TFT_WHITE, TFT_BLACK);
}

void drawPlanet(int16_t x, int16_t y)
{
  y += 64;
  x += 64;
  for (int i = 0; i < 3; i++)
  {
    sprite.drawWideLine(point[(i + 7) % planetPoints].x + x, point[(i + 7) % planetPoints].y + y, point[(i + 8) % planetPoints].x + x, point[(i + 8) % planetPoints].y + y, 1, TFT_WHITE);
  }
  for (int i = 0; i < 3; i++)
  {
    sprite.drawWideLine(point[(i + 0) % planetPoints].x + x, point[(i + 0) % planetPoints].y + y, point[(i + 1) % planetPoints].x + x, point[(i + 1) % planetPoints].y + y, 1, TFT_WHITE);
  }

  // Small planet behind
  if ((progress > 0.1f) && (progress < 0.4f))
  {
    sprite.drawSpot(lerp(point[uint8_t(progress * planetPoints)].x, point[(uint8_t(progress * planetPoints) + 1) % planetPoints].x, fmod(progress * planetPoints, 1)) + x, lerp(point[uint8_t(progress * planetPoints)].y, point[(uint8_t(progress * planetPoints) + 1) % planetPoints].y, fmod(progress * planetPoints, 1)) + y, planetRadius * 0.25 + 1, TFT_WHITE);
    sprite.drawSpot(lerp(point[uint8_t(progress * planetPoints)].x, point[(uint8_t(progress * planetPoints) + 1) % planetPoints].x, fmod(progress * planetPoints, 1)) + x, lerp(point[uint8_t(progress * planetPoints)].y, point[(uint8_t(progress * planetPoints) + 1) % planetPoints].y, fmod(progress * planetPoints, 1)) + y, planetRadius * 0.25, TFT_BLACK);
  }
  if ((((1.0f - progress) > 0.3) && ((1.0f - progress) < 0.8)))
  {
    sprite.drawSpot(0.75 * lerp(point2[uint8_t((1.0f - progress) * planetPoints + 14) % planetPoints].x, point2[(uint8_t((1.0f - progress) * planetPoints) + 15) % planetPoints].x, fmod((1.0f - progress) * planetPoints, 1)) + x, 0.75 * lerp(point2[uint8_t((1.0f - progress) * planetPoints + 14) % planetPoints].y, point2[(uint8_t((1.0f - progress) * planetPoints) + 15) % planetPoints].y, fmod((1.0f - progress) * planetPoints, 1)) + y, planetRadius * 0.1, TFT_WHITE);
  }

  sprite.fillCircle(x, y, planetRadius - 1, TFT_BLACK);
  sprite.drawSmoothRoundRect(x - planetRadius, y - planetRadius, planetRadius, planetRadius, 0, 0, TFT_WHITE, TFT_BLACK);

  for (int i = 0; i < 10; i++)
  {
    sprite.drawWideLine(point[(i + 10) % planetPoints].x + x, point[(i + 10) % planetPoints].y + y, point[(i + 11) % planetPoints].x + x, point[(i + 11) % planetPoints].y + y, 1, TFT_WHITE);
  }

  // Small planet in front
  if (!(((1.0f - progress) > 0.3) && ((1.0f - progress) < 0.8)))
  {
    sprite.drawSpot(0.75 * lerp(point2[uint8_t((1.0f - progress) * planetPoints + 14) % planetPoints].x, point2[(uint8_t((1.0f - progress) * planetPoints) + 15) % planetPoints].x, fmod((1.0f - progress) * planetPoints, 1)) + x, 0.75 * lerp(point2[uint8_t((1.0f - progress) * planetPoints + 14) % planetPoints].y, point2[(uint8_t((1.0f - progress) * planetPoints) + 15) % planetPoints].y, fmod((1.0f - progress) * planetPoints, 1)) + y, planetRadius * 0.1, TFT_WHITE);
  }
  if (!((progress > 0.1f) && (progress < 0.4f)))
  {

    sprite.drawSpot(lerp(point[uint8_t(progress * planetPoints)].x, point[(uint8_t(progress * planetPoints) + 1) % planetPoints].x, fmod(progress * planetPoints, 1)) + x, lerp(point[uint8_t(progress * planetPoints)].y, point[(uint8_t(progress * planetPoints) + 1) % planetPoints].y, fmod(progress * planetPoints, 1)) + y, planetRadius * 0.25 + 1, TFT_WHITE);
    sprite.drawSpot(lerp(point[uint8_t(progress * planetPoints)].x, point[(uint8_t(progress * planetPoints) + 1) % planetPoints].x, fmod(progress * planetPoints, 1)) + x, lerp(point[uint8_t(progress * planetPoints)].y, point[(uint8_t(progress * planetPoints) + 1) % planetPoints].y, fmod(progress * planetPoints, 1)) + y, planetRadius * 0.25, TFT_BLACK);
  }
}

void drawPRESENTATION(int16_t x, int16_t y)
{
  uint8_t boxSize = 50;

  x += 64;
  y += 64;
  sprite.fillRoundRect(x - boxSize / 2, y - boxSize / 3, boxSize, boxSize / 1.5, 4, TFT_BLACK);
  sprite.drawSmoothRoundRect(x - boxSize / 2, y - boxSize / 3, 5, 5, boxSize, boxSize / 1.5, TFT_WHITE, TFT_BLACK);
  drawGraph(x - boxSize / 2+2, y - boxSize / 3, 10, 8, TFT_WHITE);
}

void drawMEDIACONTROL(int16_t x, int16_t y)
{
  y += 64;
  x += 64;

  if ((progress > 0.1f) && (progress < 0.4f))
  {
    drawCenteredPause(1.2f * lerp(point[uint8_t(progress * planetPoints)].x, point[(uint8_t(progress * planetPoints) + 1) % planetPoints].x, fmod(progress * planetPoints, 1)) + x, 1.2f * lerp(point[uint8_t(progress * planetPoints)].y, point[(uint8_t(progress * planetPoints) + 1) % planetPoints].y, fmod(progress * planetPoints, 1)) + y, 17, TFT_WHITE, TFT_BLACK);
  }
  if ((((1.0f - progress) > 0.3) && ((1.0f - progress) < 0.8)))
  {
    drawSkip(lerp(point2[uint8_t((1.0f - progress) * planetPoints + 14) % planetPoints].x, point2[(uint8_t((1.0f - progress) * planetPoints) + 15) % planetPoints].x, fmod((1.0f - progress) * planetPoints, 1)) + x, lerp(point2[uint8_t((1.0f - progress) * planetPoints + 14) % planetPoints].y, point2[(uint8_t((1.0f - progress) * planetPoints) + 15) % planetPoints].y, fmod((1.0f - progress) * planetPoints, 1)) + y, 10, TFT_WHITE, TFT_BLACK);
  }

  drawCenteredTriangle(x + 4, y, 35, TFT_WHITE, TFT_BLACK);

  // Small planet in front
  if (!(((1.0f - progress) > 0.3) && ((1.0f - progress) < 0.8)))
  {
    drawSkip(lerp(point2[uint8_t((1.0f - progress) * planetPoints + 14) % planetPoints].x, point2[(uint8_t((1.0f - progress) * planetPoints) + 15) % planetPoints].x, fmod((1.0f - progress) * planetPoints, 1)) + x, lerp(point2[uint8_t((1.0f - progress) * planetPoints + 14) % planetPoints].y, point2[(uint8_t((1.0f - progress) * planetPoints) + 15) % planetPoints].y, fmod((1.0f - progress) * planetPoints, 1)) + y, 10, TFT_WHITE, TFT_BLACK);
  }
  if (!((progress > 0.1f) && (progress < 0.4f)))
  {
    drawCenteredPause(1.2f * lerp(point[uint8_t(progress * planetPoints)].x, point[(uint8_t(progress * planetPoints) + 1) % planetPoints].x, fmod(progress * planetPoints, 1)) + x, 1.2f * lerp(point[uint8_t(progress * planetPoints)].y, point[(uint8_t(progress * planetPoints) + 1) % planetPoints].y, fmod(progress * planetPoints, 1)) + y, 17, TFT_WHITE, TFT_BLACK);
  }
}

void drawSETTINGS(int16_t x, int16_t y)
{
  if ((((1.0f - progress) > 0.3) && ((1.0f - progress) < 0.8)))
  {
    drawGear(1.5f * lerp(point2[uint8_t((1.0f - progress) * planetPoints + 14) % planetPoints].x, point2[(uint8_t((1.0f - progress) * planetPoints) + 15) % planetPoints].x, fmod((1.0f - progress) * planetPoints, 1)) + x, 1.5f * lerp(point2[uint8_t((1.0f - progress) * planetPoints + 14) % planetPoints].y, point2[(uint8_t((1.0f - progress) * planetPoints) + 15) % planetPoints].y, fmod((1.0f - progress) * planetPoints, 1)) + y, 10, 7, 3, 5, -2 * progress, TFT_WHITE);
    sprite.drawSpot(1.5f * lerp(point2[uint8_t((1.0f - progress) * planetPoints + 14) % planetPoints].x, point2[(uint8_t((1.0f - progress) * planetPoints) + 15) % planetPoints].x, fmod((1.0f - progress) * planetPoints, 1)) + x, 1.5f * lerp(point2[uint8_t((1.0f - progress) * planetPoints + 14) % planetPoints].y, point2[(uint8_t((1.0f - progress) * planetPoints) + 15) % planetPoints].y, fmod((1.0f - progress) * planetPoints, 1)) + y, planetRadius * 0.1, TFT_WHITE);
  }
  drawGear(x, y, 30, 25, 17, 9, progress, TFT_WHITE);
  if (!(((1.0f - progress) > 0.3) && ((1.0f - progress) < 0.8)))
  {
    drawGear(1.5f * lerp(point2[uint8_t((1.0f - progress) * planetPoints + 14) % planetPoints].x, point2[(uint8_t((1.0f - progress) * planetPoints) + 15) % planetPoints].x, fmod((1.0f - progress) * planetPoints, 1)) + x, 1.5f * lerp(point2[uint8_t((1.0f - progress) * planetPoints + 14) % planetPoints].y, point2[(uint8_t((1.0f - progress) * planetPoints) + 15) % planetPoints].y, fmod((1.0f - progress) * planetPoints, 1)) + y, 10, 7, 3, 5, -2 * progress, TFT_WHITE);
    sprite.drawSpot(1.5f * lerp(point2[uint8_t((1.0f - progress) * planetPoints + 14) % planetPoints].x, point2[(uint8_t((1.0f - progress) * planetPoints) + 15) % planetPoints].x, fmod((1.0f - progress) * planetPoints, 1)) + x, 1.5f * lerp(point2[uint8_t((1.0f - progress) * planetPoints + 14) % planetPoints].y, point2[(uint8_t((1.0f - progress) * planetPoints) + 15) % planetPoints].y, fmod((1.0f - progress) * planetPoints, 1)) + y, planetRadius * 0.1, TFT_WHITE);
  }
}

void drawIcon(uint8_t icon, int16_t x, int16_t y)
{
  switch (icon)
  {
  case PRESENTATION:
    drawPRESENTATION(x, y);
    break;
  case MEDIACONTROL:
    drawMEDIACONTROL(x, y);
    break;
  case SETTINGS:
    drawSETTINGS(x, y);
    break;

  default:
    drawPlanet(x, y);
    break;
  }
}

uint8_t scanNetworks()
{
  int n; // = WiFi.scanNetworks();

  if (n == 0)
  {
    sprite.fillRect(13, 120, 128, 128, TFT_BLACK);
    sprite.setCursor(13, 120);
    sprite.print("No networks found");
  }
  else
  {
    sprite.fillRect(13, 120, 128, 128, TFT_BLACK);
    sprite.setCursor(13, 120);
    sprite.print(n);
    sprite.print(" networks found");
    sprite.setCursor(0, 0);
    for (int i = 0; i < n; ++i)
    {
      // sprite.println(WiFi.SSID(i));
    }
  }
  return n;
}

//
//
//
//
// Lerp functions

uint16_t
lerpRGB565(uint32_t c, uint32_t d, float t)
{
  // Extract the R, G, and B components from the 24-bit color values
  uint8_t c_r = (c >> 16) & 0xFF;
  uint8_t c_g = (c >> 8) & 0xFF;
  uint8_t c_b = c & 0xFF;
  uint8_t d_r = (d >> 16) & 0xFF;
  uint8_t d_g = (d >> 8) & 0xFF;
  uint8_t d_b = d & 0xFF;

  // Convert the 8-bit R, G, and B components to 5-bit and 6-bit values
  uint8_t c_r5 = (c_r * 31) / 255;
  uint8_t c_g6 = (c_g * 63) / 255;
  uint8_t c_b5 = (c_b * 31) / 255;
  uint8_t d_r5 = (d_r * 31) / 255;
  uint8_t d_g6 = (d_g * 63) / 255;
  uint8_t d_b5 = (d_b * 31) / 255;

  // Linearly interpolate the R, G, and B components
  uint8_t r5 = (uint8_t)lerp(c_r5, d_r5, t);
  uint8_t g6 = (uint8_t)lerp(c_g6, d_g6, t);
  uint8_t b5 = (uint8_t)lerp(c_b5, d_b5, t);

  // Pack the interpolated R, G, and B components into a single 16-bit RGB 565 color value
  uint16_t rgb565 = ((r5 << 11) & 0xF800) | ((g6 << 5) & 0x7E0) | (b5 & 0x1F);

  return rgb565;
}

bool loading()
{
  sprite.drawSmoothArc(4, 123, 4, 3, uint16_t(lerp(0, 359, Ease::inOut(constrain((progress - 0.375) * 1.6, 0, 1))) + lerp(0, 359, Ease::inOut(progress)) + 170) % 360, uint16_t(lerp(0, 359, Ease::inOut(constrain(progress * 2, 0, 1))) + lerp(0, 359, Ease::inOut(progress)) + 175) % 360, TFT_WHITE, TFT_BLACK);
  return (progress == 0);
}

Point pointOnRotatedEllipse(float a, float b, float t, float rotation)
{
  // Calculate the point on the non-rotated ellipse
  float x = a * cosf(t);
  float y = b * sinf(t);
  // Apply the rotation
  float rotatedX = x * cosf(rotation) - y * sinf(rotation);
  float rotatedY = x * sinf(rotation) + y * cosf(rotation);
  // Return the rotated point
  Point result = {rotatedX, rotatedY};
  return result;
}

void titleText(String title, int16_t x, int16_t boxWidth)
{
  // sprite.drawSmoothRoundRect(64 - boxWidth / 2 - 5 + x, 128 - 14,6,6, boxWidth + 10, 13, TFT_WHITE,TFT_BLACK);
  // sprite.setTextColor(TFT_BLACK);
  sprite.drawString(title, 64 - boxWidth / 2 + x, 128 - 12);
  // sprite.setTextColor(TFT_WHITE);
}

void rightClick()
{
  rightClicked = true;
  updateFirstButtonClickTime();
}

void leftClick()
{

  leftClicked = true;
  updateFirstButtonClickTime();
}

void updateFirstButtonClickTime()
{
  if (firstButtonClickTime == 0)
  {
    firstButtonClickTime = millis();
  }
}

void resetButtonStates()
{
  rightClicked = false;
  leftClicked = false;
  firstButtonClickTime = 0;
}