/*
  ============================================================================
  GALAGINO — video.cpp
  ============================================================================
  Driver de la pantalla TFT. Implementa la clase Video, que se encarga de:
    - Configurar el bus SPI y el dispositivo SPI (pantalla).
    - Inicializar el panel (ILI9341 o ST7789) con la secuencia de comandos.
    - Enviar bloques de píxeles (224×8 por llamada) usando DMA.
    - Gestionar el doble búfer (frame_buffer → dma_buffer) para no bloquear
      mientras se transmite.
  ============================================================================
*/

#include <Arduino.h>
#include "video.h"
#include "config.h"
#include "esp_err.h"    // esp_err_to_name() para traducir códigos de error


/* ==========================================================================
   CONFIGURACIÓN DEL DISPOSITIVO SPI
   spi_device_interface_config_t describe cómo se comunica el ESP32 con el
   panel TFT: modo SPI, frecuencia, tamaño de cola, etc.
   ========================================================================== */

static spi_device_interface_config_t if_cfg {
  .command_bits = 0,        // Sin fase de comando (usamos DC por GPIO).
  .address_bits = 0,        // Sin fase de dirección.
  .dummy_bits = 0,          // Sin bits dummy.
  .mode = SPI_MODE0,        // CPOL=0, CPHA=0. Modo estándar de estos paneles.
  .duty_cycle_pos = 128,    // 50% de duty cycle (128/256).
  .cs_ena_pretrans = 0,     // Sin delay antes de activar CS.
  .cs_ena_posttrans = 0,    // Sin delay después de desactivar CS.
  .clock_speed_hz = TFT_SPICLK,  // Frecuencia del bus (40 MHz por defecto).
  .input_delay_ns = 0,      // Sin delay de entrada (solo escribimos).
  .spics_io_num = -1,       // CS se controla manualmente por GPIO.
  .flags = SPI_DEVICE_HALFDUPLEX,  // Solo escribimos, no leemos.
  .queue_size = 3,          // Cola de hasta 3 transacciones pendientes.
  .pre_cb = NULL,           // Sin callback antes de la transacción.
  .post_cb = NULL,          // Sin callback después.
};


/* ==========================================================================
   PINES SPI POR DEFECTO
   Si config.h no define TFT_MISO/MOSI/SCLK, se usan los pines por defecto
   del ESP32 (MISO=19, MOSI=23, SCK=18 en la mayoría de placas).
   ========================================================================== */

#if !defined(TFT_MISO) || !defined(TFT_MOSI) || !defined(TFT_SCLK)

#ifndef TFT_MISO
#define TFT_MISO MISO
#endif
#ifndef TFT_MOSI
#define TFT_MOSI MOSI
#endif
#ifndef TFT_SCLK
#define TFT_SCLK SCK
#endif

#endif


/* ==========================================================================
   MACRO DE CONTROL DE ACCESO A MEMORIA DE LA PANTALLA
   TFT_MAC controla la rotación y el orden de los canales de color.
   0x48 = RGB, orientación normal (sin flip).
   ========================================================================== */

#ifndef TFT_MAC
#define TFT_MAC 0x48
#endif


/* ==========================================================================
   CONFIGURACIÓN DEL BUS SPI
   spi_bus_config_t describe los pines físicos y el tamaño máximo de
   transferencia.
   ========================================================================== */

spi_bus_config_t bus_cfg{
    .mosi_io_num = TFT_MOSI,        // Pin MOSI (datos al panel).
    .miso_io_num = TFT_MISO,        // Pin MISO (no usado, pero debe existir).
    .sclk_io_num = TFT_SCLK,        // Pin de reloj.
    .max_transfer_sz = 224 * 8 * 2, // Una fila completa de 8x8 tiles a 16 bpp.
    .flags = SPICOMMON_BUSFLAG_MASTER,  // El ESP32 es maestro del bus.
};


/* ==========================================================================
   MACRO DE AYUDA: W16
   Convierte un valor de 16 bits en dos bytes (MSB primero, LSB después),
   para usarlo en la tabla de comandos de inicialización.
   ========================================================================== */

#define W16(a)    (a>>8), (a&0xff)    // store 16 bit parameter


