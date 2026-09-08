/*  ==============================================
    TUTORIAL: Pantalla Táctil ILI9488 con ESP32
    AUTOR: CARLOS MIRANDA (IA DeepSeek)
    DIMENSIONES: 480x320
    CALIBRACIÓN: Valores únicos para esta pantalla
    ==============================================
*/

// ================================================================
// 1. BIBLIOTECAS
// ================================================================
#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h> 

// ================================================================
// 2. CALIBRACIÓN
// ================================================================

#define MIN_RAW_X 250
#define MAX_RAW_X 3600
#define MIN_RAW_Y 250
#define MAX_RAW_Y 3800

// ================================================================
// 3. OBJETOS | CONSTRUCTOR | INSTANCIA
// ================================================================

TFT_eSPI tft = TFT_eSPI();
SPIClass touchscreenSPI = SPIClass(VSPI);
XPT2046_Touchscreen touchscreen(XPT2046_CS, XPT2046_IRQ);

// ================================================================
// 4. VARIABLES
// ================================================================

int lastX = -1, lastY = -1;
uint16_t currentColor = TFT_RED;
int colorIndex = 0;

uint16_t colors[] = {TFT_RED, TFT_ORANGE, TFT_YELLOW, TFT_GREEN, 
                      TFT_CYAN, TFT_BLUE, TFT_MAGENTA, TFT_WHITE};

// ================================================================
// 5. FUNCIONES
// ================================================================

void setBrightness(uint8_t level) {
  analogWrite(TFT_BL, level);
}

void initBacklight() {
  pinMode(TFT_BL, OUTPUT);
  setBrightness(200);
}

bool isInButton(int px, int py, int x, int y, int w, int h) {
  return (px >= x && px <= x + w && py >= y && py <= y + h);
}

void drawButton(int x, int y, int w, int h, String label, uint16_t color, bool showColor = false) {
  tft.fillRoundRect(x, y, w, h, 10, color);
  tft.drawRoundRect(x, y, w, h, 10, TFT_WHITE);
  tft.setTextColor(TFT_WHITE, color);
  tft.setTextSize(2);
  
  if (showColor) {
    tft.setCursor(x + 15, y + h/2 - 8);
    tft.print(label);
    tft.fillRect(x + w - 45, y + 8, 35, h - 16, currentColor);
    tft.drawRect(x + w - 45, y + 8, 35, h - 16, TFT_WHITE);
  } else {
    tft.setCursor(x + (w - label.length() * 12) / 2, y + h/2 - 8);
    tft.print(label);
  }
}

void drawMenu() {
  tft.fillRect(0, 0, 480, 60, TFT_BLACK);
  tft.drawLine(0, 60, 480, 60, TFT_DARKGREY);
  
  drawButton(40, 5, 180, 50, "COLOR", TFT_PURPLE, true);
  drawButton(260, 5, 180, 50, "LIMPIAR", TFT_RED);
}

void handleTouch(int x, int y) {
  if (y < 60) {
    if (isInButton(x, y, 40, 5, 180, 50)) {
      colorIndex = (colorIndex + 1) % 8;
      currentColor = colors[colorIndex];
      drawMenu();
      return;
    }
    if (isInButton(x, y, 260, 5, 180, 50)) {
      tft.fillScreen(TFT_BLACK);
      drawMenu();
      lastX = -1;
      lastY = -1;
      return;
    }
    return;
  }
  
  tft.fillCircle(x, y, 4, currentColor);
  if (lastX >= 0 && lastY >= 0) {
    tft.drawLine(lastX, lastY, x, y, currentColor);
  }
  lastX = x;
  lastY = y;
}

// ================================================================
// 6. SETUP Y LOOP
// ================================================================

void setup() {
  initBacklight();
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  
  touchscreenSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  touchscreen.begin(touchscreenSPI);
  touchscreen.setRotation(3);
  
  drawMenu();
}

void loop() {
  if (touchscreen.tirqTouched() && touchscreen.touched()) {
    TS_Point p = touchscreen.getPoint();
    
    int touchX = constrain(map(p.x, MIN_RAW_X, MAX_RAW_X, 0, 480), 0, 479);
    int touchY = constrain(map(p.y, MIN_RAW_Y, MAX_RAW_Y, 0, 320), 0, 319);
    
    handleTouch(touchX, touchY);
    delay(50);
  }
  
  if (!touchscreen.touched()) {
    lastX = -1;
    lastY = -1;
  }
  
  delay(10);
}                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     
