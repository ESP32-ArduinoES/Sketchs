#include "TFT_eSPI.h"

#ifdef EPAPER_ENABLE
EPaper epaper;
#endif

void setup() {
#ifdef EPAPER_ENABLE
    epaper.begin();
    epaper.fillScreen(TFT_WHITE);
    
    //-------------------------------------------------
    // 1. LÍNEA
    //-------------------------------------------------
    epaper.drawLine(99, 99, 699, 99, TFT_BLACK);
    
    //-------------------------------------------------
    // 2. RECTÁNGULO CON TRAZO (solo borde)
    //-------------------------------------------------
    epaper.drawRect(99, 159, 259, 139, TFT_BLACK);
   
    //-------------------------------------------------
    // 3. RECTÁNGULO RELLENO (interior completo)
    //-------------------------------------------------
    epaper.fillRect(439, 159, 259, 139, TFT_BLACK);
  
    //-------------------------------------------------
    // 4. TEXTO EN TRES TAMAÑOS
    //-------------------------------------------------
    epaper.setTextColor(TFT_BLACK, TFT_WHITE);
    
    epaper.setTextSize(1);
    epaper.drawString("Texto 1 ArduinoES", 100, 330);
    
    epaper.setTextSize(2);
    epaper.drawString("Texto 2 ArduinoES", 100, 360);
    
    epaper.setTextSize(3);
    epaper.drawString("Texto 3 ArduinoES", 100, 410);
    
    //-------------------------------------------------
    // REFRESCAR PANTALLA
    //-------------------------------------------------
    epaper.update();
#endif
}

void loop() {
    // Vacío
}