/* ==========================================================================
   TABLA DE COMANDOS DE INICIALIZACIÓN
   Cada entrada es: {comando, número_de_bytes_de_datos, datos...}
   Un comando 0xFF es un delay (el siguiente byte son los ms).
   Un comando 0x00 termina la tabla.
   ========================================================================== */

static const uint8_t init_cmd[] = {

#ifdef TFT_ILI9341
  // --------------------------------------------------
  // Secuencia de inicialización para ILI9341
  // --------------------------------------------------
  0xEF, 3, 0x03, 0x80, 0x02,           // Vendor specific
  0xCF, 3, 0x00, 0xC1, 0x30,           // Power control B
  0xED, 4, 0x64, 0x03, 0x12, 0x81,     // Power on sequence control
  0xE8, 3, 0x85, 0x00, 0x78,           // Driver timing control A
  0xCB, 5, 0x39, 0x2C, 0x00, 0x34, 0x02, // Power control A
  0xF7, 1, 0x20,                       // Pump ratio control
  0xEA, 2, 0x00, 0x00,                 // Driver timing control B
  0xC0, 1, 0x23,                       // Power control VRH[5:0]
  0xC1, 1, 0x10,                       // Power control SAP[2:0];BT[3:0]
  0xC5, 2, 0x3e, 0x28,                 // VCM control
  0xC7, 1, 0x86,                       // VCM control2

#ifdef TFT_VFLIP
  0x36, 1, TFT_MAC^0xc0,               // Mem access ctrl invertido (180º)
#else
  0x36, 1, TFT_MAC,                    // Mem access ctrl normal
#endif

  0x37, 1, 0x00,                       // Vertical scroll zero
  0x3A, 1, 0x55,                       // Pixel format: 16 bpp
  0xB1, 2, 0x00, 0x18,                 // Framerate control (79 Hz)
  0xB6, 3, 0x08, 0x82, 0x27,           // Display function control
  0xF2, 1, 0x00,                       // 3Gamma function disable
  0x26, 1, 0x01,                       // Gamma curve selected
  0xE0, 15, 0x0F, 0x31, 0x2B, 0x0C, 0x0E, 0x08, // Gamma positiva
    0x4E, 0xF1, 0x37, 0x07, 0x10, 0x03, 0x0E, 0x09, 0x00,
  0xE1, 15, 0x00, 0x0E, 0x14, 0x03, 0x11, 0x07, // Gamma negativa
    0x31, 0xC1, 0x48, 0x08, 0x0F, 0x0C, 0x31, 0x36, 0x0F,

#ifdef TFT_INVERT
  0x21, 0,                             // Inversión de color ON
#endif

  0x11, 0,                             // Salir de sleep
  0xff, 150,                           // Delay 150 ms
  0x29, 0,                             // Display ON
  0xff, 150,                           // Delay 150 ms
  0x00                                 // Fin de la tabla

#else
  // --------------------------------------------------
  // Secuencia de inicialización para ST7789
  // --------------------------------------------------
  0x01, 0,                             // Software reset
  0xff, 150,                           // Delay 150 ms
  0x11, 0,                             // Salir de sleep
  0xff, 10,                            // Delay 10 ms
  0x3a, 1, 0x55,                       // Color mode: 16 bpp
  0xff, 10,                            // Delay 10 ms

#ifdef TFT_VFLIP
  0x36, 1, 0x00,                       // Mem access ctrl: boca abajo, RGB
#else
  0x36, 1, 0xc0,                       // Mem access ctrl: boca arriba, RGB
#endif

  0x2a, 4, W16(0), W16(240),           // Column addr set: 0..240
  0x2b, 4, W16(0), W16(320),           // Row addr set: 0..320
  0x21, 0,                             // Inversión ON
  0xff, 10,                            // Delay 10 ms
  0x13, 0,                             // Normal display ON
  0xff, 10,                            // Delay 10 ms
  0x29, 0,                             // Main screen ON
  0xff, 10,                            // Delay 10 ms
  0x00                                 // Fin de la tabla

#endif
};


/* ==========================================================================
   CONSTRUCTOR DE Video
   Configura los pines de control, reserva el búfer DMA, inicializa el bus
   SPI y registra el dispositivo. No envía la secuencia de init (eso lo hace
   begin()).
   ========================================================================== */

