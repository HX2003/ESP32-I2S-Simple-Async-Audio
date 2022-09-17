/*
	Author: bitluni 2019
	License: 
	Creative Commons Attribution ShareAlike 4.0
	https://creativecommons.org/licenses/by-sa/4.0/
	
	For further details check out: 
		https://youtube.com/bitlunislab
		https://github.com/bitluni
		http://bitluni.net
	2019 - modified by HX2003
*/
#include <Arduino.h>
#include <FS.h>
#include "SPIFFS.h"
#include "soc/timer_group_struct.h"
#include "driver/periph_ctrl.h"
#include "driver/timer.h"
#include "driver/i2s.h"

#include "Log.h"

typedef enum {
	I2S_INTERNAL_DAC,
	I2S_EXTERNAL_DAC
}i2s_audio_output_type_t;

class AudioOutput;
void IRAM_ATTR timerInterrupt(AudioOutput *audioOutput);

class AudioOutput
{
  private:
    uint8_t i2s_num = 0; // i2s port number
    SemaphoreHandle_t xBinarySemaphore;
    uint8_t BCLK = 26;
    uint8_t LRCK = 25;
    uint8_t DOUT = 27;
	uint8_t I2S_MODE = I2S_EXTERNAL_DAC;
  public:
    AudioSystem *audioSystem;

    void configPin(uint8_t BCLK, uint8_t LRCK, uint8_t DOUT) {
      this->BCLK = BCLK;
      this->LRCK = LRCK;
      this->DOUT = DOUT;
    }
	void configPin() {
      this->I2S_MODE = I2S_INTERNAL_DAC;
    }
    void init(AudioSystem &audioSystem)
    {
      this->audioSystem = &audioSystem;
	  bool use_apll = true;
	  
	  #ifdef CONFIG_IDF_TARGET_ESP32
      esp_chip_info_t out_info;
      esp_chip_info(&out_info);
      if(out_info.revision > 0) {
        use_apll = false;
      }
	  #endif
	  
	  i2s_mode_t mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
	  if(this->I2S_MODE==I2S_INTERNAL_DAC){
		#ifdef CONFIG_IDF_TARGET_ESP32
		mode = (i2s_mode_t)(mode | I2S_MODE_DAC_BUILT_IN);
		#else
		//ERROR, DAC not supported on esp32s2 for now
		#endif
	  }
	  
	  i2s_comm_format_t comm_format;

	  //if(this->I2S_MODE==I2S_INTERNAL_DAC){
	  //	comm_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_STAND_I2S);
	  //}else{
		comm_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_STAND_I2S);
	  //}
	 
      //CONFIGURE I2S
      i2s_config_t i2s_config = {
        .mode = mode,
        .sample_rate = 48000,//must be above ~6000
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT, //I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = comm_format,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1, // high interrupt priority
        .dma_buf_count = audioSystem.bufferCount,  // max buffers
        .dma_buf_len = audioSystem.bufferSize, // max value
        .use_apll  = use_apll
      };
	  //NOTE total memory usage = dma_buf_count*dma_buf_len*sizeof(uint16_t)
      i2s_driver_install((i2s_port_t)i2s_num, &i2s_config, 0, NULL);
		
	  if(this->I2S_MODE==I2S_INTERNAL_DAC){
		#ifdef CONFIG_IDF_TARGET_ESP32
		i2s_set_pin((i2s_port_t)i2s_num, NULL); 
		i2s_set_dac_mode(I2S_DAC_CHANNEL_BOTH_EN);
		#else
		//ERROR, DAC not supported on esp32s2 for now
		#endif
	  }else{
		i2s_pin_config_t pin_config = {
			.bck_io_num = this->BCLK,
			.ws_io_num =  this->LRCK,
			.data_out_num = this->DOUT,
			.data_in_num = I2S_PIN_NO_CHANGE
		};
		i2s_set_pin((i2s_port_t)i2s_num, &pin_config);
	  }
      i2s_zero_dma_buffer((i2s_port_t)i2s_num);
      i2s_set_sample_rates((i2s_port_t)i2s_num, audioSystem.samplingRate);

      DEBUG_PRINTLN("Configured audio i2s"+String(i2s_num)+" @" + String(audioSystem.samplingRate) + "Hz BCLK: " + String(this->BCLK) + ", LRCK: " + String(this->LRCK) + ", DOUT :" + String(this->DOUT));
      
      //CONFIGURE SEMAPHORE
      xBinarySemaphore = xSemaphoreCreateBinary(); /* this task will process the interrupt event which is forwarded by interrupt callback function */
      xTaskCreate(
        this->startISRprocessing,           /* Task function. */
        "ISRprocessing",        /* name of task. */
        audioSystem.bufferSize + 64,                    /* Stack size of task */
        this,                     /* parameter of the task */
        4,                        /* priority of the task */
        NULL);

      DEBUG_PRINTLN("Configured semaphore");

      //CONFIGURE TIMER
      
