#include "demo_music.h"
#include <string.h>
#include <strings.h>

char * title_list[99];

char * songs[99] ;
bool read_flag = false;

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
    int i=0;
    while(file)
    {
        String path = file.path();
        const char *filepath = path.c_str();
        const char *basename = strrchr(filepath, '/');
        basename = basename ? basename + 1 : filepath;

        if (strncmp(basename, "._", 2) != 0 && file.isDirectory())
        {
            Serial.print("  DIR : ");
            Serial.println(file.name());
            if(levels)
            {
                listDir(fs, filepath, levels -1);
            }
        }
        else if (strncmp(basename, "._", 2) != 0)
        {
            Serial.println(filepath);
            size_t len = strlen(filepath);
            if (len >= 4 && strcasecmp(filepath + len - 4, ".mp3") == 0)
            {
                songs[i] = new char[len + 1]; // Add 1 for null terminator
                read_flag = true;
                strcpy(songs[i], filepath);
                Serial.print("out:");
                Serial.println(filepath);
                Serial.print("songs:");
                Serial.println(songs[i]);
                i+=1;
            }
        }
        file = root.openNextFile();
    }
}

bool demo_music(void)
{
    listDir(SD_MMC,"/",1);
    if (read_flag)
    {
        memcpy(title_list,songs,sizeof(songs));
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
    if(track_id >= sizeof(title_list) / sizeof(title_list[0])) 
    {
        return NULL;
    }
    return title_list[track_id];
}

void demo_music_play(uint32_t track_id) 
{
    char chbuf[100];
    const char *filename = demo_music_get_title(track_id);
    Serial.println(String(filename));
    sprintf(chbuf, "%s", filename);
    audio.pauseResume();
    delay(100);
    audio.connecttoFS(SD_MMC, chbuf);
}

void demo_music_pause(void)
{
    audio.pauseResume();
}
