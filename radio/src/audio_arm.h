/*
 * Copyright (C) OpenTX
 *
 * Based on code named
 *   th9x - http://code.google.com/p/th9x
 *   er9x - http://code.google.com/p/er9x
 *   gruvin9x - http://code.google.com/p/gruvin9x
 *
 * License GPLv2: http://www.gnu.org/licenses/gpl-2.0.html
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _AUDIO_ARM_H_
#define _AUDIO_ARM_H_

#include <stddef.h>
#include "ff.h"

/*
  Implements a bit field, number of bits is set by the template,
  each bit can be modified and read by the provided methods.
*/
template <unsigned int NUM_BITS> class BitField {
  private:
    uint8_t bits[(NUM_BITS+7)/8];
  public:
    BitField()
    {
      reset();
    }

    void reset()
    {
      memset(bits, 0, sizeof(bits));
    }

    void setBit(unsigned int bitNo)
    {
      if (bitNo >= NUM_BITS) return;
      bits[bitNo >> 3] = bits[bitNo >> 3] | (1 << (bitNo & 0x07));
    }

    bool getBit(unsigned int bitNo) const
    {
      // assert(bitNo < NUM_BITS);
      if (bitNo >= NUM_BITS) return false;
      return bits[bitNo >> 3] & (1 << (bitNo & 0x07));
    }

    void getSize() const
    {
      return NUM_BITS;
    }
};

#define INDEX_LOGICAL_SWITCH_AUDIO_FILE(index, event) (2*(index)+(event))
#define INDEX_PHASE_AUDIO_FILE(index, event)          (2*(index)+(event))


#define AUDIO_FILENAME_MAXLEN          (42) // max length (example: /SOUNDS/fr/123456789012/1234567890-off.wav)
#define AUDIO_QUEUE_LENGTH             (20)

#define AUDIO_SAMPLE_RATE              (32000)
#define AUDIO_BUFFER_DURATION          (10)
#define AUDIO_BUFFER_SIZE              (AUDIO_SAMPLE_RATE*AUDIO_BUFFER_DURATION/1000)

#if defined(SIMU) && defined(SIMU_AUDIO)
  #define AUDIO_BUFFER_COUNT           (10) // simulator needs more buffers for smooth audio
#elif defined(PCBHORUS)
  #define AUDIO_BUFFER_COUNT           (2)
#else
  #define AUDIO_BUFFER_COUNT           (3)
#endif

#define BEEP_MIN_FREQ                  (150)
#define BEEP_MAX_FREQ                  (15000)
#define BEEP_DEFAULT_FREQ              (2250)
#define BEEP_KEY_UP_FREQ               (BEEP_DEFAULT_FREQ+150)
#define BEEP_KEY_DOWN_FREQ             (BEEP_DEFAULT_FREQ-150)

enum AudioBufferState
{
  AUDIO_BUFFER_FREE,
  AUDIO_BUFFER_FILLED,
  AUDIO_BUFFER_PLAYING
};

#if defined(SIMU)
  typedef uint16_t audio_data_t;
  #define AUDIO_DATA_SILENCE           0x8000
  #define AUDIO_DATA_MIN               0
  #define AUDIO_DATA_MAX               0xffff
  #define AUDIO_BITS_PER_SAMPLE        16
#elif defined(PCBHORUS)
  typedef int16_t audio_data_t;
  #define AUDIO_DATA_SILENCE           0
  #define AUDIO_DATA_MIN               INT16_MIN
  #define AUDIO_DATA_MAX               INT16_MAX
  #define AUDIO_BITS_PER_SAMPLE        16
#else
  typedef uint16_t audio_data_t;
  #define AUDIO_DATA_SILENCE           (0x8000 >> 4)
  #define AUDIO_DATA_MIN               0
  #define AUDIO_DATA_MAX               0x0fff
  #define AUDIO_BITS_PER_SAMPLE        12
#endif

struct AudioBuffer {
  audio_data_t data[AUDIO_BUFFER_SIZE];
  uint16_t size;
  // volatile AudioBufferState state;
};

