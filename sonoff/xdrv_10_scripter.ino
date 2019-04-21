/*
  xdrv_10_rules.ino - rule support for Sonoff-Tasmota

  Copyright (C) 2019  ESP Easy Group and Theo Arends

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

#ifdef USE_RULES
/*********************************************************************************************\
 * Rules based heavily on ESP Easy implementation
 *
 * Inspiration: https://github.com/letscontrolit/ESPEasy
 *
 * Add rules using the following, case insensitive, format:
 *   on <trigger1> do <commands> endon on <trigger2> do <commands> endon ..
 *
 * Examples:
 *   on System#Boot do Color 001000 endon
 *   on INA219#Current>0.100 do Dimmer 10 endon
 *   on INA219#Current>0.100 do Backlog Dimmer 10;Color 10,0,0 endon
 *   on INA219#Current>0.100 do Backlog Dimmer 10;Color 100000 endon on System#Boot do color 001000 endon
 *   on ds18b20#temperature>23 do power off endon on ds18b20#temperature<22 do power on endon
 *   on mqtt#connected do color 000010 endon
 *   on mqtt#disconnected do color 00100C endon
 *   on time#initialized do color 001000 endon
 *   on time#initialized>120 do color 001000 endon
 *   on time#set do color 001008 endon
 *   on clock#timer=3 do color 080800 endon
 *   on rules#timer=1 do color 080800 endon
 *   on mqtt#connected do color 000010 endon on mqtt#disconnected do color 001010 endon on time#initialized do color 001000 endon on time#set do backlog color 000810;ruletimer1 10 endon on rules#timer=1 do color 080800 endon
 *   on event#anyname do color 100000 endon
 *   on event#anyname do color %value% endon
 *   on power1#state=1 do color 001000 endon
 *   on button1#state do publish cmnd/ring2/power %value% endon on button2#state do publish cmnd/strip1/power %value% endon
 *   on switch1#state do power2 %value% endon
 *   on analog#a0div10 do publish cmnd/ring2/dimmer %value% endon
 *
 * Notes:
 *   Spaces after <on>, around <do> and before <endon> are mandatory
 *   System#Boot is initiated after MQTT is connected due to command handling preparation
 *   Control rule triggering with command:
 *     Rule 0 = Rules disabled (Off)
 *     Rule 1 = Rules enabled (On)
 *     Rule 2 = Toggle rules state
 *     Rule 4 = Perform commands as long as trigger is met (Once OFF)
 *     Rule 5 = Perform commands once until trigger is not met (Once ON)
 *     Rule 6 = Toggle Once state
 *   Execute an event like:
 *     Event anyname=001000
 *   Set a RuleTimer to 100 seconds like:
 *     RuleTimer2 100
\*********************************************************************************************/

#define XDRV_10             10


// global memory
struct SCRIPT_MEM {
    float *fvars; // number var pointer
    char *glob_vnp; // var name pointer
    uint8_t *vnp_offset;
    char *glob_snp; // string vars pointer
    uint8_t *snp_offset;
    uint8_t *type; // type and index pointer
    uint16_t numvars;
    void *script_mem;
    uint16_t script_mem_size;
    uint16_t section;
} glob_script_mem;
enum {SECTION_BOOT=1,SECTION_TELE,SECTION_TIME};



#define D_CMND_RULE "Rule"
#define D_CMND_RULETIMER "RuleTimer"
#define D_CMND_EVENT "Event"
#define D_CMND_VAR "Var"
#define D_CMND_MEM "Mem"
#define D_CMND_ADD "Add"
#define D_CMND_SUB "Sub"
#define D_CMND_MULT "Mult"
#define D_CMND_SCALE "Scale"
#define D_CMND_CALC_RESOLUTION "CalcRes"

#define D_JSON_INITIATED "Initiated"

enum RulesCommands { CMND_RULE, CMND_RULETIMER, CMND_EVENT, CMND_VAR, CMND_MEM, CMND_ADD, CMND_SUB, CMND_MULT, CMND_SCALE, CMND_CALC_RESOLUTION };
const char kRulesCommands[] PROGMEM = D_CMND_RULE "|" D_CMND_RULETIMER "|" D_CMND_EVENT "|" D_CMND_VAR "|" D_CMND_MEM "|" D_CMND_ADD "|" D_CMND_SUB "|" D_CMND_MULT "|" D_CMND_SCALE "|" D_CMND_CALC_RESOLUTION ;

String rules_event_value;
unsigned long rules_timer[MAX_RULE_TIMERS] = { 0 };
uint8_t rules_quota = 0;
long rules_new_power = -1;
long rules_old_power = -1;
long rules_old_dimm = -1;

uint32_t rules_triggers[MAX_RULE_SETS] = { 0 };
uint16_t rules_last_minute = 60;
uint8_t rules_trigger_count[MAX_RULE_SETS] = { 0 };
uint8_t rules_teleperiod = 0;

char event_data[100];
char vars[MAX_RULE_VARS][33] = { 0 };
#if (MAX_RULE_VARS>16)
#error MAX_RULE_VARS is bigger than 16
#endif
#if (MAX_RULE_MEMS>5)
#error MAX_RULE_MEMS is bigger than 5
#endif
uint16_t vars_event = 0;
uint8_t mems_event = 0;

/*******************************************************************************************/

