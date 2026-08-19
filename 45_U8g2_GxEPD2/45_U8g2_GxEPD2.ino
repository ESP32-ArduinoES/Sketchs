#include <GxEPD2_BW.h>

// Necesita la biblioteca Adafruit-GFX y Adafruit_BusIO
#include <U8g2_for_Adafruit_GFX.h> 

// Pines para XIAO ESP32-C3
#define EPD_CS    3
#define EPD_DC    5
#define EPD_RST   2
#define EPD_BUSY  4

// Inicializar pantalla de 7.5" 800x480
GxEPD2_BW<GxEPD2_750_T7, GxEPD2_750_T7::HEIGHT> display(
  GxEPD2_750_T7(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY)
);

// Adaptador para fuentes U8g2
U8G2_FOR_ADAFRUIT_GFX u8g2Fonts;

void setup() {
  // 1. Iniciar pantalla
  display.init(115200, true, 2, false);
  
  // 2. Conectar U8g2 a la pantalla
  u8g2Fonts.begin(display);
  
  // 3. Configurar fuente grande
  u8g2Fonts.setFont(u8g2_font_helvB24_tf);
  
  // 4. Configurar colores
  u8g2Fonts.setForegroundColor(GxEPD_BLACK);  // Texto negro
  u8g2Fonts.setBackgroundColor(GxEPD_WHITE);  // Fondo blanco

  // 5. Dibujar en la pantalla (método de doble buffer)
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);  // Limpiar pantalla
    
    u8g2Fonts.setCursor(110, 250);    // Posición del texto
    u8g2Fonts.print("Mañana ESPAÑA Á É Í Ó Ú á é í ó ú ü");
    
  } while (display.nextPage());  // Actualizar pantalla
}

void loop() {}