Video::Video() : handle(nullptr), transaction{}, dma_buffer(nullptr) {

  // Configurar CS como salida y ponerlo en alto (deseleccionado).
  pinMode(TFT_CS, OUTPUT);
  digitalWrite(TFT_CS, HIGH);

  // Configurar DC como salida y ponerlo en alto (modo datos).
  pinMode(TFT_DC, OUTPUT);
  digitalWrite(TFT_DC, HIGH);

  // Reservar búfer de DMA para 224×8 píxeles × 2 bytes/píxel.
  // MALLOC_CAP_DMA asegura que la memoria es accesible por el DMA del SPI.
  dma_buffer = (unsigned char*)heap_caps_malloc(224*8*2, MALLOC_CAP_DMA);
  if(!dma_buffer) {
    Serial.println("TFT: failed to allocate DMA buffer");
    return;
  }

  // Inicializar el bus SPI.
  // 40 MHz = 2.5 MPix/s. Un frame tiene 64512 píxeles -> máx 38 fps.
  esp_err_t error = spi_bus_initialize(TFT_SPI_HOST, &bus_cfg, SPI_DMA_CH_AUTO);
  if(error != ESP_OK) {
    Serial.printf("TFT: SPI bus init failed on host %d: %s\n",
                  TFT_SPI_HOST, esp_err_to_name(error));
    return;
  }

  // Registrar el dispositivo SPI (la pantalla) en el bus.
  error = spi_bus_add_device(TFT_SPI_HOST, &if_cfg, &handle);
  if(error != ESP_OK) {
    Serial.printf("TFT: SPI device registration failed on host %d: %s\n",
                  TFT_SPI_HOST, esp_err_to_name(error));
    spi_bus_free(TFT_SPI_HOST);
    return;
  }

  // Reset hardware del panel.
  if (TFT_RST >= 0) {
    // Si hay pin de reset físico, usarlo.
    pinMode(TFT_RST, OUTPUT);
    digitalWrite(TFT_RST, LOW);
    delay(100);
    digitalWrite(TFT_RST, HIGH);
    delay(200);
  } else {
    // Si no hay pin de reset, usar el comando de software reset (0x01).
    sendCommand(0x01, NULL, 0);
    delay(150);
  }
}


/* ==========================================================================
   begin(): envía la secuencia de inicialización del panel
   ========================================================================== */

void Video::begin(void) {
  if(!handle)
    return;  // Si el constructor falló, no hacer nada.

  uint8_t cmd;
  const uint8_t *addr = init_cmd;

  // Recorrer la tabla de comandos hasta encontrar el terminador (0x00).
  while(cmd = *addr++) {
    const uint8_t num = *addr++;  // Número de bytes de datos.

    if(cmd != 0xff) {
      // Comando normal: enviar comando + datos.
      sendCommand(cmd, (uint8_t*)addr, num);
      addr += num;  // Avanzar por los datos.
    } else {
      // Comando 0xFF = delay de "num" milisegundos.
      delay(num);
    }
  }

  // Limpiar la pantalla a negro (240×320 píxeles = 16 bpp).
  digitalWrite(TFT_CS, LOW);
  setAddrWindow(0, 0, 240, 320);

  // Rellenar el búfer DMA de ceros (negro).
  // 240*4*2 = 1920 bytes = 4 líneas de 240 píxeles × 2 bytes.
  memset(dma_buffer, 0, 240*4*2);

  // Enviar el búfer 320/4 = 80 veces para cubrir las 320 líneas.
  for(int i = 0; i < 320/4; i++) {
    transaction.flags = 0;
    transaction.length = 240*4*16;  // Longitud en BITS.
    transaction.tx_buffer = (const void *)dma_buffer;
    esp_err_t error = spi_device_transmit(handle, &transaction);
    if(error != ESP_OK) {
      Serial.printf("TFT: initial screen clear failed: %s\n", esp_err_to_name(error));
      digitalWrite(TFT_CS, HIGH);
      return;
    }
  }

  // Definir la ventana activa como 224×288 centrada en la pantalla.
  setAddrWindow(TFT_X_OFFSET, TFT_Y_OFFSET, 224, 288);

  // Encender la retroiluminación si hay pin definido.
#ifdef TFT_BL
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
#endif
}


/* ==========================================================================
   write(): envía un bloque de píxeles a la pantalla
   Usa doble búfer:
     - frame_buffer del llamante
     - dma_buffer propio, que es el que se entrega al DMA
   Mientras el DMA transmite un bloque, se puede seguir renderizando el
   siguiente en frame_buffer. Antes de copiar al dma_buffer, se espera a
   que la transacción anterior termine.
   ========================================================================== */