bool RulesRuleMatch(byte rule_set, String &event, String &rule)
{
  // event = {"INA219":{"Voltage":4.494,"Current":0.020,"Power":0.089}}
  // event = {"System":{"Boot":1}}
  // rule = "INA219#CURRENT>0.100"

  bool match = false;
  char stemp[10];

  // Step1: Analyse rule
  int pos = rule.indexOf('#');
  if (pos == -1) { return false; }                     // No # sign in rule

  String rule_task = rule.substring(0, pos);           // "INA219" or "SYSTEM"
  if (rules_teleperiod) {
    int ppos = rule_task.indexOf("TELE-");             // "TELE-INA219" or "INA219"
    if (ppos == -1) { return false; }                  // No pre-amble in rule
    rule_task = rule.substring(5, pos);                // "INA219" or "SYSTEM"
  }

  String rule_name = rule.substring(pos +1);           // "CURRENT>0.100" or "BOOT" or "%var1%" or "MINUTE|5"

  char compare = ' ';
  pos = rule_name.indexOf(">");
  if (pos > 0) {
    compare = '>';
  } else {
    pos = rule_name.indexOf("<");
    if (pos > 0) {
      compare = '<';
    } else {
      pos = rule_name.indexOf("=");
      if (pos > 0) {
        compare = '=';
      } else {
        pos = rule_name.indexOf("|");                  // Modulo, cannot use % easily as it is used for variable detection
        if (pos > 0) {
          compare = '%';
        }
      }
    }
  }

  char rule_svalue[CMDSZ] = { 0 };
  double rule_value = 0;
  if (pos > 0) {
    String rule_param = rule_name.substring(pos + 1);
    for (byte i = 0; i < MAX_RULE_VARS; i++) {
      snprintf_P(stemp, sizeof(stemp), PSTR("%%VAR%d%%"), i +1);
      if (rule_param.startsWith(stemp)) {
        rule_param = vars[i];
        break;
      }
    }
    for (byte i = 0; i < MAX_RULE_MEMS; i++) {
      snprintf_P(stemp, sizeof(stemp), PSTR("%%MEM%d%%"), i +1);
      if (rule_param.startsWith(stemp)) {
        rule_param = Settings.mems[i];
        break;
      }
    }
    snprintf_P(stemp, sizeof(stemp), PSTR("%%TIME%%"));
    if (rule_param.startsWith(stemp)) {
      rule_param = String(GetMinutesPastMidnight());
    }
    snprintf_P(stemp, sizeof(stemp), PSTR("%%UPTIME%%"));
    if (rule_param.startsWith(stemp)) {
      rule_param = String(GetMinutesUptime());
    }
    snprintf_P(stemp, sizeof(stemp), PSTR("%%TIMESTAMP%%"));
    if (rule_param.startsWith(stemp)) {
      rule_param = GetDateAndTime(DT_LOCAL).c_str();
    }
#if defined(USE_TIMERS) && defined(USE_SUNRISE)
    snprintf_P(stemp, sizeof(stemp), PSTR("%%SUNRISE%%"));
    if (rule_param.startsWith(stemp)) {
      rule_param = String(GetSunMinutes(0));
    }
    snprintf_P(stemp, sizeof(stemp), PSTR("%%SUNSET%%"));
    if (rule_param.startsWith(stemp)) {
      rule_param = String(GetSunMinutes(1));
    }
#endif  // USE_TIMERS and USE_SUNRISE
    rule_param.toUpperCase();
    snprintf(rule_svalue, sizeof(rule_svalue), rule_param.c_str());

    int temp_value = GetStateNumber(rule_svalue);
    if (temp_value > -1) {
      rule_value = temp_value;
    } else {
      rule_value = CharToDouble((char*)rule_svalue);   // 0.1      - This saves 9k code over toFLoat()!
    }
    rule_name = rule_name.substring(0, pos);           // "CURRENT"
  }

  // Step2: Search rule_task and rule_name
  StaticJsonBuffer<1024> jsonBuf;
  JsonObject &root = jsonBuf.parseObject(event);
  if (!root.success()) { return false; }               // No valid JSON data

  double value = 0;
  const char* str_value = root[rule_task][rule_name];

//snprintf_P(log_data, sizeof(log_data), PSTR("RUL: Task %s, Name %s, Value |%s|, TrigCnt %d, TrigSt %d, Source %s, Json %s"),
//  rule_task.c_str(), rule_name.c_str(), rule_svalue, rules_trigger_count[rule_set], bitRead(rules_triggers[rule_set], rules_trigger_count[rule_set]), event.c_str(), (str_value) ? str_value : "none");
//AddLog(LOG_LEVEL_DEBUG);

  if (!root[rule_task][rule_name].success()) { return false; }
  // No value but rule_name is ok

  rules_event_value = str_value;                       // Prepare %value%

  // Step 3: Compare rule (value)
  if (str_value) {
    value = CharToDouble((char*)str_value);
    int int_value = int(value);
    int int_rule_value = int(rule_value);
    switch (compare) {
      case '%':
        if ((int_value > 0) && (int_rule_value > 0)) {
          if ((int_value % int_rule_value) == 0) { match = true; }
        }
        break;
      case '>':
        if (value > rule_value) { match = true; }
        break;
      case '<':
        if (value < rule_value) { match = true; }
        break;
      case '=':
//        if (value == rule_value) { match = true; }     // Compare values - only decimals or partly hexadecimals
        if (!strcasecmp(str_value, rule_svalue)) { match = true; }  // Compare strings - this also works for hexadecimals
        break;
      case ' ':
        match = true;                                  // Json value but not needed
        break;
    }
  } else match = true;

  if (bitRead(Settings.rule_once, rule_set)) {
    if (match) {                                       // Only allow match state changes
      if (!bitRead(rules_triggers[rule_set], rules_trigger_count[rule_set])) {
        bitSet(rules_triggers[rule_set], rules_trigger_count[rule_set]);
      } else {
        match = false;
      }
    } else {
      bitClear(rules_triggers[rule_set], rules_trigger_count[rule_set]);
    }
  }

  return match;
}

/*******************************************************************************************/


bool RuleSetProcess(byte rule_set, String &event_saved)
{
  bool serviced = false;
  char stemp[10];

  delay(0);                                               // Prohibit possible loop software watchdog

//snprintf_P(log_data, sizeof(log_data), PSTR("RUL: Event = %s, Rule = %s"), event_saved.c_str(), Settings.rules[rule_set]);
//AddLog(LOG_LEVEL_DEBUG);

  String rules = Settings.rules[rule_set];

  rules_trigger_count[rule_set] = 0;
  int plen = 0;
  int plen2 = 0;
  bool stop_all_rules = false;
  while (true) {
    rules = rules.substring(plen);                        // Select relative to last rule
    rules.trim();
    if (!rules.length()) { return serviced; }             // No more rules

    String rule = rules;
    rule.toUpperCase();                                   // "ON INA219#CURRENT>0.100 DO BACKLOG DIMMER 10;COLOR 100000 ENDON"
    if (!rule.startsWith("ON ")) { return serviced; }     // Bad syntax - Nothing to start on

    int pevt = rule.indexOf(" DO ");
    if (pevt == -1) { return serviced; }                  // Bad syntax - Nothing to do
    String event_trigger = rule.substring(3, pevt);       // "INA219#CURRENT>0.100"

    plen = rule.indexOf(" ENDON");
    plen2 = rule.indexOf(" BREAK");
    if ((plen == -1) && (plen2 == -1)) { return serviced; } // Bad syntax - No ENDON neither BREAK

    if (plen == -1) { plen = 9999; }
    if (plen2 == -1) { plen2 = 9999; }
    plen = tmin(plen, plen2);
    if (plen == plen2) { stop_all_rules = true; }     // If BREAK was used, Stop execution of this rule set

    String commands = rules.substring(pevt +4, plen);     // "Backlog Dimmer 10;Color 100000"
    plen += 6;
    rules_event_value = "";
    String event = event_saved;

//snprintf_P(log_data, sizeof(log_data), PSTR("RUL: Event |%s|, Rule |%s|, Command(s) |%s|"), event.c_str(), event_trigger.c_str(), commands.c_str());
//AddLog(LOG_LEVEL_DEBUG);

    if (RulesRuleMatch(rule_set, event, event_trigger)) {
      commands.trim();
      String ucommand = commands;
      ucommand.toUpperCase();
//      if (!ucommand.startsWith("BACKLOG")) { commands = "backlog " + commands; }  // Always use Backlog to prevent power race exception
      if (ucommand.indexOf("EVENT ") != -1) { commands = "backlog " + commands; }  // Always use Backlog with event to prevent rule event loop exception
      commands.replace(F("%value%"), rules_event_value);
      for (byte i = 0; i < MAX_RULE_VARS; i++) {
        snprintf_P(stemp, sizeof(stemp), PSTR("%%var%d%%"), i +1);
        commands.replace(stemp, vars[i]);
      }
      for (byte i = 0; i < MAX_RULE_MEMS; i++) {
        snprintf_P(stemp, sizeof(stemp), PSTR("%%mem%d%%"), i +1);
        commands.replace(stemp, Settings.mems[i]);
      }
      commands.replace(F("%time%"), String(GetMinutesPastMidnight()));
      commands.replace(F("%uptime%"), String(GetMinutesUptime()));
      commands.replace(F("%timestamp%"), GetDateAndTime(DT_LOCAL).c_str());
#if defined(USE_TIMERS) && defined(USE_SUNRISE)
      commands.replace(F("%sunrise%"), String(GetSunMinutes(0)));
      commands.replace(F("%sunset%"), String(GetSunMinutes(1)));
#endif  // USE_TIMERS and USE_SUNRISE

      //char command[commands.length() +1];
      //snprintf(command, sizeof(command), commands.c_str());

      snprintf_P(log_data, sizeof(log_data), PSTR("RUL: %s performs \"%s\""), event_trigger.c_str(), (char*)commands.c_str());
      AddLog(LOG_LEVEL_INFO);

//      snprintf_P(mqtt_data, sizeof(mqtt_data), S_JSON_COMMAND_SVALUE, D_CMND_RULE, D_JSON_INITIATED);
//      MqttPublishPrefixTopic_P(RESULT_OR_STAT, PSTR(D_CMND_RULE));
      ExecuteCommand((char*)commands.c_str(), SRC_RULE);
      serviced = true;
      if (stop_all_rules) { return serviced; }     // If BREAK was used, Stop execution of this rule set
    }
    rules_trigger_count[rule_set]++;
  }
  return serviced;
}

