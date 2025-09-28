// Simple WiFi network scanner application
// Released to the public domain in 2022 by Earle F. Philhower, III
#include <WiFi.h>
#include <PWMAudio.h>
#include "audio.h"


#define BEEP_AUDIO_OFFSET     32000

//Modes
#define RANDOM_MODE   0
#define WIFI_MODE     1

//Pins
#define AUDIO_PIN     1
//DAC Current pins
#define IPIN_1        2
#define IPIN_2        3
#define IPIN_3        4
#define IPIN_4        5
#define IPIN_5        6
#define IPIN_6        7

#define LED_PIN       8


// Create the PWM audio device on GPIO 1.   Hook amp/speaker between GPIO1 and convenient GND.
PWMAudio pwm(AUDIO_PIN);

uint32_t timer, timerSlice;
uint16_t adc0, adc1;
uint8_t mode = WIFI_MODE;   //0 = random, 1 = WiFi
int windowValues;



// The sample pointers
int16_t AUDIO_RAM_BUFFER[44100];
uint32_t ramWRCount = 0;
uint32_t ramRDCount = 22050; //apply offset
uint32_t sndCount_beep = 0;
uint32_t sndCount_signal = 0;

const uint32_t sndSize_beep = sizeof(beep)/sizeof(int16_t);
const uint32_t sndSize_signal = sizeof(signal)/sizeof(int16_t);







int levelMeasured[] = {-90, -90, -90, -90, -90, -90, -90, -90, -90, -90}; // Wifi level RSSI (-90dBm = min)
int level = -90; //filtered level
uint8_t currentOutput = 0, slicedOutput = 0;
bool AudioSignalChannelPlay = false;


//Prorotypes
void Sampler(void);
void WriteIDAC(uint8_t val);


void setup() {
  Serial.begin(115200);
  pwm.setBitsPerSample(8);
  pinMode(IPIN_1, OUTPUT);
  pinMode(IPIN_2, OUTPUT);
  pinMode(IPIN_3, OUTPUT);
  pinMode(IPIN_4, OUTPUT);
  pinMode(IPIN_5, OUTPUT);
  pinMode(IPIN_6, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  //pwm.onTransmit(Sampler);
  //pwm.begin(44100);

}

void loop() {

  if(millis() - timer >= 1000)
  {

    adc0 = analogRead(26); //read for mode
    adc1 = analogRead(27); //read for "filter" medium method

    digitalWrite(LED_PIN, HIGH);
    delay(100);
    digitalWrite(LED_PIN, LOW);
    delay(100);

    windowValues = map(adc1, 0, 4095, 1, 10);

    if(adc0 < 400 || adc0 > 3695)
    {
      mode = RANDOM_MODE;
      mode += (adc0 > 3695); //get WIFI_MODE if true
    }

    if(mode == WIFI_MODE)
    {
        auto cnt = WiFi.scanNetworks();
        if (cnt) 
        {
          for(int i = 9; i > 0; i--) //swip arrray to the right
          {
              levelMeasured[i] = levelMeasured[i-1];
          }
          levelMeasured[0] = -90; //force minimal level
          for (auto i = 0; i < cnt; i++) 
          {
            uint8_t bssid[6];
            WiFi.BSSID(i, bssid);
            //Serial.println(WiFi.RSSI(i));
            if(levelMeasured[0] < WiFi.RSSI(i))
            {
              levelMeasured[0] = WiFi.RSSI(i);
            }
            //Serial.printf("%32s %5s %17s %2d %4ld\n", WiFi.SSID(i), encToString(WiFi.encryptionType(i)), macToString(bssid), WiFi.channel(i), WiFi.RSSI(i));
          }


          level = 0; //reset value
          for(int i = 0; i < windowValues; i++)
          {
              level += levelMeasured[i]; 
          }
          level /= windowValues;

        }
    }
    else //random mode
    {
        level = random(-90, -20);
    }
    
    currentOutput = map(level, -90, -20, 1, 32);
    
    

    timer = millis();
  }


  if(millis() - timerSlice >= 50)
  {
    slicedOutput = (slicedOutput < currentOutput) ? ++slicedOutput : --slicedOutput;
    slicedOutput = (slicedOutput > 250) ? 0 : slicedOutput;
    //Serial.println(slicedOutput);
    WriteIDAC(slicedOutput);
    timerSlice = millis();
  }

  if(level > -40) //strong enough signal
  {
      AudioSignalChannelPlay = true; //play it
  }


}







/////////////////////////////////////////////////////////////
//Functions



//Callback function for audio
void Sampler(void) 
{
  while (pwm.availableForWrite()) {
    AUDIO_RAM_BUFFER[ramWRCount] = ((sndCount_beep < sndSize_beep) ? beep[sndCount_beep] : 0) + signal[sndCount_signal]; //let's put sound mixing (with beep playing every ~2 seconds)
    pwm.write(AUDIO_RAM_BUFFER[ramWRCount]); //play raw buffer
    sndCount_beep++;
    ramWRCount++;
    ramRDCount++;
    if (sndCount_beep >= sndSize_beep + BEEP_AUDIO_OFFSET) {
      sndCount_beep = 0;
    }


    //specific channel playing (signal)
    if(AudioSignalChannelPlay)
    {
      sndCount_signal++;
      if (sndCount_signal >= sndSize_signal) {
        sndCount_signal = 0;
        AudioSignalChannelPlay = false; //stop playing
      }
    }
    

    if (ramWRCount >= 44100) {
      ramWRCount = 0;
    }
    if (ramRDCount >= 44100) {
      ramRDCount = 0;
    }
  }
}




//DAC Current write
void WriteIDAC(uint8_t val)
{
  digitalWrite(IPIN_6, (val & 1) > 0);
  digitalWrite(IPIN_5, (val & 2) > 0);
  digitalWrite(IPIN_4, (val & 4) > 0);
  digitalWrite(IPIN_3, (val & 8) > 0);
  digitalWrite(IPIN_2, (val & 16) > 0);
  digitalWrite(IPIN_1, (val & 32) > 0);
}
