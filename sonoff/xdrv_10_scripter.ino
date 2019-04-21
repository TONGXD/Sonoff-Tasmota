/*
  xdrv_10_scripter.ino - script support for Sonoff-Tasmota

  Copyright (C) 2019  Gerhard Mutz and Theo Arends

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


/* example program, p: means permanent
>DEF
hello="hello world"
string="xxx"
p:mintmp=10
p:maxtmp=30
hum=0
temp=0
timer=0
dimmer=0
sw=0

>BOOT
=>print BOOT executed

>TELE
hum=BME280#Humidity
temp=BME280#Temperature
sw=Switch1
=> power1 %sw%

if temp>30
and hum>70
then print damn hot!
endif

; math => +,-,*,/,% left, right evaluation with optional brackets
;e.g. temp=hum*(100/37.5)+temp-(timer*hum%10)

>TIME
timer+=1
if timer>=5
then =>print timer=%timer%
timer=0
endif

dimmer+=1
if dimmer>100
then dimmer=0
endif
dimmer %dimmer%

special vars:

upsecs = seconds since start
uptime = minutes since start
time = minutes since midnight
sunrise = sunrise minutes since midnight
sunset = sunset minutes since midnight
tstamp = timestamp (local date and time)
*/


#ifdef USE_RULES


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

void ScriptEverySecond(void) {
  if (bitRead(Settings.rule_enabled, 0)) Run_Scripter(Settings.rules[0],SECTION_TIME,0);
}

void RulesTeleperiod(void) {
  if (bitRead(Settings.rule_enabled, 0)) Run_Scripter(Settings.rules[0],SECTION_TELE, mqtt_data);
}


#ifdef USE_WEBSERVER

#define WEB_HANDLE_RULES "s10"
#define D_CONFIGURE_RULES "Edit script"
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

  if (glob_script_mem.script_mem) free(glob_script_mem.script_mem);
  if (bitRead(Settings.rule_enabled, 0)) {
    Init_Scripter(Settings.rules[0]);
    Run_Scripter(Settings.rules[0],SECTION_BOOT, 0);
  }
}

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
    // scan lines for >DEF
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

// vtype => ff=nothing found, fe=number, 40 = string, 0 = number
char *isvar(char *lp, uint8_t *vtype,float *fp,char *sp,char *js) {
    uint16_t count,len=0;
    char vname[32];
    for (count=0; count<sizeof(vname); count++) {
        char iob=lp[count];
        if (iob==0 || iob=='\n' || iob=='\r'|| iob=='=' || iob=='+' || iob=='-' || iob=='/' || iob=='*' || iob=='%' || iob==')' || iob=='>' || iob=='<' || iob=='!') {
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
                    if (sp) strcpy(sp,glob_script_mem.glob_snp+glob_script_mem.snp_offset[index]);
                }
                return lp+len;
            }
        }
    }

    if (js) {
      // look for json input
      StaticJsonBuffer<1024> jsonBuffer; //stack
      //DynamicJsonBuffer<1024> jsonBuffer; //heap
      char jsbuff[strlen(js)+1];
      strcpy(jsbuff,js);

      JsonObject &root = jsonBuffer.parseObject(jsbuff);

//{"Time":"2019-04-20T19:00:40","BME280":{"Temperature":23.0,"Humidity":32.3,"Pressure":1014.0},"SGP30":{"eCO2":584,"TVOC":263,"aHumidity":6.5717},"PressureUnit":"hPa","TempUnit":"C"}
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
            if (*fp) {
              if (!strncmp(str_value,"ON",2)) {
                *fp=1;
              } else if (!strncmp(str_value,"OFF",3)) {
                *fp=0;
              } else {
                *fp=CharToDouble((char*)str_value);
              }
            }
            *vtype=NTYPE;
            return lp+len;
          }
        }
      }
    }

    float fvar;

    // check for special vars
    if (!strncmp(vname,"uptime",6)) {
      fvar=GetMinutesUptime();
      goto exit;
    }
    if (!strncmp(vname,"upsecs",6)) {
      fvar=uptime;
      goto exit;
    }
    if (!strncmp(vname,"time",4)) {
      fvar=GetMinutesPastMidnight();
      goto exit;
    }
    if (!strncmp(vname,"time",4)) {
      fvar=GetMinutesPastMidnight();
      goto exit;
    }

#if defined(USE_TIMERS) && defined(USE_SUNRISE)
    if (!strncmp(vname,"sunrise",7)) {
      fvar=GetSunMinutes(0);
      goto exit;
    }
    if (!strncmp(vname,"sunset",6)) {
      fvar=GetSunMinutes(1);
      goto exit;
    }
#endif

    if (!strncmp(vname,"tstamp",6)) {
      if (sp) strcpy(sp,GetDateAndTime(DT_LOCAL).c_str());
      *vtype=STYPE;
      return lp+len;
    }

    // check for immediate value
    //if (fp) sscanf(vname,"%f",fp);
    fvar=CharToDouble(vname);
    if (isnan(fvar)) {
      if (fp) *fp=0;
      *vtype=0xff;
      return lp;
    }
exit:
    if (fp) *fp=fvar;
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
    char string[32];
    for (count=0;count<dstsize;count++) {
        if (*cp=='%') {
            cp++;
            cp=isvar(cp,&vtype,&fvar,string,0);
            if (vtype!=0xff) {
                // found variable as result
                if (vtype==0xfe || (vtype&STYPE)==0) {
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

void toLog(const char *str) {
  if (!str) return;
  snprintf_P(log_data, sizeof(log_data), PSTR("%s"),str);
  AddLog(LOG_LEVEL_INFO);
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
                if (!strncmp(tmp,"print",5)) {
                  toLog(&tmp[5]);
                } else {
                  snprintf_P(log_data, sizeof(log_data), PSTR("Script: %s performs \"%s\""), "1", tmp);
                  AddLog(LOG_LEVEL_INFO);
                  ExecuteCommand((char*)tmp, SRC_RULE);
                }
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
                        res=(*dfvar==fvar1);
                        break;
                    case OPER_NOTEQU:
                        res=(*dfvar!=fvar1);
                        break;
                    case OPER_LOW:
                        res=(*dfvar<fvar1);
                        break;
                    case OPER_LOWEQU:
                        res=(*dfvar<=fvar1);
                        break;
                    case OPER_GRT:
                        res=(*dfvar>fvar1);
                        break;
                    case OPER_GRTEQU:
                        res=(*dfvar>=fvar1);
                        break;
                    default:
                        // error
                        break;
                }

                if (!and_or) {
                    if_result=res;
                } else if (and_or==1) {
                    if_result|=res;
                } else {
                    if_result&=res;
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
      if (bitRead(Settings.rule_enabled, 0)) {
        Init_Scripter(Settings.rules[0]);
        Run_Scripter(Settings.rules[0],SECTION_BOOT, 0);
      }
      break;
    case FUNC_EVERY_SECOND:
      ScriptEverySecond();
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