/*******************************************************************************************/

bool RulesProcessEvent(char *json_event)
{
  bool serviced = false;

  ShowFreeMem(PSTR("RulesProcessEvent"));

  String event_saved = json_event;
  event_saved.toUpperCase();

//snprintf_P(log_data, sizeof(log_data), PSTR("RUL: Event %s"), event_saved.c_str());
//AddLog(LOG_LEVEL_DEBUG);

  for (byte i = 0; i < MAX_RULE_SETS; i++) {
    if (strlen(Settings.rules[i]) && bitRead(Settings.rule_enabled, i)) {
      if (RuleSetProcess(i, event_saved)) { serviced = true; }
    }
  }
  return serviced;
}

bool RulesProcess(void)
{
  return RulesProcessEvent(mqtt_data);
}

void RulesInit(void)
{
  rules_flag.data = 0;
  for (byte i = 0; i < MAX_RULE_SETS; i++) {
    if (Settings.rules[i][0] == '\0') {
      bitWrite(Settings.rule_enabled, i, 0);
      bitWrite(Settings.rule_once, i, 0);
    }
  }
  rules_teleperiod = 0;
}

void RulesEvery50ms(void)
{
  if (Settings.rule_enabled) {  // Any rule enabled
    char json_event[120];

    if (-1 == rules_new_power) { rules_new_power = power; }
    if (rules_new_power != rules_old_power) {
      if (rules_old_power != -1) {
        for (byte i = 0; i < devices_present; i++) {
          uint8_t new_state = (rules_new_power >> i) &1;
          if (new_state != ((rules_old_power >> i) &1)) {
            snprintf_P(json_event, sizeof(json_event), PSTR("{\"Power%d\":{\"State\":%d}}"), i +1, new_state);
            RulesProcessEvent(json_event);
          }
        }
      } else {
        // Boot time POWER OUTPUTS (Relays) Status
        for (byte i = 0; i < devices_present; i++) {
          uint8_t new_state = (rules_new_power >> i) &1;
          snprintf_P(json_event, sizeof(json_event), PSTR("{\"Power%d\":{\"Boot\":%d}}"), i +1, new_state);
          RulesProcessEvent(json_event);
        }
        // Boot time SWITCHES Status
        for (byte i = 0; i < MAX_SWITCHES; i++) {
#ifdef USE_TM1638
          if ((pin[GPIO_SWT1 +i] < 99) || ((pin[GPIO_TM16CLK] < 99) && (pin[GPIO_TM16DIO] < 99) && (pin[GPIO_TM16STB] < 99))) {
#else
          if (pin[GPIO_SWT1 +i] < 99) {
#endif // USE_TM1638
            boolean swm = ((FOLLOW_INV == Settings.switchmode[i]) || (PUSHBUTTON_INV == Settings.switchmode[i]) || (PUSHBUTTONHOLD_INV == Settings.switchmode[i]));
            snprintf_P(json_event, sizeof(json_event), PSTR("{\"" D_JSON_SWITCH "%d\":{\"Boot\":%d}}"), i +1, (swm ^ SwitchLastState(i)));
            RulesProcessEvent(json_event);
          }
        }
      }
      rules_old_power = rules_new_power;
    }
    else if (rules_old_dimm != Settings.light_dimmer) {
      if (rules_old_dimm != -1) {
        snprintf_P(json_event, sizeof(json_event), PSTR("{\"Dimmer\":{\"State\":%d}}"), Settings.light_dimmer);
      } else {
        // Boot time DIMMER VALUE
        snprintf_P(json_event, sizeof(json_event), PSTR("{\"Dimmer\":{\"Boot\":%d}}"), Settings.light_dimmer);
      }
      RulesProcessEvent(json_event);
      rules_old_dimm = Settings.light_dimmer;
    }
    else if (event_data[0]) {
      char *event;
      char *parameter;
      event = strtok_r(event_data, "=", &parameter);     // event_data = fanspeed=10
      if (event) {
        event = Trim(event);
        if (parameter) {
          parameter = Trim(parameter);
        } else {
          parameter = event + strlen(event);  // '\0'
        }
        snprintf_P(json_event, sizeof(json_event), PSTR("{\"Event\":{\"%s\":\"%s\"}}"), event, parameter);
        event_data[0] ='\0';
        RulesProcessEvent(json_event);
      } else {
        event_data[0] ='\0';
      }
    }
    else if (vars_event) {
      for (byte i = 0; i < MAX_RULE_VARS-1; i++) {
        if (bitRead(vars_event, i)) {
          bitClear(vars_event, i);
          snprintf_P(json_event, sizeof(json_event), PSTR("{\"Var%d\":{\"State\":%s}}"), i+1, vars[i]);
          RulesProcessEvent(json_event);
          break;
        }
      }
    }
    else if (mems_event) {
      for (byte i = 0; i < MAX_RULE_MEMS-1; i++) {
        if (bitRead(mems_event, i)) {
          bitClear(mems_event, i);
          snprintf_P(json_event, sizeof(json_event), PSTR("{\"Mem%d\":{\"State\":%s}}"), i+1, Settings.mems[i]);
          RulesProcessEvent(json_event);
          break;
        }
      }
    }
    else if (rules_flag.data) {
      uint16_t mask = 1;
      for (byte i = 0; i < MAX_RULES_FLAG; i++) {
        if (rules_flag.data & mask) {
          rules_flag.data ^= mask;
          json_event[0] = '\0';
          switch (i) {
            case 0: strncpy_P(json_event, PSTR("{\"System\":{\"Boot\":1}}"), sizeof(json_event)); break;
            case 1: snprintf_P(json_event, sizeof(json_event), PSTR("{\"Time\":{\"Initialized\":%d}}"), GetMinutesPastMidnight()); break;
            case 2: snprintf_P(json_event, sizeof(json_event), PSTR("{\"Time\":{\"Set\":%d}}"), GetMinutesPastMidnight()); break;
            case 3: strncpy_P(json_event, PSTR("{\"MQTT\":{\"Connected\":1}}"), sizeof(json_event)); break;
            case 4: strncpy_P(json_event, PSTR("{\"MQTT\":{\"Disconnected\":1}}"), sizeof(json_event)); break;
            case 5: strncpy_P(json_event, PSTR("{\"WIFI\":{\"Connected\":1}}"), sizeof(json_event)); break;
            case 6: strncpy_P(json_event, PSTR("{\"WIFI\":{\"Disconnected\":1}}"), sizeof(json_event)); break;
          }
          if (json_event[0]) {
            RulesProcessEvent(json_event);
            break;                       // Only service one event within 50mS
          }
        }
        mask <<= 1;
      }
    }
  }
}

uint8_t rules_xsns_index = 0;

void RulesEvery100ms(void)
{
  if (Settings.rule_enabled && (uptime > 4)) {  // Any rule enabled and allow 4 seconds start-up time for sensors (#3811)
    mqtt_data[0] = '\0';
    int tele_period_save = tele_period;
    tele_period = 2;                                   // Do not allow HA updates during next function call
    XsnsNextCall(FUNC_JSON_APPEND, rules_xsns_index);  // ,"INA219":{"Voltage":4.494,"Current":0.020,"Power":0.089}
    tele_period = tele_period_save;
    if (strlen(mqtt_data)) {
      mqtt_data[0] = '{';                              // {"INA219":{"Voltage":4.494,"Current":0.020,"Power":0.089}
      snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("%s}"), mqtt_data);
      RulesProcess();
    }
  }
}

