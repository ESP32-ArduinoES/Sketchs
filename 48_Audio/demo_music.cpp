#include "demo_music.h"

namespace {
constexpr size_t MAX_TRACKS = 99;                   // Máximo de pistas que se pueden guardar en la lista de reproducción.

String title_list[MAX_TRACKS];                      // Array donde se almacenarán las rutas completas de los archivos MP3 encontrados.

size_t track_count = 0;                             // Contador de pistas detectadas.

// Devuelve solo el nombre del archivo a partir de la ruta completa.
// Ejemplo: "/music/track01.mp3" -> "track01.mp3"
String file_name(const String &path)
{
    return path.substring(path.lastIndexOf('/') + 1);
}

// Comprueba si la ruta pertenece a un archivo MP3 válido.
// Ignora nombres ocultos como ".hidden.mp3" y archivos sin extensión.
bool is_mp3_file(const String &path)
{
    const String name = file_name(path);
    const int extension_index = name.lastIndexOf('.');
    return !name.startsWith(".") &&
           extension_index >= 0 &&
           name.substring(extension_index).equalsIgnoreCase(".mp3");
}

// Recorre un directorio del sistema de archivos y guarda en title_list
// todas las pistas MP3 que encuentre.
void list_dir(fs::FS &fs, const char *dirname, uint8_t levels)
{
    Serial.printf("Listing directory: %s\n", dirname);

    // Abre el directorio indicado.
    File root = fs.open(dirname);
    if (!root || !root.isDirectory()) {
        Serial.printf("Failed to open directory: %s\n", dirname);
        return;
    }

    // Visita el primer archivo o carpeta dentro del directorio.
    File file = root.openNextFile();
    while (file) {
        const String path = file.path();
        const String name = file_name(path);

        // Ignora archivos ocultos.
        if (!name.startsWith(".")) {
            // Si es una carpeta, se recorre de forma recursiva.
            if (file.isDirectory()) {
                if (levels) {
                    list_dir(fs, path.c_str(), levels - 1);
                }
            }
            // Si es un archivo y además es MP3, se guarda en la lista.
            else if (is_mp3_file(path)) {
                if (track_count < MAX_TRACKS) {
                    title_list[track_count++] = path;
                    Serial.printf("Track %u: %s\n",
                                  static_cast<unsigned>(track_count),
                                  path.c_str());
                } else {
                    Serial.println("Track list full; skipping additional MP3 files");
                }
            }
        }

        // Pasa al siguiente elemento del directorio.
        file = root.openNextFile();
    }
}
}

// Busca y carga todas las pistas MP3 de la tarjeta SD.
// Devuelve true si se encontró al menos una canción.
bool demo_music(void)
{
    track_count = 0;   
    list_dir(SD_MMC, "/", 1);       // Revisa la raíz de la SD con profundidad 1 para detectar archivos MP3.

    if (track_count == 0) {
        Serial.println("No MP3 files found");
        return false;
    }

    return true;
}

// Devuelve la ruta de la pista indicada por su índice.
// Si el número está fuera de rango, devuelve null.
const char *demo_music_get_title(uint32_t track_id)
{
    if (track_id >= track_count) {
        return nullptr;
    }
    return title_list[track_id].c_str();
}

// Reproduce la canción correspondiente al índice indicado.
void demo_music_play(uint32_t track_id)
{
    const char *filename = demo_music_get_title(track_id);
    if (filename == nullptr) {
        Serial.println("Invalid music track index");
        return;
    }

    Serial.printf("Playing: %s\n", filename);

    // Se conecta a la tarjeta SD y reproduce el archivo MP3 elegido.
    audio.connecttoFS(SD_MMC, filename);
}

// Pausa o reanuda la reproducción actual de audio.
void demo_music_pause(void)
{
    audio.pauseResume();
}
