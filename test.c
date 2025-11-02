#pragma mark - Depend ESP8266Audio and ESP8266_Spiram libraries
/*
cd ~/Arduino/libraries
git clone https://github.com/earlephilhower/ESP8266Audio
git clone https://github.com/Gianbacchio/ESP8266_Spiram
*/


#include "config.h"

#include "AudioFileSourcePROGMEM.h"
#include "AudioFileSourceID3.h"
#include "AudioGeneratorMP3.h"
#include "AudioOutputI2S.h"

#include "pika.h"

TTGOClass *ttgo;
AudioGeneratorMP3 *mp3;
AudioFileSourcePROGMEM *file;
AudioOutputI2S *out;
AudioFileSourceID3 *id3;

void setup()
{

    Serial.begin(115200);
    ttgo = TTGOClass::getWatch();
    ttgo->begin();
#ifdef LILYGO_WATCH_HAS_DISPLAY
    ttgo->openBL();
    ttgo->tft->fillScreen(TFT_BLACK);
    ttgo->tft->setTextFont(2);
    ttgo->tft->setCursor(0, 0);
    ttgo->tft->fillScreen(TFT_BLACK);
    ttgo->tft->println("Sample MP3 playback begins");
#endif

    //!Turn on the audio power
    ttgo->enableLDO3();

    file = new AudioFileSourcePROGMEM(pika, sizeof(pika));
    id3 = new AudioFileSourceID3(file);

#if defined(STANDARD_BACKPLANE)
    out = new AudioOutputI2S(0, 1);
#elif defined(EXTERNAL_DAC_BACKPLANE)
    out = new AudioOutputI2S();
    //External DAC decoding
    out->SetPinout(TWATCH_DAC_IIS_BCK, TWATCH_DAC_IIS_WS, TWATCH_DAC_IIS_DOUT);
#endif
    mp3 = new AudioGeneratorMP3();
    mp3->begin(id3, out);
}

void loop()
{
    if (mp3->isRunning()) {
        if (!mp3->loop()) mp3->stop();
        Serial.println("test");
    } else {
        Serial.printf("MP3 done\n");
        delay(1000);
    }
}