void RulesEverySecond(void)
{
  if (Settings.rule_enabled) {  // Any rule enabled
    char json_event[120];

    if (RtcTime.valid) {
      if ((uptime > 60) && (RtcTime.minute != rules_last_minute)) {  // Execute from one minute after restart every minute only once
        rules_last_minute = RtcTime.minute;
        snprintf_P(json_event, sizeof(json_event), PSTR("{\"Time\":{\"Minute\":%d}}"), GetMinutesPastMidnight());
        RulesProcessEvent(json_event);
      }
    }
    for (byte i = 0; i < MAX_RULE_TIMERS; i++) {
      if (rules_timer[i] != 0L) {           // Timer active?
        if (TimeReached(rules_timer[i])) {  // Timer finished?
          rules_timer[i] = 0L;              // Turn off this timer
          snprintf_P(json_event, sizeof(json_event), PSTR("{\"Rules\":{\"Timer\":%d}}"), i +1);
          RulesProcessEvent(json_event);
        }
      }
    }
  }
}

void RulesSetPower(void)
{
  rules_new_power = XdrvMailbox.index;
}

void RulesTeleperiod(void)
{
  //rules_teleperiod = 1;
  //RulesProcess();
  //rules_teleperiod = 0;
  Run_Scripter(Settings.rules[0],SECTION_TELE, mqtt_data);
}

