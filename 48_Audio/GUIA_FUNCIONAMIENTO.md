# Guia paso a paso: reproductor MP3 con ES8311

Este proyecto reproduce archivos MP3 guardados en una tarjeta microSD. El
ESP32-S3 lee y decodifica el archivo, y envia el audio digital por I2S al
ES8311. El ES8311 convierte la señal digital en audio analogico para la salida
de la placa.

## Esquema general

```text
Tarjeta microSD
     | SD_MMC: SCK, CMD, D0-D3
     v
ESP32-S3 -- decodificacion MP3 --> audio digital I2S -- BCK, WS, DOUT, MCK -->
                                                                      ES8311
                                                                        |
                                                               audio analogico
                                                                        v
                                                         salida de audio/altavoz

ESP32-S3 -- I2C: SDA, SCL --> configuracion del ES8311
```

El esquema es funcional y no sustituye el diagrama electrico de la placa.
Comprueba siempre el pinout y como se conecta el altavoz o amplificador.

## Antes de ejecutar

1. Selecciona la placa `FNK0104AB_2P8_240x320_ILI9341` en el entorno de Arduino
   y conecta la placa al ordenador.
2. Usa una tarjeta microSD compatible con el modo SD_MMC y copia en ella los
   archivos MP3 que quieras escuchar.
3. Coloca los MP3 en la raiz de la tarjeta o en una carpeta situada un nivel
   por debajo de la raiz. Por ejemplo:

   ```text
   /cancion.mp3
   /musica/otra_cancion.mp3
   ```

4. Carga el sketch. Abre el monitor serie a `115200` baudios para ver los
   mensajes de inicializacion y reproduccion.

Los MP3 que aparecen dentro de la carpeta `music` del proyecto son archivos de
ejemplo. El sketch busca los archivos en la tarjeta microSD, no en esa carpeta
del ordenador.

## Ejecucion paso a paso

### 1. Seleccion de placa y pines

Al principio de `Sketch_07.1_MusicCorregido.ino` se define la placa
`FNK0104AB_2P8_240x320_ILI9341`. A continuacion se asignan sus pines para:

- **microSD (SD_MMC):** SCK 38, CMD 40 y D0-D3 en 39, 41, 48 y 47.
- **Audio I2S:** MCK 4, BCK 5, DOUT 8 y WS 7.
- **Control I2C del codec:** SCL 15 y SDA 16, a 400 kHz.
- **Control de la placa:** AP_ENABLE 1.

Estos numeros corresponden al codigo del sketch. Antes de cambiar el cableado,
verifica que coinciden con la revision concreta de tu placa.

### 2. Inicio del monitor serie y de la tarjeta

Al arrancar, `setup()` inicia el puerto serie a `115200` baudios. Luego llama a
`SD_MMC.setPins()` para asignar los pines de la tarjeta y a `SD_MMC.begin()`
para montarla.

Si no se puede asignar el pinout, el programa informa del error y sale de
`setup()`. Si no detecta una tarjeta, muestra un mensaje y vuelve a intentar
montarla cada 100 ms.

### 3. Preparacion del bus I2C

La funcion `driver_es8311_init()` configura `AP_ENABLE` como salida y lo pone
en `LOW`. Despues inicia I2C con los pines SDA y SCL definidos para la placa.
Si `Wire.begin()` falla, informa del error y no continua con la inicializacion
del codec.

### 4. Configuracion del ES8311

`es8311_codec_init()`, implementada en `es8311.cpp`, prepara el codec mediante
I2C. En esta version:

- Usa la direccion I2C `0x18`.
- Configura una frecuencia de muestreo de **44,1 kHz**.
- Configura la entrada y salida serie del codec a **16 bits**.
- Configura el volumen interno del codec usando `EXAMPLE_VOICE_VOLUME` (85).

Si la inicializacion devuelve un error, el sketch lo informa por el monitor
serie y no empieza la reproduccion.

### 5. Preparacion del audio I2S

El sketch llama a `audio.setPinout()` y entrega a la libreria `Audio` los pines
BCK, WS, DOUT y MCK. Estos pines transportan el audio digital desde el ESP32-S3
hacia el ES8311.

Despues llama a `audio.setVolume(10)`. Este es el nivel que configura la
libreria `Audio`; es independiente del volumen interno ajustado al inicializar
el codec.

### 6. Busqueda de archivos MP3

`demo_music()` pone a cero el contador de pistas y llama a `listDir()` para
recorrer la raiz de la microSD y un nivel de subdirectorios. Cada archivo
encontrado se muestra en el monitor serie.

Se agregan a la lista los archivos con extension `.mp3` (sin distinguir
mayusculas y minusculas), excepto los que empiezan por `._`. La lista admite
hasta **99 pistas**. Si no encuentra ninguna, informa del problema y `setup()`
termina sin iniciar la reproduccion.

### 7. Inicio de la primera pista

`demo_music_play(0)` solicita la pista con indice 0, es decir, la primera que
se agrego a la lista durante el recorrido de la tarjeta. `demo_music_get_title()`
comprueba que el indice sea valido y devuelve la ruta del archivo.

Luego `audio.connecttoFS(SD_MMC, filename)` abre ese archivo desde la tarjeta y
prepara la reproduccion. Si el indice no es valido o no se puede abrir el MP3,
el codigo imprime un mensaje de error.

El orden de las pistas depende del orden en que la tarjeta devuelve los
archivos; el indice 0 no implica un orden alfabetico.

### 8. Reproduccion continua

Una vez que `setup()` termina, el microcontrolador ejecuta `loop()` repetidamente.
En cada vuelta se llama a `audio.loop()`, que permite a la libreria `Audio`
seguir leyendo y decodificando el MP3 y enviando las muestras de audio por I2S.
El codec ES8311 recibe esas muestras y produce la salida analogica.

## Para que sirve cada archivo

- **`Sketch_07.1_MusicCorregido.ino`:** seleccion de pines, inicializacion de
  la tarjeta, el bus I2C y el audio; inicia la primera pista y mantiene el
  proceso de reproduccion.
- **`demo_music.cpp`:** recorre la microSD, guarda las rutas MP3 encontradas,
  valida indices e inicia la reproduccion de una pista.
- **`demo_music.h`:** declara las funciones de reproduccion que usa el sketch.
- **`es8311.cpp` y `es8311.h`:** implementan y declaran la configuracion y el
  control del codec ES8311.
- **`es8311_reg.h`:** contiene los nombres y direcciones de los registros del
  codec.
- **`music/`:** contiene MP3 de ejemplo en el proyecto; para que el sketch los
  reproduzca, hay que copiarlos a la microSD.

## Resumen para explicar en el video

1. La placa arranca y monta la microSD.
2. Se inicia I2C y se configura el ES8311.
3. Se asignan los pines I2S y el volumen de la libreria de audio.
4. Se buscan MP3 en la raiz de la tarjeta y en sus subcarpetas directas.
5. Se abre y reproduce la pista de indice 0.
6. `audio.loop()` mantiene la lectura, decodificacion y envio del sonido.
