# Funcionamiento del sketch

Este proyecto reproduce archivos MP3 almacenados en una tarjeta SD con un ESP32-S3, usando el códec ES8311 para la salida de audio.

## Archivos principales

- `Sketch_07.1_Music.ino`: configura los pines y coordina el arranque y la reproducción.
- `demo_music.cpp` y `demo_music.h`: buscan archivos MP3 en la SD y ofrecen funciones para seleccionarlos y reproducirlos.
- `es8311.cpp`, `es8311.h` y `es8311_reg.h`: implementan la configuración y el control del códec ES8311.

## Pines

La configuración activa es la rama `#else` del sketch:

| Función | GPIO |
|---|---:|
| SD_MMC SCK / CMD | 38 / 40 |
| SD_MMC D0 / D1 / D2 / D3 | 39 / 41 / 48 / 47 |
| I2S MCLK / BCK / WS | 4 / 5 / 7 |
| I2S entrada / salida de datos | 6 / 8 |
| Habilitación del amplificador | 1 |
| I2C SCL / SDA | 15 / 16 |

Si se define `FNK0104N_3P5_320x480_ST77922`, se utiliza el otro conjunto de pines definido en el bloque `#ifdef`. La macro `FNK0104AB_2P8_240x320_ILI9341` identifica la placa en el archivo, pero no participa en la selección de pines.

## Secuencia de arranque

1. `setup()` inicia el puerto serie a 115200 baudios.
2. Configura los pines de SD_MMC. Si la tarjeta no está disponible, espera y vuelve a intentarlo cada 100 ms.
3. `audio_hardware_init()` configura GPIO 1 como salida y lo pone en LOW, configura I2C como maestro e instala el controlador I2C antiguo. Después configura e inicia I2S.
4. `es8311_codec_init()` inicializa el ES8311 por I2C y configura su reloj, formato y volumen.
5. Se configuran los pines de la biblioteca `Audio` y su volumen.
6. `demo_music()` busca canciones. Si encuentra alguna, `demo_music_play(1)` inicia la canción con índice 1.
7. `loop()` llama continuamente a `audio.loop()`, necesario para que la biblioteca procese y mantenga la reproducción.

Si falla la inicialización de I2C, I2S, el códec o la búsqueda de canciones, `setup()` termina y no inicia la reproducción.

## Búsqueda y reproducción

La búsqueda empieza en `/` y entra un nivel en los directorios. Los archivos con extensión `.mp3` se aceptan sin distinguir mayúsculas y minúsculas. Se ignoran nombres que empiezan por `._`, archivos auxiliares que macOS puede crear en la SD.

Las rutas encontradas se guardan en una lista y se pasan a `audio.connecttoFS()` para reproducirlas desde SD_MMC. Los índices empiezan en cero: `demo_music_play(0)` reproduce la primera canción encontrada y `demo_music_play(1)`, la segunda.

## Notas importantes

- No se debe volver a añadir `Wire.begin()` mientras el códec use `i2c_master_write_to_device()` y las demás funciones I2C antiguas de ESP-IDF: mezclar ambos controladores provoca el conflicto I2C observado.
- El I2S del sketch se inicia a 44100 Hz, mientras que los valores de configuración del ES8311 en `es8311.h` indican 16000 Hz. Como el proyecto está funcionando, esta documentación refleja ambos valores actuales; conviene mantenerlos coordinados si se cambia la frecuencia de audio.
- `demo_music_play()` espera una ruta válida. El índice 1 requiere que se haya encontrado una segunda canción.
- Las listas tienen capacidad para 99 canciones; el código no valida actualmente ese límite ni la longitud máxima de ruta que cabe en su búfer de reproducción.