boolean RulesCommand(void)
{
  char command[CMDSZ];
  boolean serviced = true;
  uint8_t index = XdrvMailbox.index;

  int command_code = GetCommandCode(command, sizeof(command), XdrvMailbox.topic, kRulesCommands);
  if (-1 == command_code) {
    serviced = false;  // Unknown command
  }
  else if ((CMND_RULE == command_code) && (index > 0) && (index <= MAX_RULE_SETS)) {
    if ((XdrvMailbox.data_len > 0) && (XdrvMailbox.data_len < sizeof(Settings.rules[index -1]))) {
      if ((XdrvMailbox.payload >= 0) && (XdrvMailbox.payload <= 10)) {
        switch (XdrvMailbox.payload) {
        case 0: // Off
        case 1: // On
          bitWrite(Settings.rule_enabled, index -1, XdrvMailbox.payload);
          break;
        case 2: // Toggle
          bitWrite(Settings.rule_enabled, index -1, bitRead(Settings.rule_enabled, index -1) ^1);
          break;
        case 4: // Off
        case 5: // On
          bitWrite(Settings.rule_once, index -1, XdrvMailbox.payload &1);
          break;
        case 6: // Toggle
          bitWrite(Settings.rule_once, index -1, bitRead(Settings.rule_once, index -1) ^1);
          break;
        case 8: // Off
        case 9: // On
          bitWrite(Settings.rule_stop, index -1, XdrvMailbox.payload &1);
          break;
        case 10: // Toggle
          bitWrite(Settings.rule_stop, index -1, bitRead(Settings.rule_stop, index -1) ^1);
          break;
        }
      } else {
        int offset = 0;
        if ('+' == XdrvMailbox.data[0]) {
          offset = strlen(Settings.rules[index -1]);
          if (XdrvMailbox.data_len < (sizeof(Settings.rules[index -1]) - offset -1)) {  // Check free space
            XdrvMailbox.data[0] = ' ';  // Remove + and make sure at least one space is inserted
          } else {
            offset = -1;                // Not enough space so skip it
          }
        }
        if (offset != -1) {
          strlcpy(Settings.rules[index -1] + offset, ('"' == XdrvMailbox.data[0]) ? "" : XdrvMailbox.data, sizeof(Settings.rules[index -1]));
        }
      }
      rules_triggers[index -1] = 0;  // Reset once flag
    }
    snprintf_P (mqtt_data, sizeof(mqtt_data), PSTR("{\"%s%d\":\"%s\",\"Once\":\"%s\",\"StopOnError\":\"%s\",\"Free\":%d,\"Rules\":\"%s\"}"),
      command, index, GetStateText(bitRead(Settings.rule_enabled, index -1)), GetStateText(bitRead(Settings.rule_once, index -1)),
      GetStateText(bitRead(Settings.rule_stop, index -1)), sizeof(Settings.rules[index -1]) - strlen(Settings.rules[index -1]) -1, Settings.rules[index -1]);
  }
  else if ((CMND_RULETIMER == command_code) && (index > 0) && (index <= MAX_RULE_TIMERS)) {
    if (XdrvMailbox.data_len > 0) {
      rules_timer[index -1] = (XdrvMailbox.payload > 0) ? millis() + (1000 * XdrvMailbox.payload) : 0;
    }
    mqtt_data[0] = '\0';
    for (byte i = 0; i < MAX_RULE_TIMERS; i++) {
      snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("%s%c\"T%d\":%d"), mqtt_data, (i) ? ',' : '{', i +1, (rules_timer[i]) ? (rules_timer[i] - millis()) / 1000 : 0);
    }
    snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("%s}"), mqtt_data);
  }
  else if (CMND_EVENT == command_code) {
    if (XdrvMailbox.data_len > 0) {
      strlcpy(event_data, XdrvMailbox.data, sizeof(event_data));
    }
    snprintf_P(mqtt_data, sizeof(mqtt_data), S_JSON_COMMAND_SVALUE, command, D_JSON_DONE);
  }
  else if ((CMND_VAR == command_code) && (index > 0) && (index <= MAX_RULE_VARS)) {
    if (XdrvMailbox.data_len > 0) {
      strlcpy(vars[index -1], ('"' == XdrvMailbox.data[0]) ? "" : XdrvMailbox.data, sizeof(vars[index -1]));
      bitSet(vars_event, index -1);
    }
    snprintf_P(mqtt_data, sizeof(mqtt_data), S_JSON_COMMAND_INDEX_SVALUE, command, index, vars[index -1]);
  }
  else if ((CMND_MEM == command_code) && (index > 0) && (index <= MAX_RULE_MEMS)) {
    if (XdrvMailbox.data_len > 0) {
      strlcpy(Settings.mems[index -1], ('"' == XdrvMailbox.data[0]) ? "" : XdrvMailbox.data, sizeof(Settings.mems[index -1]));
      bitSet(mems_event, index -1);
    }
    snprintf_P(mqtt_data, sizeof(mqtt_data), S_JSON_COMMAND_INDEX_SVALUE, command, index, Settings.mems[index -1]);
  }
  else if (CMND_CALC_RESOLUTION == command_code) {
    if ((XdrvMailbox.payload >= 0) && (XdrvMailbox.payload <= 7)) {
      Settings.flag2.calc_resolution = XdrvMailbox.payload;
    }
    snprintf_P(mqtt_data, sizeof(mqtt_data), S_JSON_COMMAND_NVALUE, command, Settings.flag2.calc_resolution);
  }
  else if ((CMND_ADD == command_code) && (index > 0) && (index <= MAX_RULE_VARS)) {
    if (XdrvMailbox.data_len > 0) {
      double tempvar = CharToDouble(vars[index -1]) + CharToDouble(XdrvMailbox.data);
      dtostrfd(tempvar, Settings.flag2.calc_resolution, vars[index -1]);
      bitSet(vars_event, index -1);
    }
    snprintf_P(mqtt_data, sizeof(mqtt_data), S_JSON_COMMAND_INDEX_SVALUE, command, index, vars[index -1]);
  }
  else if ((CMND_SUB == command_code) && (index > 0) && (index <= MAX_RULE_VARS)) {
    if (XdrvMailbox.data_len > 0) {
      double tempvar = CharToDouble(vars[index -1]) - CharToDouble(XdrvMailbox.data);
      dtostrfd(tempvar, Settings.flag2.calc_resolution, vars[index -1]);
      bitSet(vars_event, index -1);
    }
    snprintf_P(mqtt_data, sizeof(mqtt_data), S_JSON_COMMAND_INDEX_SVALUE, command, index, vars[index -1]);
  }
  else if ((CMND_MULT == command_code) && (index > 0) && (index <= MAX_RULE_VARS)) {
    if (XdrvMailbox.data_len > 0) {
      double tempvar = CharToDouble(vars[index -1]) * CharToDouble(XdrvMailbox.data);
      dtostrfd(tempvar, Settings.flag2.calc_resolution, vars[index -1]);
      bitSet(vars_event, index -1);
    }
    snprintf_P(mqtt_data, sizeof(mqtt_data), S_JSON_COMMAND_INDEX_SVALUE, command, index, vars[index -1]);
  }
  else if ((CMND_SCALE == command_code) && (index > 0) && (index <= MAX_RULE_VARS)) {
    if (XdrvMailbox.data_len > 0) {
      if (strstr(XdrvMailbox.data, ",")) {     // Process parameter entry
        char sub_string[XdrvMailbox.data_len +1];

        double valueIN = CharToDouble(subStr(sub_string, XdrvMailbox.data, ",", 1));
        double fromLow = CharToDouble(subStr(sub_string, XdrvMailbox.data, ",", 2));
        double fromHigh = CharToDouble(subStr(sub_string, XdrvMailbox.data, ",", 3));
        double toLow = CharToDouble(subStr(sub_string, XdrvMailbox.data, ",", 4));
        double toHigh = CharToDouble(subStr(sub_string, XdrvMailbox.data, ",", 5));
        double value = map_double(valueIN, fromLow, fromHigh, toLow, toHigh);
        dtostrfd(value, Settings.flag2.calc_resolution, vars[index -1]);
        bitSet(vars_event, index -1);
      }
    }
    snprintf_P(mqtt_data, sizeof(mqtt_data), S_JSON_COMMAND_INDEX_SVALUE, command, index, vars[index -1]);
  }
  else serviced = false;  // Unknown command

  return serviced;
}

double map_double(double x, double in_min, double in_max, double out_min, double out_max)
{
 return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}


#ifdef USE_WEBSERVER
#ifdef USE_RULES_GUI

#define WEB_HANDLE_RULES "s10"
#define D_CONFIGURE_RULES "Configure scripter"
#define D_RULEVARS "edit script"

const char S_CONFIGURE_RULES[] PROGMEM = D_CONFIGURE_RULES;

const char HTTP_BTN_MENU_RULES[] PROGMEM =
  "<br/><form action='" WEB_HANDLE_RULES "' method='get'><button>" D_CONFIGURE_RULES "</button></form>";

const char HTTP_FORM_RULES[] PROGMEM =
    "<fieldset><legend><b>&nbsp;" D_RULEVARS "&nbsp;</b></legend>"
    "<form method='post' action='" WEB_HANDLE_RULES "'>";

const char HTTP_FORM_RULES1[] PROGMEM =
    "<br/><input style='width:10%%;' id='c%d' name='c%d' type='checkbox'{r%d><b>script enable</b><br/>"
    "<br><textarea  id='t%1d' name='t%d' rows='20' cols='80' maxlength='%d' style='font-size: 12pt' >xyz%dxyz</textarea>";

const char HTTP_FORM_RULES2[] PROGMEM =
    "<br/><b>" "%s" "</b> (" "%s" ")<br/><input type='number' step='0.1' id='m%d' name='m%d' placeholder='0' value='{%d'><br/>";