extern AudioBuffer audioBuffers[AUDIO_BUFFER_COUNT];

enum FragmentTypes {
  FRAGMENT_EMPTY,
  FRAGMENT_TONE,
  FRAGMENT_FILE,
};

struct AudioFragment {
  uint8_t type;
  uint8_t id;
  uint8_t repeat;
  union {
    struct {
      uint16_t freq;
      uint16_t duration;
      uint16_t pause;
      int8_t   freqIncr;
      uint8_t  reset;
    } tone;

    char file[AUDIO_FILENAME_MAXLEN+1];
  };

  void clear()
  {
    memset(this, 0, sizeof(AudioFragment));
  }


  AudioFragment()
  {
    clear();
  }

  AudioFragment(uint16_t freq, uint16_t duration, uint16_t pause, uint8_t repeat, int8_t freqIncr, bool reset, uint8_t id=0)
  {
    type = FRAGMENT_TONE;
    tone.freq = freq;
    tone.duration = duration;
    tone.pause = pause;
    tone.freqIncr = freqIncr;
    tone.reset = reset;
    this->repeat = repeat;
    this->id = id;
  }

  AudioFragment(const char * filename, uint8_t repeat, uint8_t id=0)
  {
    type = FRAGMENT_FILE;
    strcpy(file, filename);
    this->repeat = repeat;
    this->id = id;
  }
};

class ToneContext {
  public:
    AudioFragment fragment;

    struct {
      double step;
      double idx;
      float  volume;
      uint16_t freq;
      uint16_t duration;
      uint16_t pause;
    } state;

    // inline void setFragment(AudioFragment & fragment)
    // {
    //   this->fragment = fragment;
    //   memset(&state, 0, sizeof(state));
    // }

    inline void clear()
    {
      memset(this, 0, sizeof(ToneContext));
    }

    int mixBuffer(AudioBuffer *buffer, int volume, unsigned int fade);


    bool free() const
    {
      return fragment.type == FRAGMENT_EMPTY;
    }

    void setFragment(uint16_t freq, uint16_t duration, uint16_t pause, uint8_t repeat, int8_t freqIncr, bool reset, uint8_t id=0)
    {
      fragment.type = FRAGMENT_TONE;
      fragment.tone.freq = freq;
      fragment.tone.duration = duration;
      fragment.tone.pause = pause;
      fragment.repeat = repeat;
      fragment.tone.freqIncr = freqIncr;
      fragment.tone.reset = reset;
    }

};

class WavContext {
  public:
    AudioFragment fragment;

    struct {
      FIL      file;
      uint8_t  codec;
      uint32_t freq;
      uint32_t size;
      uint8_t  resampleRatio;
      uint16_t readSize;
    } state;

    inline void clear()
    {
      fragment.clear();
    }

    int mixBuffer(AudioBuffer *buffer, int volume, unsigned int fade);
};

class MixedContext {
  public:
    union {
      AudioFragment fragment;
      ToneContext tone;
      WavContext wav;
    };

    MixedContext()
    {
      clear();
    }

    int mixBuffer(AudioBuffer *buffer, int volume, unsigned int fade);
    void setFragment(const AudioFragment * frag)
    {
      if (frag) {
        fragment = *frag;
      }
    }

    inline void clear()
    {
      tone.clear();   // the biggest member of the uninon
      // wav.clear();
    }

};

// void audioPushBuffer(AudioBuffer * buffer);

class AudioBufferFifo {
  // friend void audioTask(void* pdata);
#if defined(CLI)
  friend void printAudioVars();
#endif

  private:
    volatile uint8_t readIdx;
    volatile uint8_t writeIdx;

    // readIdx == writeIdx       -> buffer empty
    // readIdx == writeIdx + 1   -> buffer full

    inline uint8_t nextBufferIdx(uint8_t idx) const
    {
      return (idx >= AUDIO_BUFFER_COUNT-1 ? 0 : idx+1);
    }
    bool full() const
    {
      return readIdx == nextBufferIdx(writeIdx);
    }
    bool empty() const
    {
      return readIdx == writeIdx;
    }
    uint8_t used() const
    {
      return writeIdx - readIdx;
    }

