#include <LiquidCrystal_I2C.h>

#include <image.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);

// VALORES DE PRUEBA (DHT11/22 | BME280)
float humedad = 60.5;
float temperatura = 22.5;

void setup() {
  lcd.init();
  lcd.backlight();
  
  // Crear los caracteres personalizados
  lcd.createChar(0, gota);
  lcd.createChar(1, termometro);
  lcd.createChar(2, corazon);
}

void loop() {
  lcd.clear();
  
  // PRIMERA LÍNEA: ° 22C 60%
  lcd.setCursor(0, 0);
  lcd.write(1);              // Termómetro
  lcd.print(" ");
  lcd.print(temperatura, 1); // Temperatura con un decimal
  lcd.print("C ");
  lcd.write(0);              // Gota
  lcd.print(" ");
  lcd.print(humedad, 1);     // Humedad con un decimal
  lcd.print("%");
  
  // --- SEGUNDA LÍNEA: Hola Mundo ♥ ---
  lcd.setCursor(0, 1);
  lcd.print("Hola Mundo ");
  lcd.write(2);  // Corazón fijo (sin parpadear)
  
  delay(20000);  // Actualizar cada 20 segundos
}