      timer_config_t config;
      config.alarm_en = TIMER_ALARM_EN;
      config.auto_reload = TIMER_AUTORELOAD_EN;
      config.counter_dir = TIMER_COUNT_UP;
      config.divider = 80;
      config.intr_type = TIMER_INTR_LEVEL;
      config.counter_en = TIMER_PAUSE;
      timer_init((timer_group_t)TIMER_GROUP_0, (timer_idx_t)TIMER_0, &config);
      timer_pause((timer_group_t)TIMER_GROUP_0, (timer_idx_t)TIMER_0);
      timer_set_counter_value((timer_group_t)TIMER_GROUP_0, (timer_idx_t)TIMER_0, 0x00000000ULL);
      float timerinterval = (1.0 / audioSystem.samplingRate) * audioSystem.bufferSize*0.5; // in seconds
      timer_set_alarm_value((timer_group_t)TIMER_GROUP_0, (timer_idx_t)TIMER_0, timerinterval * TIMER_BASE_CLK / config.divider);
      timer_enable_intr((timer_group_t)TIMER_GROUP_0, (timer_idx_t)TIMER_0);
      timer_isr_register((timer_group_t)TIMER_GROUP_0, (timer_idx_t)TIMER_0, (void (*)(void*))timerInterrupt, (void*) this, ESP_INTR_FLAG_IRAM, NULL);
      timer_start((timer_group_t)TIMER_GROUP_0, (timer_idx_t)TIMER_0);

      DEBUG_PRINTLN("Configured timer @"+String(timerinterval)+"s");

    }
    static void startISRprocessing(void* _this) {
      static_cast<AudioOutput*>(_this)->ISRprocessing();
    }
    void ISRprocessing()
    {
      while (1) {
       /* task move to Block state to wait for interrupt event */
       xSemaphoreTake(xBinarySemaphore , portMAX_DELAY);
       unsigned long initial = millis();
       this->audioSystem->calcSamples();
      
       uint8_t* bufz = (uint8_t*)malloc(this->audioSystem->bufferSize*sizeof(uint8_t));
       if(!bufz)
		ERROR("Not enough memory for audio buffer");
	
	   if(this->I2S_MODE==I2S_INTERNAL_DAC){
		for(int i = 0; i < this->audioSystem->bufferSize; i++){
		unsigned char sample = this->audioSystem->nextSample();
		bufz[i] = sample;
		} 
	   }else{
		for(int i = 0; i < this->audioSystem->bufferSize; i++){
		unsigned char sample = this->audioSystem->nextSample();
		bufz[i] = sample - 128;   
		}
	   }
	   /*
        for(int i = 0; i < this->audioSystem->bufferSize; i++){
        size_t m_bytesWritten;
        esp_err_t err = i2s_write_expand((i2s_port_t)i2s_num,(const char*)&bufz[i], sizeof(uint8_t), 8, 16, &m_bytesWritten, portMAX_DELAY);
        esp_err_t err = i2s_write((i2s_port_t)i2s_num, (const char*)&bufz, sizeof(uint8_t), &m_bytesWritten, portMAX_DELAY);
        }
	   */
      size_t m_bytesWritten;
      esp_err_t err = i2s_write_expand((i2s_port_t)i2s_num,(const char*)bufz, this->audioSystem->bufferSize*sizeof(uint8_t), 8, 16, &m_bytesWritten, portMAX_DELAY);
      if(m_bytesWritten < this->audioSystem->bufferSize){
         DEBUG_PRINTLN("Buffer full");
      }
      free(bufz);
      
	  //Convert 8bit left & right audio into 16bit
       // sample16 = ((left) << 8) | (right & 0xff);
      }
      vTaskDelete( NULL );
    }
    void giveSemaphore() {
      static BaseType_t xHigherPriorityTaskWoken;
      xHigherPriorityTaskWoken = pdFALSE;
      xSemaphoreGiveFromISR( xBinarySemaphore , &xHigherPriorityTaskWoken );
    }
};

void IRAM_ATTR timerInterrupt(AudioOutput *audioOutput)
{ 
  #ifdef CONFIG_IDF_TARGET_ESP32
  uint32_t intStatus = TIMERG0.int_st_timers.val;
  if (intStatus & BIT(TIMER_0))
  {
	TIMERG0.hw_timer[TIMER_0].update = 1;
    TIMERG0.int_clr_timers.t0_int_clr = 1;
    TIMERG0.hw_timer[TIMER_0].config.tx_alarm_en = 1;
    audioOutput->giveSemaphore();
  }
  #else
  uint32_t intStatus = TIMERG0.int_st_timers.val;
  if (intStatus & BIT(TIMER_0))
  {
    TIMERG0.hw_timer[TIMER_0].update.val = 1;
    TIMERG0.int_clr_timers.t0_int_clr = 1;
    TIMERG0.hw_timer[TIMER_0].config.tx_alarm_en = 1;
    audioOutput->giveSemaphore();
  }
  #endif
}