  public:
    AudioBufferFifo() : readIdx(0), writeIdx(0)
    {
      memset(audioBuffers, 0, sizeof(audioBuffers));
    }

    // returns an empty buffer to be filled wit data and put back into FIFO with audioPushBuffer()
    AudioBuffer * getEmptyBuffer() const
    {
      return full() ? 0 : &audioBuffers[writeIdx];
    }

    // puts filled buffer into FIFO
    void audioPushBuffer()
    {
      // AudioBuffer * buffer = &audioBuffers[writeIdx];
      writeIdx = nextBufferIdx(writeIdx);
      // buffer->state = AUDIO_BUFFER_FILLED;
    }

    // returns a pointer to the audio buffer to be played
    const AudioBuffer * getNextFilledBuffer() const
    {
      return empty() ? 0 : &audioBuffers[readIdx];
    }

    // frees the last played buffer
    void freeNextFilledBuffer()
    {
      readIdx = nextBufferIdx(readIdx);
    }

    bool filledAtleast(int noBuffers) const
    {
      return used() >= noBuffers;
    }

};

class AudioFragmentFifo
{
#if defined(CLI)
  friend void printAudioVars();
#endif
  private:
    volatile uint8_t ridx;
    volatile uint8_t widx;
    AudioFragment fragments[AUDIO_QUEUE_LENGTH];

    uint8_t nextIdx(uint8_t idx) const
    {
      return (idx + 1) % AUDIO_QUEUE_LENGTH;
    }

  public:
    AudioFragmentFifo() : ridx(0), widx(0), fragments() {};

    bool findFragment(uint8_t id)
    {
      uint8_t i = ridx;
      while (i != widx) {
        AudioFragment & fragment = fragments[i];
        if (fragment.id == id) return true;
        i = (i + 1) % AUDIO_QUEUE_LENGTH;
      }
      return false;
    }

    bool empty() const
    {
      return ridx == widx;
    }

    bool full() const
    {
      return ridx == nextIdx(widx);
    }

    void clear()
    {
      widx = ridx;                      // clean the queue
    }

    const AudioFragment * get()
    {
      if (!empty()) {
        const AudioFragment * result = &fragments[ridx];
        if (!fragments[ridx].repeat--) {
          // repeat is done, move to the next fragment
          ridx = nextIdx(ridx);
          // return 0;
        }
        return result;
      }
      return 0;
    }

    void push(const AudioFragment & fragment)
    {
      if (!full()) {
        fragments[widx] = fragment;
        widx = nextIdx(widx);
      }
    }

};

class AudioQueue {

  friend void audioTask(void* pdata);
#if defined(SIMU_AUDIO)
  friend void * audioThread(void *);
  friend void fillAudioBuffer(void *, uint8_t *, int);
#endif
#if defined(CLI)
  friend void printAudioVars();
#endif
  public:

    AudioQueue();

    void start()
    {
      _started = true;
    }

    void playTone(uint16_t freq, uint16_t len, uint16_t pause=0, uint8_t flags=0, int8_t freqIncr=0);

    void playFile(const char *filename, uint8_t flags=0, uint8_t id=0);

    void stopPlay(uint8_t id);

    void stopAll();

    void flush();

    void pause(uint16_t tLen);

    void stopSD();

    bool isPlaying(uint8_t id);

    bool started() const
    {
      return _started;
    }

    bool empty() const
    {
      return fragmentsFifo.empty();
    }

    AudioBufferFifo buffersFifo;

  protected:

    void wakeup();
    void fillBuffers();

    volatile bool _started;


    MixedContext normalContext;
    WavContext   backgroundContext;
    ToneContext  priorityContext;
    ToneContext  varioContext;

    AudioFragmentFifo fragmentsFifo;

};

extern AudioQueue audioQueue;

enum {
  ID_PLAY_FROM_SD_MANAGER = 254,
  ID_PLAY_BYE = 255
};