void HandleRulesAction(void)
{
    if (!HttpCheckPriviledgedAccess()) { return; }

    AddLog_P(LOG_LEVEL_DEBUG, S_LOG_HTTP, S_CONFIGURE_RULES);

    // must edit and or expand custom mem names here
    // later may have also type def for mems here
    const struct MEM_NAMES {
      char name[10];
      char unit[6];
    } mem_names[6]= {
      [0]={"in_tmp","C"},
      [1]={"out_tmp","C"},
      [2]={"mem3","C"},
      [3]={"mem4","C"},
      [4]={"mem5","C"},
      [5]={"",""},
    };

    if (WebServer->hasArg("save")) {
      RuleSaveSettings();
      HandleConfiguration();
      return;
    }

    String page = FPSTR(HTTP_HEAD);
    page.replace(F("{v}"), FPSTR(D_CONFIGURE_RULES));
    page += FPSTR(HTTP_HEAD_STYLE);
    page += FPSTR(HTTP_FORM_RULES);
    for (uint8_t i=0;i<1;i++) {
      char stemp[256];
      snprintf_P(stemp, sizeof(stemp), HTTP_FORM_RULES1,i+1,i+1,i+1,i+1,i+1,MAX_RULE_SIZE*3,i+1);
      page +=stemp;
    }
    /*
    for (uint8_t i=0;i<MAX_RULE_MEMS;i++) {
      char stemp[128];
      if (!mem_names[i].name) break;
      snprintf_P(stemp, sizeof(stemp), HTTP_FORM_RULES2,mem_names[i].name,mem_names[i].unit,i+1,i+1,i+1,i+1);
      page +=stemp;
    }
    */
    page += FPSTR(HTTP_FORM_END);
    page += FPSTR(HTTP_BTN_CONF);

    //for (uint8_t i=0;i<MAX_RULE_SETS;i++) {
    char *cp=Settings.rules[0];
    for (uint8_t i=0;i<1;i++) {
      char stemp[32];
      snprintf_P(stemp, sizeof(stemp), "xyz%dxyz",i+1);
      page.replace(stemp, cp);
      snprintf_P(stemp, sizeof(stemp), "{r%d",i+1);
      page.replace(stemp, bitRead(Settings.rule_enabled,i) ? F(" checked") : F(""));
    }

    for (uint8_t i=0;i<MAX_RULE_MEMS;i++) {
      char stemp[16];
      snprintf_P(stemp, sizeof(stemp), "{%d",i+1);
      page.replace(stemp, String(Settings.mems[i]));
    }

    //page.replace("\r","\n");

    ShowPage(page);
  }



void RuleSaveSettings(void)
{
  char tmp[MAX_RULE_SIZE*3+2];
  //for (uint8_t i=0;i<MAX_RULE_SETS;i++) {
  for (uint8_t i=0;i<1;i++) {
    char stemp[32];
    snprintf_P(stemp, sizeof(stemp), "c%d",i+1);
    if (WebServer->hasArg(stemp)) {
      bitWrite(Settings.rule_enabled,i,1);
    } else {
      bitWrite(Settings.rule_enabled,i,0);
    }
    snprintf_P(stemp, sizeof(stemp), "t%d",i+1);
    WebGetArg(stemp, tmp, sizeof(tmp));
    if (tmp && *tmp) {
      char *cp=Settings.rules[0];
      String str=tmp;
      str.replace("\r","\n");
      strlcpy(cp,tmp, MAX_RULE_SIZE*3);
    }
  }

  for (uint8_t i=0; i<MAX_RULE_MEMS; i++) {
    char stemp[16];
    snprintf_P(stemp, sizeof(stemp), "m%d",i+1);
    WebGetArg(stemp, tmp, sizeof(tmp));
    if (tmp && *tmp) {
      strlcpy(Settings.mems[i],tmp,sizeof(Settings.mems[i]));
    }
  }

  if (glob_script_mem.script_mem) free(glob_script_mem.script_mem);
  Init_Scripter(Settings.rules[0]);
  Run_Scripter(Settings.rules[0],SECTION_BOOT, 0);
}

#endif
#endif




#define NTYPE 0
#define STYPE 0x40
#define PTYPE 0x80
#define TYPEMASK 0xc0
#define INDMASK 0x3F

#define MAXVARS 50
#define MAXNVARS 40
#define MAXSVARS 10

#define SCRIPT_EOL '\n'



//#define SCRIPT_FLOAT_PRECISION "%.2f"
#define SCRIPT_FLOAT_PRECISION 2

#define SCRIPT_SKIP_SPACES while (*lp==' ' || *lp=='\t') lp++;

// allocates all variable and presets them
int16_t Init_Scripter(char *script) {
    // scan lines for >INIT
    uint16_t lines=0,nvars=0,svars=0,vars=0;
    char *lp=script;
    char vnames[MAXVARS*10];
    char *vnames_p=vnames;
    char *vnp[MAXVARS];
    char **vnp_p=vnp;
    char strings[MAXSVARS*10];
    char *strings_p=strings;
    char *snp[MAXSVARS];
    char **snp_p=snp;
    char error=0;

    float fvalues[MAXVARS];
    uint8_t vtypes[MAXVARS];
    char init=0;
    while (1) {
        // check line
        // skip leading spaces
        SCRIPT_SKIP_SPACES
        // skip empty line
        while (*lp==SCRIPT_EOL) lp++;
        // skip comment
        if (*lp==';') goto next_line;
        if (init) {
            // init section
            if (*lp=='>') {
                init=0;
                break;
            }
            char *op=strchr(lp,'=');
            if (op) {
                // found variable definition
                if (*lp=='p' && *(lp+1)==':') {
                    lp+=2;
                    vtypes[vars]=PTYPE;
                } else {
                    vtypes[vars]=0;
                }
                *vnp_p++=vnames_p;
                while (lp<op) {
                    *vnames_p++=*lp++;
                }
                *vnames_p++=0;
                // init variable
                op++;
                if (*op!='"') {
                    float fv;
                    //sscanf(op,"%f",&fv);
                    fv=CharToDouble(op);
                    fvalues[nvars]=fv;
                    vtypes[vars]|=NTYPE;
                    vtypes[vars]|=nvars;
                    nvars++;
                    if (nvars>MAXNVARS) {
                        error=-1;
                        break;
                    }
                } else {
                    // string vars
                    op++;
                    *snp_p++=strings_p;
                    while (*op!='\"') {
                        *strings_p++=*op++;
                    }
                    *strings_p++=0;
                    vtypes[vars]|=STYPE;
                    vtypes[vars]|=svars;
                    svars++;
                    if (svars>MAXSVARS) {
                        error=-2;
                        break;
                    }
                }
                vars++;
                if (vars>MAXVARS) {
                    error=-3;
                    break;
                }
            }
        } else {
            if (!strncmp(lp,">DEF",4)) {
                init=1;
            }
        }
        // next line
    next_line:
        lp = strchr(lp, SCRIPT_EOL);
        if (!lp) break;
        lp++;
    }
    // now copy vars to memory
    uint16_t script_mem_size=(sizeof(float)*nvars)+
    (vnames_p-vnames)+
    (strings_p-strings)+
    // vars offsets
    (sizeof(uint8_t)*vars)+
    // string offsets
    (sizeof(uint8_t)*svars)+
    // type array
    (sizeof(uint8_t)*vars);

    script_mem_size+=4;
    uint8_t *script_mem;
    script_mem=(uint8_t*)calloc(script_mem_size,1);
    if (!script_mem) return -4;

    glob_script_mem.script_mem=script_mem;
    glob_script_mem.script_mem_size=script_mem_size;

    // now copy all vars
    // numbers
    glob_script_mem.fvars=(float*)script_mem;
    uint16_t size=sizeof(float)*nvars;
    memcpy(script_mem,fvalues,size);
    script_mem+=size;

    // var name strings
    char *namep=(char*)script_mem;
    glob_script_mem.glob_vnp=(char*)script_mem;
    size=vnames_p-vnames;
    memcpy(script_mem,vnames,size);
    script_mem+=size;

    glob_script_mem.vnp_offset=(uint8_t*)script_mem;
    size=vars*sizeof(uint8_t);
    script_mem+=size;

    // strings
    char *snamep=(char*)script_mem;
    glob_script_mem.glob_snp=(char*)script_mem;
    size=strings_p-strings;
    memcpy(script_mem,strings,size);
    script_mem+=size;

    glob_script_mem.snp_offset=(uint8_t*)script_mem;
    size=svars*sizeof(uint8_t);
    script_mem+=size;

    // memory types
    glob_script_mem.type=(uint8_t*)script_mem;
    size=sizeof(uint8_t)*vars;
    memcpy(script_mem,vtypes,size);


    // now must recalc memory offsets
    uint16_t count,index=0;
    uint8_t *cp=glob_script_mem.vnp_offset;
    for (count=0; count<vars;count++) {
        *cp++=index;
        while (*namep) {
            index++;
            namep++;
        }
        namep++;
        index++;
    }
    index=0;
    cp=glob_script_mem.snp_offset;
    for (count=0; count<svars;count++) {
        *cp++=index;
        while (*snamep) {
            index++;
            snamep++;
        }
        snamep++;
        index++;
    }

    glob_script_mem.numvars=vars;
    return error;
}