void Video::write(uint16_t *colors, uint32_t len) {
  if(!handle || !colors || len == 0)
    return;

  static char dma_active = 0;  // Bandera: hay una transacción en curso.

  transaction.flags = 0;
  transaction.length = 16 * len;  // Longitud en BITS.
  transaction.tx_buffer = dma_buffer;

  // Si había una transacción anterior pendiente, esperar a que termine.
  if(dma_active) {
    spi_transaction_t* r_trans;
    esp_err_t e = spi_device_get_trans_result(handle, &r_trans, portMAX_DELAY);
    if(e != ESP_OK) {
      Serial.printf("TFT: SPI result retrieval failed: %s\n", esp_err_to_name(e));
      return;
    }
  }

  // Copiar los píxeles al búfer de DMA (el DMA no puede leer del frame_buffer
  // porque el CPU lo estaría modificando al mismo tiempo).
  memcpy(dma_buffer, colors, 2*len);

  // Encolar la transacción para que el DMA la envíe en segundo plano.
  esp_err_t e = spi_device_queue_trans(handle, &transaction, portMAX_DELAY);
  if(e != ESP_OK) {
    Serial.printf("TFT: SPI transaction queue failed: %s\n", esp_err_to_name(e));
    return;
  }

  dma_active = 1;
}


/* ==========================================================================
   sendCommand(): envía un comando con sus datos
   ========================================================================== */

void Video::sendCommand(uint8_t commandByte, uint8_t *dataBytes, uint8_t numDataBytes) {
  digitalWrite(TFT_CS, LOW);     // Seleccionar la pantalla.
  writeCommand(commandByte);     // Enviar comando (DC=LOW).
  for(int i = 0; i < numDataBytes; i++)
    write8(dataBytes[i]);        // Enviar datos (DC=HIGH).
  digitalWrite(TFT_CS, HIGH);    // Deseleccionar.
}


/* ==========================================================================
   writeCommand(): envía un byte de comando (DC=LOW)
   ========================================================================== */

void Video::writeCommand(uint8_t cmd) {
  digitalWrite(TFT_DC, LOW);     // Modo comando.
  write8(cmd);
  digitalWrite(TFT_DC, HIGH);    // Volver a modo datos.
}


/* ==========================================================================
   setAddrWindow(): define la ventana activa para escritura
   El panel tiene un "puntero" interno que avanza solo: cada píxel que se
   escribe se coloca en la posición actual, y al llegar al final de una fila,
   salta a la siguiente. Con setAddrWindow le decimos dónde empieza y termina
   la zona en la que queremos escribir.
   ========================================================================== */

void Video::setAddrWindow(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {

  // Comando 0x2A: column address set (X).
  writeCommand(0x2A);
  write16(x);             // X inicial.
  write16(x + w - 1);     // X final.

  // Comando 0x2B: row address set (Y).
  writeCommand(0x2B);
  write16(y);             // Y inicial.
  write16(y + h - 1);     // Y final.

  // Comando 0x2C: write to RAM (empezar a escribir píxeles).
  writeCommand(0x2C);
}


/* ==========================================================================
   write16(): envía un valor de 16 bits (MSB primero)
   ========================================================================== */

void Video::write16(uint16_t data) {
  transaction.flags = SPI_TRANS_USE_TXDATA;  // Usar tx_data en lugar de tx_buffer.
  transaction.length = 16;                    // Longitud en BITS.
  transaction.rxlength = 0;                   // Sin recepción.
  transaction.tx_data[0] = ((data >> 8) & 0xFF);  // Byte alto.
  transaction.tx_data[1] = data & 0xFF;           // Byte bajo.

  spi_device_transmit(handle, &transaction);  // Envío síncrono (bloquea).
}


/* ==========================================================================
   write8(): envía un byte
   ========================================================================== */

void Video::write8(uint8_t data) {
  transaction.flags = SPI_TRANS_USE_TXDATA;
  transaction.length = 8;                     // Longitud en BITS.
  transaction.rxlength = 0;
  transaction.tx_data[0] = data;

  spi_device_transmit(handle, &transaction);  // Envío síncrono (bloquea).
}