void codecsInit();
void audioEvent(unsigned int index);
void audioPlay(unsigned int index, uint8_t id=0);
void audioStart();

#if defined(AUDIO) && defined(BUZZER)
  #define AUDIO_BUZZER(a, b)  do { a; b; } while(0)
#elif defined(AUDIO)
  #define AUDIO_BUZZER(a, b)  a
#else
  #define AUDIO_BUZZER(a, b)  b
#endif

#if defined(VOICE)
  #define AUDIO_ERROR_MESSAGE(e) audioEvent(e)
  #define AUDIO_TIMER_MINUTE(t)  playDuration(t, 0, 0)
#else
  #define AUDIO_ERROR_MESSAGE(e) audioEvent(AU_ERROR)
  #define AUDIO_TIMER_MINUTE(t)  audioDefevent(AU_WARNING1)
#endif

void audioKeyPress();
void audioKeyError();
void audioTrimPress(int value);
void audioTimerCountdown(uint8_t timer, int value);

#define AUDIO_KEY_PRESS()        audioKeyPress()
#define AUDIO_KEY_ERROR()        audioKeyError()

#define AUDIO_HELLO()            audioPlay(AUDIO_HELLO)
#define AUDIO_BYE()              audioPlay(AU_BYE, ID_PLAY_BYE)
#define AUDIO_WARNING1()         AUDIO_BUZZER(audioEvent(AU_WARNING1), beep(3))
#define AUDIO_WARNING2()         AUDIO_BUZZER(audioEvent(AU_WARNING2), beep(2))
#define AUDIO_TX_BATTERY_LOW()   AUDIO_BUZZER(audioEvent(AU_TX_BATTERY_LOW), beep(4))
#if defined(PCBSKY9X)
#define AUDIO_TX_MAH_HIGH()      audioEvent(AU_TX_MAH_HIGH)
#define AUDIO_TX_TEMP_HIGH()     audioEvent(AU_TX_TEMP_HIGH)
#endif
#define AUDIO_ERROR()            AUDIO_BUZZER(audioEvent(AU_ERROR), beep(4))
#define AUDIO_TIMER_COUNTDOWN(idx, val) audioTimerCountdown(idx, val)
#define AUDIO_TIMER_ELAPSED(idx) AUDIO_BUZZER(audioEvent(AU_TIMER1_ELAPSED+idx), beep(3))
#define AUDIO_INACTIVITY()       AUDIO_BUZZER(audioEvent(AU_INACTIVITY), beep(3))
#define AUDIO_MIX_WARNING(x)     AUDIO_BUZZER(audioEvent(AU_MIX_WARNING_1+x-1), beep(1))
#define AUDIO_POT_MIDDLE(x)      AUDIO_BUZZER(audioEvent(AU_STICK1_MIDDLE+x), beep(2))
#define AUDIO_TRIM_MIDDLE()      AUDIO_BUZZER(audioEvent(AU_TRIM_MIDDLE), beep(2))
#define AUDIO_TRIM_MIN()         AUDIO_BUZZER(audioEvent(AU_TRIM_MIN), beep(2))
#define AUDIO_TRIM_MAX()         AUDIO_BUZZER(audioEvent(AU_TRIM_MAX), beep(2))
#define AUDIO_TRIM_PRESS(val)    audioTrimPress(val)
#define AUDIO_PLAY(p)            audioEvent(p)
#define AUDIO_VARIO(fq, t, p, f) audioQueue.playTone(fq, t, p, f)
#define AUDIO_RSSI_ORANGE()      audioEvent(AU_RSSI_ORANGE)
#define AUDIO_RSSI_RED()         audioEvent(AU_RSSI_RED)
#define AUDIO_SWR_RED()          audioEvent(AU_SWR_RED)
#define AUDIO_TELEMETRY_LOST()   audioEvent(AU_TELEMETRY_LOST)
#define AUDIO_TELEMETRY_BACK()   audioEvent(AU_TELEMETRY_BACK)
#define AUDIO_TRAINER_LOST()     audioEvent(AU_TRAINER_LOST)
#define AUDIO_TRAINER_BACK()     audioEvent(AU_TRAINER_BACK)