char *isvar(char *lp, uint8_t *vtype,float *fp,char *sp,char *js) {
    uint16_t count,len=0;
    char vname[16];
    for (count=0; count<sizeof(vname); count++) {
        char iob=lp[count];
        if (iob==0 || iob=='\n' || iob=='\r'|| iob=='=' || iob=='+' || iob=='-' || iob=='/' || iob=='*' || iob=='%' || iob==')') {
            vname[count]=0;
            break;
        }
        vname[count]=iob;
        len+=1;
    }

    for (count=0; count<glob_script_mem.numvars; count++) {
        char *cp=glob_script_mem.glob_vnp+glob_script_mem.vnp_offset[count];
        uint8_t slen=strlen(cp);

        if (slen==len && *cp==vname[0]) {
            if (!strncmp(cp,vname,len)) {
                *vtype=glob_script_mem.type[count];
                uint8_t index=*vtype&INDMASK;
                if ((*vtype&STYPE)==0) {
                    if (fp) *fp=glob_script_mem.fvars[index];
                } else {
                    if (sp) strcpy(sp,glob_script_mem.glob_snp+glob_script_mem.snp_offset[index]);;
                }
                return lp+len;
            }
        }
    }

    if (js) {
      // look for json input
      StaticJsonBuffer<1024> jsonBuffer; //stack
      //DynamicJsonBuffer<1024> jsonBuffer; //heap

//{"Time":"2019-04-20T19:00:40","BME280":{"Temperature":23.0,"Humidity":32.3,"Pressure":1014.0},"SGP30":{"eCO2":584,"TVOC":263,"aHumidity":6.5717},"PressureUnit":"hPa","TempUnit":"C"}

      JsonObject &root = jsonBuffer.parseObject(js);
      if (root.success()) {
        char *subtype=strchr(vname,'#');
        if (subtype) {
          *subtype=0;
          subtype++;
          String vn=vname;
          String st=subtype;
          const char* str_value = root[vn][st];
          if (root[vn][st].success()) {
            // return variable value
            *vtype=NTYPE;
            if (fp) *fp=CharToDouble((char*)str_value);
            return lp+len;
          }
        } else {
          String vn=vname;
          const char* str_value = root[vn];
          if (root[vn].success()) {
            // return variable value
            *vtype=NTYPE;
            if (fp) *fp=CharToDouble((char*)str_value);
            return lp+len;
          }
        }
      }
    }
    // check for immediate value
    //if (fp) sscanf(vname,"%f",fp);
    if (fp) *fp=CharToDouble(vname);

    if (isnan(*fp)) {
        *vtype=0xff;
        return lp;
    }
    *vtype=0xfe;
    return lp+len;
}


enum {OPER_EQU=1,OPER_PLS,OPER_MIN,OPER_MUL,OPER_DIV,OPER_PLSEQU,OPER_MINEQU,OPER_MULEQU,OPER_DIVEQU,OPER_EQUEQU,OPER_NOTEQU,OPER_GRTEQU,OPER_LOWEQU,OPER_GRT,OPER_LOW,OPER_PERC};

char *getop(char *lp, uint8_t *operand) {
    switch (*lp) {
        case '=':
            if (*(lp+1)=='=') {
                *operand=OPER_EQUEQU;
                return lp+2;
            } else {
                *operand=OPER_EQU;
                return lp+1;
            }
            break;
        case '+':
            if (*(lp+1)=='=') {
                *operand=OPER_PLSEQU;
                return lp+2;
            } else {
                *operand=OPER_PLS;
                return lp+1;
            }
            break;
        case '-':
            if (*(lp+1)=='=') {
                *operand=OPER_MINEQU;
                return lp+2;
            } else {
                *operand=OPER_MIN;
                return lp+1;
            }
            break;
        case '*':
            if (*(lp+1)=='=') {
                *operand=OPER_MULEQU;
                return lp+2;
            } else {
                *operand=OPER_MUL;
                return lp+1;
            }
            break;
        case '/':
            if (*(lp+1)=='=') {
                *operand=OPER_DIVEQU;
                return lp+2;
            } else {
                *operand=OPER_DIV;
                return lp+1;
            }
            break;
        case '!':
            if (*(lp+1)=='=') {
                *operand=OPER_NOTEQU;
                return lp+2;
            }
            break;
        case '>':
            if (*(lp+1)=='=') {
                *operand=OPER_GRTEQU;
                return lp+2;
            } else {
                *operand=OPER_GRT;
                return lp+1;

            }
            break;
        case '<':
            if (*(lp+1)=='=') {
                *operand=OPER_LOWEQU;
                return lp+2;
            } else {
                *operand=OPER_LOW;
                return lp+1;
            }
            break;
        case '%':
            *operand=OPER_PERC;
            return lp+1;
            break;
    }
    *operand=0;
    return lp;
}


char *GetNumericResult(char *lp,uint8_t lastop,float *fp,char *js) {
uint8_t operand=0;
float fvar1,fvar;
uint8_t vtype;
    while (1) {
        // get 1. value
        if (*lp=='(') {
            lp++;
            lp=GetNumericResult(lp,OPER_EQU,&fvar1,js);
            lp++;
        } else {
            lp=isvar(lp,&vtype,&fvar1,0,js);
        }
        switch (lastop) {
            case OPER_EQU:
                fvar=fvar1;
                break;
            case OPER_PLS:
                fvar+=fvar1;
                break;
            case OPER_MIN:
                fvar-=fvar1;
                break;
            case OPER_MUL:
                fvar*=fvar1;
                break;
            case OPER_DIV:
                fvar/=fvar1;
                break;
            case OPER_PERC:
                fvar=fmod(fvar,fvar1);
                break;
            default:
                break;
        }
        lp=getop(lp,&operand);
        lastop=operand;
        if (!operand) {
            *fp=fvar;
            return lp;
        }
    }
}


