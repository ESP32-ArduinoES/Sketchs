#include "demo_music.h"

char * songs[99] = {};
int song_num = 0;

void listDir(fs::FS &fs, const char * dirname, uint8_t levels)
{
    Serial.printf("Listing directory: %s\n", dirname);

    File root = fs.open(dirname);
    if(!root)
    {
        Serial.println("Failed to open directory");
        return;
    }
    if(!root.isDirectory())
    {
        Serial.println("Not a directory");
        return;
    }

    File file = root.openNextFile();
    while(file)
    {
        if(file.isDirectory())
        {
            Serial.print("  DIR : ");
            Serial.println(file.name());
            if(levels)
            {
                String subdirectory = file.path();
                listDir(fs, subdirectory.c_str(), levels -1);
            }
        } 
        else 
        {
            String filepath = file.path();
            Serial.println(filepath);
            String filename = filepath.substring(filepath.lastIndexOf('/') + 1);
            filename.toLowerCase();
            if (filename.endsWith(".mp3") && !filename.startsWith("._") && song_num < 99)
            {
                songs[song_num] = new char[filepath.length() + 1];
                strcpy(songs[song_num], filepath.c_str());
                Serial.print("Added song: ");
                Serial.println(songs[song_num]);
                song_num++;
            }
        }
        file = root.openNextFile();
    }
}

bool demo_music(void)
{
    song_num = 0;
    listDir(SD_MMC,"/",1);
    if (song_num > 0)
    {
        return true;
    } 
    else
    {
        Serial.println("not find MP3 file");
        return false;
    }
}

const char * demo_music_get_title(uint32_t track_id)
{
    if(track_id >= static_cast<uint32_t>(song_num))
    {
        return NULL;
    }
    return songs[track_id];
}

bool demo_music_play(uint32_t track_id) 
{
    const char *filename = demo_music_get_title(track_id);
    if (!filename) {
        Serial.printf("Invalid song index: %lu\n", static_cast<unsigned long>(track_id));
        return false;
    }

    Serial.printf("Playing: %s\n", filename);
    if (!audio.connecttoFS(SD_MMC, filename)) {
        Serial.println("Could not open MP3 file.");
        return false;
    }
    return true;
}

void demo_music_pause(void)
{
    audio.pauseResume();
}
