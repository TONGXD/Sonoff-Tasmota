/*
  xsns_01_counter.ino - Counter sensors (water meters, electricity meters etc.) sensor support for Sonoff-Tasmota

  Copyright (C) 2019  Maarten Damen and Theo Arends

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/*********************************************************************************************\
 * Counter sensors (water meters, electricity meters etc.)
\*********************************************************************************************/

#define XSNS_01             1


// if defined shows gas meter count for counter 1
// parameter is debouncing in us for switch open in gas counters
// set as low as possible
#define GAS_COUNTER_MODE 5000
// gas poll mode every 100 ms (no irqs) GAS_COUNTER_MODE must be >0 but value is ignored
#define GAS_POLL_MODE
#define GAS_DECIMALS 2
// if defined mirrors io state to a led (sampled at 100 ms intervalls)
//#define GAS_LED 2


unsigned long last_counter_timer[MAX_COUNTERS]; // Last counter time in micro seconds

#ifdef GAS_COUNTER_MODE
uint8_t gas_debounce;
uint8_t gas_old_state;
#endif

void CounterUpdate(byte index)
{
  unsigned long counter_debounce_time = micros() - last_counter_timer[index -1];
  if (counter_debounce_time > Settings.pulse_counter_debounce * 1000) {
    last_counter_timer[index -1] = micros();
    if (bitRead(Settings.pulse_counter_type, index -1)) {
      RtcSettings.pulse_counter[index -1] = counter_debounce_time;
    } else {
#ifdef GAS_COUNTER_MODE
#ifndef GAS_POLL_MODE
      if (index==1) {
        delayMicroseconds(GAS_COUNTER_MODE);
        if (!digitalRead(pin[GPIO_CNTR1])) {
          RtcSettings.pulse_counter[index -1]++;
        }
      } else {
        RtcSettings.pulse_counter[index -1]++;
      }
#endif
#else
      RtcSettings.pulse_counter[index -1]++;
#endif
    }

//    snprintf_P(log_data, sizeof(log_data), PSTR("CNTR: Interrupt %d"), index);
//    AddLog(LOG_LEVEL_DEBUG);
  }
}

void CounterUpdate1(void)
{
  CounterUpdate(1);
}

void CounterUpdate2(void)
{
  CounterUpdate(2);
}

void CounterUpdate3(void)
{
  CounterUpdate(3);
}

void CounterUpdate4(void)
{
  CounterUpdate(4);
}

/********************************************************************************************/

void CounterSaveState(void)
{
  for (byte i = 0; i < MAX_COUNTERS; i++) {
    if (pin[GPIO_CNTR1 +i] < 99) {
      Settings.pulse_counter[i] = RtcSettings.pulse_counter[i];
    }
  }
}

void CounterInit(void)
{
  typedef void (*function) () ;
  function counter_callbacks[] = { CounterUpdate1, CounterUpdate2, CounterUpdate3, CounterUpdate4 };

  for (byte i = 0; i < MAX_COUNTERS; i++) {
    if (pin[GPIO_CNTR1 +i] < 99) {
      pinMode(pin[GPIO_CNTR1 +i], bitRead(counter_no_pullup, i) ? INPUT : INPUT_PULLUP);
#ifdef GAS_POLL_MODE
      if (i>0) attachInterrupt(pin[GPIO_CNTR1 +i], counter_callbacks[i], FALLING);
#else
      attachInterrupt(pin[GPIO_CNTR1 +i], counter_callbacks[i], FALLING);
#endif
    }
  }

#ifdef GAS_COUNTER_MODE
  gas_debounce=0;
#endif
}

#ifdef USE_WEBSERVER
const char HTTP_SNS_COUNTER[] PROGMEM =
  "%s{s}" D_COUNTER "%d{m}%s%s{e}";  // {s} = <tr><th>, {m} = </th><td>, {e} = </td></tr>

#ifdef GAS_COUNTER_MODE
const char HTTP_SNS_COUNTER_GAS[] PROGMEM =
 "%s{s}" "Gaszähler" "{m}%s%s cbm{e}";
#endif

#endif  // USE_WEBSERVER