// replace vars in cmd %var%
void Replace_Cmd_Vars(char *srcbuf,char *dstbuf,uint16_t dstsize) {
    char *cp;
    uint16_t count;
    uint8_t vtype;
    float fvar;
    cp=srcbuf;
    char string[16];
    for (count=0;count<dstsize;count++) {
        if (*cp=='%') {
            cp++;
            cp=isvar(cp,&vtype,&fvar,string,0);
            if (vtype!=0xff) {
                // found variable as result
                if ((vtype&STYPE)==0) {
                    // numeric result
                    //sprintf(string,SCRIPT_FLOAT_PRECISION,fvar);
                    dtostrfd(fvar,SCRIPT_FLOAT_PRECISION,string);
                } else {
                    // string result
                }
                strcpy(&dstbuf[count],string);
                count+=strlen(string)-1;
                cp++;
            }
        } else {
            dstbuf[count]=*cp;
            if (*cp==0) {
                break;
            }
            cp++;
        }
    }
}


// execute section of scripter
int16_t Run_Scripter(char *script,uint8_t type, char *js) {
    char *lp=script;
    uint8_t vtype=0;
    uint8_t operand,lastop,numeric,if_state=0,if_result,and_or;
    float *dfvar;
    float fvar=0,fvar1;
    glob_script_mem.section=0;

    if (!glob_script_mem.script_mem_size) {
      Init_Scripter(script);
    }

    while (1) {
        // check line
        // skip leading spaces
        SCRIPT_SKIP_SPACES
        // skip empty line
        while (*lp==SCRIPT_EOL) lp++;
        // skip comment
        if (*lp==';') goto next_line;

        if (glob_script_mem.section) {
            // we are in section
            if (*lp=='>') {
                glob_script_mem.section=0;
                break;
            }
            if (!strncmp(lp,"if",2)) {
                lp+=2;
                if_state=1;
                and_or=0;
            } else if (!strncmp(lp,"then",4)) {
                lp+=4;
                if_state=2;
            } else if (!strncmp(lp,"else",4)) {
                lp+=4;
                if_state=3;
            } else if (!strncmp(lp,"endif",5)) {
                if_state=0;
                goto next_line;
            } else if (!strncmp(lp,"or",2)) {
                lp+=2;
                and_or=1;
            } else if (!strncmp(lp,"and",3)) {
                lp+=3;
                and_or=2;
            }

            SCRIPT_SKIP_SPACES

            if (if_state==3 && if_result) goto next_line;
            if (if_state==2 && !if_result) goto next_line;


            if (!strncmp(lp,"=>",2)) {
                // execute cmd
                lp+=2;
                SCRIPT_SKIP_SPACES
                char cmd[128];
                short count;
                for (count=0; count<sizeof(cmd)-1; count++) {
                    if (*lp=='\r' || *lp=='\n') {
                        cmd[count]=0;
                        break;
                    }
                    cmd[count]=*lp++;
                }
                //snprintf_P(log_data, sizeof(log_data), tmp);
                //AddLog(LOG_LEVEL_INFO);
                // replace vars in cmd
                char tmp[128];
                Replace_Cmd_Vars(cmd,tmp,sizeof(tmp));

#if 0
                NSLog(@"%s",tmp);
#else
                snprintf_P(log_data, sizeof(log_data), PSTR("Script: %s performs \"%s\""), "1", tmp);
                AddLog(LOG_LEVEL_INFO);
                ExecuteCommand((char*)tmp, SRC_RULE);
#endif
                goto next_line;
            }

            // check for variable result
            lp=isvar(lp,&vtype,0,0,0);
            if (vtype!=0xff) {
                // found variable as result
                if ((vtype&STYPE)==0) {
                    // numeric result
                    dfvar=&glob_script_mem.fvars[vtype&INDMASK];
                    numeric=1;
                } else {
                    // string result, not yet
                    numeric=0;
                }
            }
            // evaluate operand
            lp=getop(lp,&lastop);
            if (if_state==1) {
                uint8_t res=0xff;
                lp=GetNumericResult(lp,OPER_EQU,&fvar1,js);
                switch (lastop) {
                    case OPER_EQUEQU:
                        res=*dfvar==fvar1;
                        break;
                    case OPER_NOTEQU:
                        res=*dfvar!=fvar1;
                        break;
                    case OPER_LOW:
                        res=*dfvar<fvar1;
                        break;
                    case OPER_LOWEQU:
                        res=*dfvar<=fvar1;
                        break;
                    case OPER_GRT:
                        res=*dfvar>fvar1;
                        break;
                    case OPER_GRTEQU:
                        res=*dfvar>=fvar1;
                        break;
                    default:
                        // error
                        break;
                }
                if (res!=0xff) {
                    if (!and_or) {
                        if_result=res;
                    } else if (and_or==1) {
                        if_result|=res;
                    } else {
                        if_result&=res;
                    }
                }

            } else {
                switch (lastop) {
                    case OPER_EQU:
                        lp=GetNumericResult(lp,OPER_EQU,dfvar,js);
                        break;
                    case OPER_PLSEQU:
                        lp=GetNumericResult(lp,OPER_EQU,&fvar,js);
                        *dfvar+=fvar;
                        break;
                    case OPER_MINEQU:
                        lp=GetNumericResult(lp,OPER_EQU,&fvar,js);
                        *dfvar-=fvar;
                        break;
                    case OPER_MULEQU:
                        lp=GetNumericResult(lp,OPER_EQU,&fvar,js);
                        *dfvar*=fvar;
                        break;
                    case OPER_DIVEQU:
                        lp=GetNumericResult(lp,OPER_EQU,&fvar,js);
                        *dfvar/=fvar;
                        break;
                    default:
                        // error
                        break;
                }
            }

        } else {
            // decode line
            const char *cp;
            switch (type) {
              case SECTION_BOOT:
                cp=">BOOT";
                break;
              case SECTION_TELE:
                cp=">TELE";
                break;
              case SECTION_TIME:
                cp=">TIME";
                break;
              default:
                cp="";
                break;
            }
            if (!strncmp(lp,(const char*)cp,strlen(cp))) {
                // found section
                glob_script_mem.section=type;
            }
        }
        // next line
    next_line:
        lp = strchr(lp, SCRIPT_EOL);
        if (!lp) break;
        lp++;
    }
}



/*********************************************************************************************\
 * Interface
\*********************************************************************************************/

boolean Xdrv10(byte function)
{
  boolean result = false;

  switch (function) {
    case FUNC_PRE_INIT:
      RulesInit();
      //Init_Scripter(Settings.rules[0]);
      break;
    case FUNC_EVERY_50_MSECOND:
      RulesEvery50ms();
      break;
    case FUNC_EVERY_100_MSECOND:
      RulesEvery100ms();
      break;
    case FUNC_EVERY_SECOND:
      RulesEverySecond();
      break;
    case FUNC_SET_POWER:
      RulesSetPower();
      break;
    case FUNC_COMMAND:
      result = RulesCommand();
      break;
    case FUNC_RULES_PROCESS:
      result = RulesProcess();
      break;
#ifdef USE_WEBSERVER
#ifdef USE_RULES_GUI
    case FUNC_WEB_ADD_BUTTON:
      strncat_P(mqtt_data, HTTP_BTN_MENU_RULES, sizeof(mqtt_data) - strlen(mqtt_data) -1);
      break;
    case FUNC_WEB_ADD_HANDLER:
      WebServer->on("/" WEB_HANDLE_RULES, HandleRulesAction);
      break;
#endif  // USE_RULES_GUI
#endif // USE_WEBSERVER
  }
  return result;
}

#endif  // USE_RULES
