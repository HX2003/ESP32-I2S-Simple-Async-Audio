//HX2003
#include <ESP32I2SAudio.h>
#include "sfx/soundsamples.h"
//Audio samples by Kenney.nl & Ove Melaa (Omsofware@hotmail.com) -2013 Ove Melaa
////////////////////////////
//audio configuration
const int SAMPLING_RATE = 44100; //Minimum tested 6000
const int BUFFER_COUNT = 8;
const int SAMPLE_BUFFER_SIZE = 1024;  //Maximum number of samples for i2s engine 1024, each sample is 2bytes
const int BCLK = 26;
const int LRCK = 25;
const int DOUT = 12;
AudioSystem audioSystem(SAMPLING_RATE, SAMPLE_BUFFER_SIZE, BUFFER_COUNT);
AudioOutput audioOutput;
//0<amplitude<=1
//soundsamples.play(AudioSystem &AudioSystem, int soundNumber, float amplitude = 1, float speed = 1, bool loop = false) //Plays a sound
//soundsamples.stop(AudioSystem &AudioSystem, int soundNumber) //Stops a sound with specified soundNumber
//soundsamples.stop(AudioSystem &AudioSystem )//Stops all sounds
//soundsamples.volume(AudioSystem &AudioSystem, int soundNumber, float amplitude) //Set volume of a sound with specified soundNumber
//soundsamples.volume(AudioSystem &AudioSystem, float amplitude) //Set indivual volumes of all sounds
void setup()
{
  Serial.begin(115200);
  //initialize audio output
  audioOutput.configPin(BCLK, LRCK, DOUT);
  audioOutput.init(audioSystem);

  Serial.println("|****************************|");
  Serial.println("|**|    Setup complete    |**|");
  Serial.println("|**|    Press keys 0-11   |**|");
  Serial.println("|****************************|");
}

void loop()
{
  while (Serial.available() == 0);
  int number = Serial.parseInt();
  String description = "";

  Serial.println(description);
  Serial.println("Keyed: " + String(number));
  switch (number) {
    case 0: soundsamples.play(audioSystem, 0, 1, 1, false); description = "laser1"; break;
    case 1: soundsamples.play(audioSystem, 1, 1, 1, false); description = "zapThreeToneDown"; break;
    case 2: soundsamples.play(audioSystem, 2, 1, 1, false); description = "highDown"; break;
    case 3: soundsamples.play(audioSystem, 3, 1, 1, false); description = "Ove Melaa - Heaven Sings (trimmed)"; break;
    case 4: soundsamples.play(audioSystem, 0, 1, 1, true); description = "laser1 [loop]"; break;
    case 5: soundsamples.play(audioSystem, 1, 1, 1, true); description = "zapThreeToneDown [loop]"; break;
    case 6: soundsamples.play(audioSystem, 2, 1, 1, true); description = "highDown [loop]"; break;
    case 7: soundsamples.play(audioSystem, 3, 1, 1, true); description = "Ove Melaa - Heaven Sings (trimmed) [loop]"; break;
    case 8: soundsamples.stop(audioSystem, 0); description = "laser1 [stop]"; break;
    case 9: soundsamples.stop(audioSystem, 1); description = "zapThreeToneDown [stop]"; break;
    case 10: soundsamples.stop(audioSystem, 2); description = "highDown [stop]"; break;
    case 11: soundsamples.stop(audioSystem, 3); description = "Ove Melaa - Heaven Sings (trimmed) [stop]"; break;
    default: break;
  }
  Serial.println(description);
}