void CounterShow(boolean json)
{
  char stemp[10];

  byte dsxflg = 0;
  byte header = 0;
  for (byte i = 0; i < MAX_COUNTERS; i++) {
    if (pin[GPIO_CNTR1 +i] < 99) {
      char counter[33];
      if (bitRead(Settings.pulse_counter_type, i)) {
        dtostrfd((double)RtcSettings.pulse_counter[i] / 1000000, 6, counter);
      } else {
        dsxflg++;
#ifdef GAS_COUNTER_MODE
        if (i==0) {
          uint16_t scale=1;
          for (uint8_t s=0; s<GAS_DECIMALS; s++) scale*=10;
          dtostrfd((double)RtcSettings.pulse_counter[i]/(double)scale,GAS_DECIMALS, counter);
        } else {
          dtostrfd(RtcSettings.pulse_counter[i], 0, counter);
        }
#else
        dtostrfd(RtcSettings.pulse_counter[i], 0, counter);
#endif
      }

      if (json) {
        if (!header) {
          snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("%s,\"COUNTER\":{"), mqtt_data);
          stemp[0] = '\0';
        }
        header++;
        snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("%s%s\"C%d\":%s"), mqtt_data, stemp, i +1, counter);
        strlcpy(stemp, ",", sizeof(stemp));
#ifdef USE_DOMOTICZ
        if ((0 == tele_period) && (1 == dsxflg)) {
          DomoticzSensor(DZ_COUNT, RtcSettings.pulse_counter[i]);
          dsxflg++;
        }
#endif  // USE_DOMOTICZ
#ifdef USE_WEBSERVER
      } else {
#ifdef GAS_COUNTER_MODE
      if (i==0) {
        snprintf_P(mqtt_data, sizeof(mqtt_data), HTTP_SNS_COUNTER_GAS, mqtt_data, counter, (bitRead(Settings.pulse_counter_type, i)) ? " " D_UNIT_SECOND : "");
      } else {
        snprintf_P(mqtt_data, sizeof(mqtt_data), HTTP_SNS_COUNTER, mqtt_data, i +1, counter, (bitRead(Settings.pulse_counter_type, i)) ? " " D_UNIT_SECOND : "");
      }
#else
        snprintf_P(mqtt_data, sizeof(mqtt_data), HTTP_SNS_COUNTER, mqtt_data, i +1, counter, (bitRead(Settings.pulse_counter_type, i)) ? " " D_UNIT_SECOND : "");
#endif
#endif  // USE_WEBSERVER
      }
    }
    if (bitRead(Settings.pulse_counter_type, i)) {
      RtcSettings.pulse_counter[i] = 0xFFFFFFFF;  // Set Timer to max in case of no more interrupts due to stall of measured device
    }
  }
  if (json) {
    if (header) {
      snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("%s}"), mqtt_data);
    }
  }
}


#ifdef GAS_COUNTER_MODE
// poll pin every 100 ms
void GAS_Poll(void) {

#ifdef GAS_POLL_MODE
  uint8_t state;
  gas_debounce<<=1;
  gas_debounce|=(digitalRead(pin[GPIO_CNTR1])&1)|0xe0;
  if (gas_debounce==0xf0) {
    // is 1
    state=1;
  } else {
    // is 0, means switch down
    state=0;
  }
  if (gas_old_state!=state) {
    // state has changed
    gas_old_state=state;
    if (state==0) {
      // inc counter
      RtcSettings.pulse_counter[0]++;
    }
  }
#endif

// debug wemos d1 led
#ifdef GAS_LED
  pinMode(GAS_LED, OUTPUT);
  if (digitalRead(pin[GPIO_CNTR1])) {
    digitalWrite(GAS_LED,HIGH);
  } else {
    digitalWrite(GAS_LED,LOW);
  }
#endif
}
#endif


/*********************************************************************************************\
 * Interface
\*********************************************************************************************/

boolean Xsns01(byte function)
{
  boolean result = false;

  switch (function) {

#ifdef GAS_COUNTER_MODE
    case FUNC_EVERY_100_MSECOND:
      GAS_Poll();
      break;
#endif

    case FUNC_INIT:
      CounterInit();
      break;
    case FUNC_JSON_APPEND:
      CounterShow(1);
      break;
#ifdef USE_WEBSERVER
    case FUNC_WEB_APPEND:
      CounterShow(0);
      break;
#endif  // USE_WEBSERVER
    case FUNC_SAVE_BEFORE_RESTART:
      CounterSaveState();
      break;
  }
  return result;
}
