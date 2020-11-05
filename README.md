# ESP32 I2S Simple Async Audio
This is a library for playing audio through external/internal DAC on the ESP32. It is based on bitluni's [VGA Project](https://github.com/bitluni/ESP32Lib). Link to [audio sample editor](https://bitluni.net/wp-content/uploads/2018/03/WavetableEditor.html).

# New
- Internal DAC is now supported
# Features
- Low cpu usage using dma transfer

- Play multiple audio samples concurrently

- Change volume of audio samples individually

| Tested on                 | 
| ------------------------- | 
| PCM5102a                  |
| TAS5825M (with I2C setup) | 

# Limitations
× Only 8bit mono samples


× SPIFFS not yet supported

× SD card not yet supported