#define AUDIO_HEARTBEAT()

enum AutomaticPromptsCategories {
  SYSTEM_AUDIO_CATEGORY,
  MODEL_AUDIO_CATEGORY,
  PHASE_AUDIO_CATEGORY,
  SWITCH_AUDIO_CATEGORY,
  LOGICAL_SWITCH_AUDIO_CATEGORY,
};

enum AutomaticPromptsEvents {
  AUDIO_EVENT_OFF,
  AUDIO_EVENT_ON,
  AUDIO_EVENT_MID,
};

void pushPrompt(uint16_t prompt, uint8_t id=0);
void pushUnit(uint8_t unit, uint8_t idx, uint8_t id);
void playModelName();

#define I18N_PLAY_FUNCTION(lng, x, ...) void lng ## _ ## x(__VA_ARGS__, uint8_t id)
#define PLAY_FUNCTION(x, ...)    void x(__VA_ARGS__, uint8_t id)
#define PUSH_NUMBER_PROMPT(p)    pushPrompt((p), id)
#define PUSH_UNIT_PROMPT(p, i)   pushUnit((p), (i), id)
#define PLAY_NUMBER(n, u, a)     playNumber((n), (u), (a), id)
#define PLAY_DURATION(d, att)    playDuration((d), (att), id)
#define PLAY_DURATION_ATT        , uint8_t flags
#define PLAY_TIME                1
#define IS_PLAY_TIME()           (flags&PLAY_TIME)
#define IS_PLAYING(id)           audioQueue.isPlaying((id))
#define PLAY_VALUE(v, id)        playValue((v), (id))
#define PLAY_FILE(f, flags, id)  audioQueue.playFile((f), (flags), (id))
#define STOP_PLAY(id)            audioQueue.stopPlay((id))
#define AUDIO_RESET()            audioQueue.stopAll()
#define AUDIO_FLUSH()            audioQueue.flush()

#if defined(SDCARD)
  extern tmr10ms_t timeAutomaticPromptsSilence;
  void playModelEvent(uint8_t category, uint8_t index, event_t event=0);
  #define PLAY_PHASE_OFF(phase)         playModelEvent(PHASE_AUDIO_CATEGORY, phase, AUDIO_EVENT_OFF)
  #define PLAY_PHASE_ON(phase)          playModelEvent(PHASE_AUDIO_CATEGORY, phase, AUDIO_EVENT_ON)
  #define PLAY_SWITCH_MOVED(sw)         playModelEvent(SWITCH_AUDIO_CATEGORY, sw)
  #define PLAY_LOGICAL_SWITCH_OFF(sw)   playModelEvent(LOGICAL_SWITCH_AUDIO_CATEGORY, sw, AUDIO_EVENT_OFF)
  #define PLAY_LOGICAL_SWITCH_ON(sw)    playModelEvent(LOGICAL_SWITCH_AUDIO_CATEGORY, sw, AUDIO_EVENT_ON)
  #define PLAY_MODEL_NAME()             playModelName()
  #define START_SILENCE_PERIOD()        timeAutomaticPromptsSilence = get_tmr10ms()
  #define IS_SILENCE_PERIOD_ELAPSED()   (get_tmr10ms()-timeAutomaticPromptsSilence > 50)
#else
  #define PLAY_PHASE_OFF(phase)
  #define PLAY_PHASE_ON(phase)
  #define PLAY_SWITCH_MOVED(sw)
  #define PLAY_LOGICAL_SWITCH_OFF(sw)
  #define PLAY_LOGICAL_SWITCH_ON(sw)
  #define PLAY_MODEL_NAME()
  #define START_SILENCE_PERIOD()
#endif

char * getAudioPath(char * path);

void referenceSystemAudioFiles();
void referenceModelAudioFiles();

bool isAudioFileReferenced(uint32_t i, char * filename/*at least AUDIO_FILENAME_MAXLEN+1 long*/);

#endif // _AUDIO_ARM_H_
