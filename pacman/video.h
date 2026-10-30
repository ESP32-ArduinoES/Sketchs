/*
  ============================================================================
  GALAGINO — video.h
  ============================================================================
  Declaración de la clase Video, que encapsula el driver de la pantalla TFT.
  La implementación está en video.cpp.

  La clase expone solo dos métodos públicos:
    - begin()  -> inicializa el panel (envía la secuencia de init).
    - write()  -> envía un bloque de píxeles a la pantalla.

  El resto de métodos son privados: forman parte de la "fontanería" interna
  (comandos SPI, ventana de escritura, envío de bytes).
  ============================================================================
*/

// Guarda de inclusión: evita que el header se procese dos veces si varios
// archivos lo incluyen.
#ifndef VIDEO_H
#define VIDEO_H


// Necesario para los tipos spi_device_handle_t y spi_transaction_t que se
// usan como miembros de la clase.
#include <driver/spi_master.h>


class Video {
public:

  // --------------------------------------------------
  // Constructor
  // --------------------------------------------------
  // Configura los pines de control (CS, DC), reserva el búfer DMA,
  // inicializa el bus SPI y registra el dispositivo. NO envía todavía
  // la secuencia de inicialización del panel (eso lo hace begin()).
  Video();


  // --------------------------------------------------
  // begin()
  // --------------------------------------------------
  // Envía la secuencia de comandos de inicialización al panel, limpia la
  // pantalla a negro, define la ventana activa (224×288 centrada) y
  // enciende la retroiluminación si hay pin definido.
  void begin(void);


  // --------------------------------------------------
  // write()
  // --------------------------------------------------
  // Envía un bloque de píxeles (colors, len) a la ventana activa.
  // Usa doble búfer: copia los píxeles al dma_buffer y encola la
  // transacción SPI. No bloquea mientras el DMA transmite.
  //   - colors: puntero al array de píxeles en formato RGB565.
  //   - len:    número de píxeles (no bytes).
  void write(uint16_t *colors, uint32_t len);


private:

  // --------------------------------------------------
  // Métodos internos (no accesibles desde fuera)
  // --------------------------------------------------

  // Define la ventana rectangular donde se escribirán los próximos píxeles.
  // Envía los comandos 0x2A (columnas), 0x2B (filas) y 0x2C (write RAM).
  void setAddrWindow(uint16_t x, uint16_t y, uint16_t w, uint16_t h);

  // Envía un comando al panel con sus datos asociados.
  //   - commandByte:  byte de comando.
  //   - dataBytes:    puntero a los datos (puede ser NULL).
  //   - numDataBytes: número de bytes de datos.
  void sendCommand(uint8_t commandByte, uint8_t *dataBytes, uint8_t numDataBytes);

  // Envía un byte de comando (con DC=LOW).
  void writeCommand(uint8_t cmd);

  // Envío síncrono (bloqueante) de un byte.
  void write8(uint8_t u8);

  // Envío síncrono (bloqueante) de un word de 16 bits (MSB primero).
  void write16(uint16_t u16);


protected:

  // --------------------------------------------------
  // Miembros protegidos
  // --------------------------------------------------
  // Son accesibles desde la propia clase y desde clases derivadas
  // (aunque en este proyecto no hay ninguna). En la práctica, funcionan
  // como privados, pero el autor los dejó así por si se quiere heredar.

  // Handle del dispositivo SPI registrado en el bus. Se rellena en el
  // constructor con spi_bus_add_device(). Es nullptr si algo falló.
  spi_device_handle_t handle;

  // Estructura de transacción SPI reutilizable. Se rellena antes de cada
  // transferencia (longitud, buffer, flags) y se pasa a spi_device_transmit
  // o spi_device_queue_trans.
  spi_transaction_t transaction;

  // Búfer intermedio para el DMA. Los píxeles se copian aquí antes de
  // encolar la transacción, porque el CPU no puede modificar el búfer
  // original mientras el DMA lo está leyendo. Se reserva en el constructor
  // con heap_caps_malloc(..., MALLOC_CAP_DMA).
  unsigned char *dma_buffer;
};


// Cierra la guarda de inclusión.
#endif // VIDEO_H
