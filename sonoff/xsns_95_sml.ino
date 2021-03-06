/*
>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
EXPERIMENTAL VERSION  unterstützt im Prinzip mehrere Zähler
da durch die begrenzte Hardwareunterstützung das software serial nicht optimal funktioniert
ist es mit der originalen TasmotaSerial nicht möglich 3 Zähler gleichzeitig abzufragen

durch Modifikation des Tasmota Serial Drivers sollten jetzt auch mehr als 2 Zähler
funktionieren. Dazu muss auch die modifizierte TasmotaSerial-2.3.0 ebenfalls kopiert werden

jetzt auch mit Unterstützung für Gas und Wasserzähler
Zähler setzen mit Sensor95 c1 xxx, Sensor95 c2 xxx etc

nur dieser Treiber wird in Zukunft weiterentwickelt
die älteren werden nicht mehr unterstützt.

>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

  xsns_95_sml.ino - SML smart meter interface for Sonoff-Tasmota

  Created by Gerhard Mutz on 07.10.11.
  adapted for Tasmota

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

#ifdef USE_SML_M

#define XSNS_95 95

// Baudrate des D0 Ausgangs, sollte bei den meisten Zählern 9600 sein
#define SML_BAUDRATE 9600

// sende dies alle N Sekunden, für Zähler die erst auf Anforderung etwas senden
//#define SML_SEND_SEQ

// debug counter input to led for counter1 and 2
//#define DEBUG_CNT_LED1 2
//#define DEBUG_CNT_LED1 2

// use analog optical counter sensor with AD Converter ADS1115
//#define ANALOG_OPTO_SENSOR
// fototransistor mit pullup an A0, A1 des ADS1115 A3 and +3.3V
// die pegel und die verstärkung können dann automatisch kalibriert werden


// support für mehr als 2 Meter mit spezieller Tasmota Serial Version
// dazu muss der modifizierte Treiber => TasmotaSerial-2.3.0 ebenfalls kopiert werden

#include <TasmotaSerial.h>

// speziellen angepassten Tasmota seriell Treiber benutzen
#define SPECIAL_SS

// diese Version verwendet den serial REC Pin des ESP, und zusätzliche GPIO
// pins als Software serial
// und kann mit jeder aktuellen Version von Tasmota kombiniert werden
// dazu muss z.B. in der user_config_override #define USE_SML_M angegeben werden
// Als "Lesekopf" kann ein Fototransistor (z.B. TEKT5400 oder BPW78A) zwischen
// Masse und dem ESP REC pin verwendet werden. Eventuell ist ein zusätzlicher
// Pullup Widerstand zwischen REC und VCC 3.3 Volt erforderlich (1-4.7kOhm)

// max 23 Zeichen
#if DMY_LANGUAGE==de-DE
// deutsche Bezeichner
#define D_TPWRIN "Verbrauch"
#define D_TPWROUT "Einspeisung"
#define D_TPWRCURR "Aktueller Verbrauch"
#define D_TPWRCURR1 "Verbrauch P1"
#define D_TPWRCURR2 "Verbrauch P2"
#define D_TPWRCURR3 "Verbrauch P3"
#define D_Strom_L1 "Strom L1"
#define D_Strom_L2 "Strom L2"
#define D_Strom_L3 "Strom L3"
#define D_Spannung_L1 "Spannung L1"
#define D_Spannung_L2 "Spannung L2"
#define D_Spannung_L3 "Spannung L3"
#define D_METERNR "Zähler Nr"
#define D_METERSID "Service ID"
#define D_GasIN "Zählerstand"                // Gas-Verbrauch
#define D_H2oIN "Zählerstand"                // H2o-Verbrauch
#define D_StL1L2L3 "Ströme L1+L2+L3"
#define D_SpL1L2L3 "Spannung L1+L2+L3/3"

#else
// alle anderen Sprachen
#define D_TPWRIN "Total-In"
#define D_TPWROUT "Total-Out"
#define D_TPWRCURR "Current-In/Out"
#define D_TPWRCURR1 "Current-In p1"
#define D_TPWRCURR2 "Current-In p2"
#define D_TPWRCURR3 "Current-In p3"
#define D_Strom_L1 "Current L1"
#define D_Strom_L2 "Current L2"
#define D_Strom_L3 "Current L3"
#define D_Spannung_L1 "Voltage L1"
#define D_Spannung_L2 "Voltage L2"
#define D_Spannung_L3 "Voltage L3"
#define D_METERNR "Meter_number"
#define D_METERSID "Service ID"
#define D_GasIN "Counter"                // Gas-Verbrauch
#define D_H2oIN "Counter"                // H2o-Verbrauch
#define D_StL1L2L3 "Current L1+L2+L3"
#define D_SpL1L2L3 "Voltage L1+L2+L3/3"

#endif

// JSON Strings besser NICHT übersetzen
// max 23 Zeichen
#define DJ_TPWRIN "Total_in"
#define DJ_TPWROUT "Total_out"
#define DJ_TPWRCURR "Power_curr"
#define DJ_TPWRCURR1 "Power_p1"
#define DJ_TPWRCURR2 "Power_p2"
#define DJ_TPWRCURR3 "Power_p3"
#define DJ_CURR1 "Curr_p1"
#define DJ_CURR2 "Curr_p2"
#define DJ_CURR3 "Curr_p3"
#define DJ_VOLT1 "Volt_p1"
#define DJ_VOLT2 "Volt_p2"
#define DJ_VOLT3 "Volt_p3"
#define DJ_METERNR "Meter_number"
#define DJ_METERSID "Meter_id"
#define DJ_CSUM "Curr_summ"
#define DJ_VAVG "Volt_avg"
#define DJ_COUNTER "Count"

struct METER_DESC {
  uint8_t srcpin;
  uint8_t type;
  uint16_t flag;
  uint16_t params;
  char prefix[8];
};

// Zählerliste , hier neue Zähler eintragen
//=====================================================
#define EHZ161_0 1
#define EHZ161_1 2
#define EHZ363 3
#define EHZH 4
#define EDL300 5
#define Q3B 6
#define COMBO3 7
#define COMBO2 8
#define COMBO3a 9
#define Q3B_V1 10
#define EHZ363_2 11
#define COMBO3b 12
#define WGS_COMBO 13

// diesen Zähler auswählen
#define METER EHZ363

//=====================================================
// Einträge in Liste
// erster Eintrag = laufende Zählernummer mit Komma getrennt
// danach bis @ Zeichen => Sequenz von OBIS als ASCI, oder SML als HEX ASCI
// Skalierungsfaktor (Divisor) (kann auch negativ sein oder kleiner 0 z.B. 0.1 => Ergebnis * 10)
// statt des Skalierungsfaktors kann hier (nur in einer Zeile) ein # Zeichen stehen (OBIS, (SML Hager))
// in diesem Fall wird ein String (keine Zahl) ausgelesen (z.B. Zähler ID)
// nach dem # Zeichen muss ein Abschlusszeichen angegeben werden, also bei OBIS ein ) Zeichen
// Name des Signals in WEBUI (max 23 Zeichen)
// Einheit des Signals in WEBUI (max 7 Zeichen)
// Name des Signals in MQTT Nachricht (max 23 Zeichen)
// Anzahl der Nachkommastellen, wird hier 16 addiert wird sofort ein MQTT für diesen Wert ausgelöst, nicht erst bei teleperiod
// Beispiel: => "1-0:2.8.0*255(@1,Einspeisung,KWh,Solar_Feed,4|"
// in allen ausser der letzten Zeile muss ein | Zeichen am Ende der Zeile stehen.
// Nur am Ende der letzen Zeile steht ein Semikolon.
// max 16 Zeilen
// =====================================================
// steht in der Sequenz ein = Zeichen am Anfang kann folgender Eintrag definiert werden:
// =m => mathe berechne Werte z.B. =m 3+4+5  addiert die Ergebnisse aus den Zeilen 3,4 und 5
// + - / * werden unterstützt  das #Zeichen  bezeichnet eine Konstante  /#3 => geteilt durch 3
// damit kann z.B. die Summe aus 3 Phasen berechnet werden
// =d => differenz berechne Differenzwerte über die Zeit aus dem Ergebnis der Zeile
// z.B. =d 3 10 berechnet die Differenz nach jeweils 10 Sekunden des Ergebnisses aus Zeile 3
// damit kann z.B. der Momentanverbrauch aus dem Gesamtverbrauch berechnet werden, falls der Zähler das nicht direkt ausgibt
// =h => html Text Zeile (max 30 Zeichen) in WEBUI einfügen, diese Zeile zählt nicht bei Zeilenreferenzen

// der METER_DESC beschreibt die Zähler
// METERS_USED muss auf die Anzahl der benutzten Zähler gesetzt werden
// entsprechend viele Einträge muss der METER_DESC dann haben (für jeden Zähler einen)
// 1. srcpin der pin für den seriellen input 0 oder 3 => RX pin, ansonsten software serial GPIO pin
// 2. type o=obis, s=sml, c=COUNTER (z.B. Gaszähler reed Kontakt)
// 3. flag wenn 0 dann counter ohne Pullup, 1=mit Pullup, 2=beutze AD Konverter ADS1115 an i2c Schnittstelle
// 4. params Baudrate bei serieller Schnittstelle, bei counter poll time in Millisekunden, bei ADS1115 Abtastrate
// 5. json prefix max 7 Zeichen, kann im Prinzip frei gesetzt werden
// dieses Prefix wird sowohl in der Web Anzeige als auch in der MQTT Nachricht vorangestellt

#if METER==EHZ161_0
#define METERS_USED 1
struct METER_DESC const meter_desc[METERS_USED]={
  [0]={0,'o',0,SML_BAUDRATE,"OBIS"}};
const uint8_t meter[]=
"1,1-0:1.8.0*255(@1," D_TPWRIN ",KWh," DJ_TPWRIN ",4|"
"1,1-0:2.8.0*255(@1," D_TPWROUT ",KWh," DJ_TPWROUT ",4|"
"1,1-0:21.7.0*255(@1," D_TPWRCURR1 ",W," DJ_TPWRCURR1 ",0|"
"1,1-0:41.7.0*255(@1," D_TPWRCURR2 ",W," DJ_TPWRCURR2 ",0|"
"1,1-0:61.7.0*255(@1," D_TPWRCURR3 ",W," DJ_TPWRCURR3 ",0|"
"1,=m 3+4+5 @1," D_TPWRCURR ",W," DJ_TPWRCURR ",0|"
"1,1-0:0.0.0*255(@#)," D_METERNR ",," DJ_METERNR ",0";

#endif

//=====================================================

#if METER==EHZ161_1
#define METERS_USED 1
struct METER_DESC const meter_desc[METERS_USED]={
  [0]={0,'o',0,SML_BAUDRATE,"OBIS"}};
const uint8_t meter[]=
"1,1-0:1.8.1*255(@1," D_TPWRIN ",KWh," DJ_TPWRIN ",4|"
"1,1-0:2.8.1*255(@1," D_TPWROUT ",KWh," DJ_TPWROUT ",4|"
"1,=d 2 10 @1," D_TPWRCURR ",W," DJ_TPWRCURR ",0|"
"1,1-0:0.0.0*255(@#)," D_METERNR ",," DJ_METERNR ",0";
#endif

//=====================================================

#if METER==EHZ363
#define METERS_USED 1
struct METER_DESC const meter_desc[METERS_USED]={
  [0]={0,'s',0,SML_BAUDRATE,"SML"}};
// 2 Richtungszähler EHZ SML 8 bit 9600 baud, binär
const uint8_t meter[]=
//0x77,0x07,0x01,0x00,0x01,0x08,0x00,0xff
"1,77070100010800ff@1000," D_TPWRIN ",KWh," DJ_TPWRIN ",4|"
//0x77,0x07,0x01,0x00,0x02,0x08,0x00,0xff
"1,77070100020800ff@1000," D_TPWROUT ",KWh," DJ_TPWROUT ",4|"
//0x77,0x07,0x01,0x00,0x10,0x07,0x00,0xff
"1,77070100100700ff@1," D_TPWRCURR ",W," DJ_TPWRCURR ",0|"
//0x77,0x07,0x01,0x00,0x00,0x00,0x09,0xff
"1,77070100000009ff@#," D_METERNR ",," DJ_METERNR ",0";
#endif

//=====================================================

#if METER==EHZH
#define METERS_USED 1
struct METER_DESC const meter_desc[METERS_USED]={
  [0]={0,'s',0,SML_BAUDRATE,"SML"}};
// 2 Richtungszähler EHZ SML 8 bit 9600 baud, binär
// verbrauch total
const uint8_t meter[]=
//0x77,0x07,0x01,0x00,0x01,0x08,0x00,0xff
"1,77070100010800ff@1000," D_TPWRIN ",KWh," DJ_TPWRIN ",4|"
//0x77,0x07,0x01,0x00,0x01,0x08,0x01,0xff
"1,77070100020800ff@1000," D_TPWROUT ",KWh," DJ_TPWROUT ",4|"
//0x77,0x07,0x01,0x00,0x0f,0x07,0x00,0xff
"1,770701000f0700ff@1," D_TPWRCURR ",W," DJ_TPWRCURR ",0";
#endif

//=====================================================

#if METER==EDL300
#define METERS_USED 1
struct METER_DESC const meter_desc[METERS_USED]={
  [0]={0,'s',0,SML_BAUDRATE,"SML"}};
// 2 Richtungszähler EHZ SML 8 bit 9600 baud, binär
// verbrauch total
const uint8_t meter[]=
//0x77,0x07,0x01,0x00,0x01,0x08,0x00,0xff
"1,77070100010800ff@1000," D_TPWRIN ",KWh," DJ_TPWRIN ",4|"
//0x77,0x07,0x01,0x00,0x01,0x08,0x01,0xff
"1,77070100020801ff@1000," D_TPWROUT ",KWh," DJ_TPWROUT ",4|"
//0x77,0x07,0x01,0x00,0x0f,0x07,0x00,0xff
"1,770701000f0700ff@1," D_TPWRCURR ",W," DJ_TPWRCURR ",0";
#endif

//=====================================================

#if METER==Q3B
#define METERS_USED 1
struct METER_DESC const meter_desc[METERS_USED]={
  [0]={0,'s',0,SML_BAUDRATE,"SML"}};
const uint8_t meter[]=
//0x77,0x07,0x01,0x00,0x01,0x08,0x01,0xff
"1,77070100010800ff@100," D_TPWRIN ",KWh," DJ_TPWRIN ",4|"
//0x77,0x07,0x01,0x00,0x02,0x08,0x01,0xff
"1,77070100020801ff@100," D_TPWROUT ",KWh," DJ_TPWROUT ",4|"
//0x77,0x07,0x01,0x00,0x01,0x07,0x00,0xff
"1,77070100010700ff@100," D_TPWRCURR ",W," DJ_TPWRCURR ",0";
#endif

#if METER==COMBO3
// 3 Zähler Beispiel
#define METERS_USED 3

struct METER_DESC const meter_desc[METERS_USED]={
  [0]={3,'o',0,SML_BAUDRATE,"OBIS"}, // harware serial RX pin
  [1]={14,'s',0,SML_BAUDRATE,"SML"}, // GPIO14 software serial
  [2]={4,'o',0,SML_BAUDRATE,"OBIS2"}}; // GPIO4 software serial

// 3 Zähler definiert
const uint8_t meter[]=
"1,1-0:1.8.0*255(@1," D_TPWRIN ",KWh," DJ_TPWRIN ",4|"
"1,1-0:2.8.0*255(@1," D_TPWROUT ",KWh," DJ_TPWROUT ",4|"
"1,1-0:21.7.0*255(@1," D_TPWRCURR1 ",W," DJ_TPWRCURR1 ",0|"
"1,1-0:41.7.0*255(@1," D_TPWRCURR2 ",W," DJ_TPWRCURR2 ",0|"
"1,1-0:61.7.0*255(@1," D_TPWRCURR3 ",W," DJ_TPWRCURR3 ",0|"
"1,=m 3+4+5 @1," D_TPWRCURR ",W," DJ_TPWRCURR ",0|"
"1,1-0:0.0.0*255(@#)," D_METERNR ",," DJ_METERNR ",0|"
"2,77070100010800ff@1000," D_TPWRIN ",KWh," DJ_TPWRIN ",4|"
"2,77070100020800ff@1000," D_TPWROUT ",KWh," DJ_TPWROUT ",4|"
"2,77070100100700ff@1," D_TPWRCURR ",W," DJ_TPWRCURR ",0|"
"3,1-0:1.8.1*255(@1," D_TPWRIN ",KWh," DJ_TPWRIN ",4|"
"3,1-0:2.8.1*255(@1," D_TPWROUT ",KWh," DJ_TPWROUT ",4|"
"3,=d 2 10 @1," D_TPWRCURR ",W," DJ_TPWRCURR ",0|"
"3,1-0:0.0.0*255(@#)," D_METERNR ",," DJ_METERNR ",0";

#endif

#if METER==COMBO2
// 2 Zähler Beispiel
#define METERS_USED 2

struct METER_DESC const meter_desc[METERS_USED]={
  [0]={3,'o',0,SML_BAUDRATE,"OBIS1"}, // harware serial RX pin
  [1]={14,'o',0,SML_BAUDRATE,"OBIS2"}}; // GPIO14 software serial

// 2 Zähler definiert
const uint8_t meter[]=
"1,1-0:1.8.1*255(@1," D_TPWRIN ",KWh," DJ_TPWRIN ",4|"
"1,1-0:2.8.1*255(@1," D_TPWROUT ",KWh," DJ_TPWROUT ",4|"
"1,=d 2 10 @1," D_TPWRCURR ",W," DJ_TPWRCURR ",0|"
"1,1-0:0.0.0*255(@#)," D_METERNR ",," DJ_METERNR ",0|"

"2,1-0:1.8.1*255(@1," D_TPWRIN ",KWh," DJ_TPWRIN ",4|"
"2,1-0:2.8.1*255(@1," D_TPWROUT ",KWh," DJ_TPWROUT ",4|"
"2,=d 6 10 @1," D_TPWRCURR ",W," DJ_TPWRCURR ",0|"
"2,1-0:0.0.0*255(@#)," D_METERNR ",," DJ_METERNR ",0";

#endif

#if METER==COMBO3a
#define METERS_USED 3

struct METER_DESC const meter_desc[METERS_USED]={
  [0]={3,'o',0,SML_BAUDRATE,"OBIS1"}, // harware serial RX pin
  [1]={14,'o',0,SML_BAUDRATE,"OBIS2"},
  [2]={1,'o',0,SML_BAUDRATE,"OBIS3"}};

// 3 Zähler definiert
const uint8_t meter[]=
"1,=h --- Zähler Nr 1 ---|"
"1,1-0:1.8.1*255(@1," D_TPWRIN ",KWh," DJ_TPWRIN ",4|"
"1,1-0:2.8.1*255(@1," D_TPWROUT ",KWh," DJ_TPWROUT ",4|"
"1,=d 2 10 @1," D_TPWRCURR ",W," DJ_TPWRCURR ",0|"
"1,1-0:0.0.0*255(@#)," D_METERNR ",," DJ_METERNR ",0|"
"2,=h --- Zähler Nr 2 ---|"
"2,1-0:1.8.1*255(@1," D_TPWRIN ",KWh," DJ_TPWRIN ",4|"
"2,1-0:2.8.1*255(@1," D_TPWROUT ",KWh," DJ_TPWROUT ",4|"
"2,=d 6 10 @1," D_TPWRCURR ",W," DJ_TPWRCURR ",0|"
"2,1-0:0.0.0*255(@#)," D_METERNR ",," DJ_METERNR ",0|"
"3,=h --- Zähler Nr 3 ---|"
"3,1-0:1.8.1*255(@1," D_TPWRIN ",KWh," DJ_TPWRIN ",4|"
"3,1-0:2.8.1*255(@1," D_TPWROUT ",KWh," DJ_TPWROUT ",4|"
"3,=d 10 10 @1," D_TPWRCURR ",W," DJ_TPWRCURR ",0|"
"3,1-0:0.0.0*255(@#)," D_METERNR ",," DJ_METERNR ",0";

#endif

//=====================================================

#if METER==Q3B_V1
#define METERS_USED 1
struct METER_DESC const meter_desc[METERS_USED]={
[0]={0,'o',0,SML_BAUDRATE,"OBIS"}};
const uint8_t meter[]=
"1,1-0:1.8.1*255(@1," D_TPWRIN ",KWh," DJ_TPWRIN ",4|"
"1,=d 1 10 @1," D_TPWRCURR ",W," DJ_TPWRCURR ",0|"
"1,1-0:0.0.0*255(@#)," D_METERNR ",," DJ_METERNR ",0";
#endif

//=====================================================

#if METER==EHZ363_2
#define METERS_USED 1
struct METER_DESC const meter_desc[METERS_USED]={
[0]={0,'s',0,SML_BAUDRATE,"SML"}};
// 2 Richtungszähler EHZ SML 8 bit 9600 baud, binär
const uint8_t meter[]=
//0x77,0x07,0x01,0x00,0x01,0x08,0x00,0xff
"1,77070100010800ff@1000," D_TPWRIN ",KWh," DJ_TPWRIN ",4|"
//0x77,0x07,0x01,0x00,0x02,0x08,0x00,0xff
"1,77070100020800ff@1000," D_TPWROUT ",KWh," DJ_TPWROUT ",4|"
//0x77,0x07,0x01,0x00,0x01,0x08,0x01,0xff
"1,77070100010801ff@1000," D_TPWRCURR1 ",KWh," DJ_TPWRCURR1 ",4|"
//0x77,0x07,0x01,0x00,0x01,0x08,0x02,0xff
"1,77070100010802ff@1000," D_TPWRCURR2 ",KWh," DJ_TPWRCURR2 ",4|"
//0x77,0x07,0x01,0x00,0x10,0x07,0x00,0xff
"1,77070100100700ff@1," D_TPWRCURR ",W," DJ_TPWRCURR ",0|"
//0x77,0x07,0x01,0x00,0x00,0x00,0x09,0xff
"1,77070100000009ff@#," D_METERNR ",," DJ_METERNR ",0";
#endif

// Beispiel für einen OBIS Stromzähler und einen Gaszähler + Wasserzähler
#if METER==COMBO3b
#define METERS_USED 3
struct METER_DESC const meter_desc[METERS_USED]={
  [0]={3,'o',0,SML_BAUDRATE,"OBIS"}, // harware serial RX pin
  [1]={14,'c',0,50,"Gas"}, // GPIO14 gas counter
  [2]={1,'c',0,10,"Wasser"}}; // water counter

// 3 Zähler definiert
const uint8_t meter[]=
"1,1-0:1.8.1*255(@1," D_TPWRIN ",KWh," DJ_TPWRIN ",4|"
"1,1-0:2.8.1*255(@1," D_TPWROUT ",KWh," DJ_TPWROUT ",4|"
"1,=d 2 10 @1," D_TPWRCURR ",W," DJ_TPWRCURR ",0|"
"1,1-0:0.0.0*255(@#)," D_METERNR ",," DJ_METERNR ",0|"

// bei gaszählern (countern) muss der Vergleichsstring so aussehen wie hier
"2,1-0:1.8.0*255(@100," D_GasIN ",cbm," DJ_COUNTER ",2|"

"3,1-0:1.8.0*255(@100," D_H2oIN ",cbm," DJ_COUNTER ",2";
#endif


#if METER==WGS_COMBO
#define METERS_USED 3

struct METER_DESC const meter_desc[METERS_USED]={
  [0]={1,'c',0,10,"H20"}, // GPIO1 Wasser Zähler
  [1]={4,'c',0,50,"GAS"}, // GPIO4 gas Zähler
  [2]={3,'s',0,SML_BAUDRATE,"SML"}}; // SML harware serial RX pin

const uint8_t meter[]=
//----------------------------Wasserzähler--sensor95 c1------------------------------------
//"1,=h==================|"
"1,1-0:1.8.0*255(@10000," D_H2oIN ",cbm," DJ_COUNTER ",4|"            // 1
//----------------------------Gaszähler-----sensor95 c2------------------------------------
// bei gaszählern (countern) muss der Vergleichsstring so aussehen wie hier
"2,=h==================|"
"2,1-0:1.8.0*255(@100," D_GasIN ",cbm," DJ_COUNTER ",3|"              // 2
//----------------------------Stromzähler-EHZ363W5--sensor95 d0----------------------------
"3,=h==================|"
//0x77,0x07,0x01,0x00,0x01,0x08,0x00,0xff
"3,77070100010800ff@1000," D_TPWRIN ",KWh," DJ_TPWRIN ",3|"         // 3  Zählerstand Total
"3,=h==================|"
//0x77,0x07,0x01,0x00,0x10,0x07,0x00,0xff
"3,77070100100700ff@1," D_TPWRCURR ",W," DJ_TPWRCURR ",2|"          // 4  Aktuelle Leistung
"3,=h -------------------------------|"
"3,=m 10+11+12 @100," D_StL1L2L3 ",A," DJ_CSUM ",2|"            // 5  Summe Aktuelle Ströme
//"3,=h -------------------------------|"
"3,=m 13+14+15/#3 @100," D_SpL1L2L3 ",V," DJ_VAVG ",2|"      // 6   Mittelwert Spannungen
"3,=h==================|"
//0x77,0x07,0x01,0x00,0x24,0x07,0x00,0xff
"3,77070100240700ff@1," D_TPWRCURR1 ",W," DJ_TPWRCURR1 ",2|"        // 7  Wirkleistung L1
//0x77,0x07,0x01,0x00,0x38,0x07,0x00,0xff
"3,77070100380700ff@1," D_TPWRCURR2 ",W," DJ_TPWRCURR1 ",2|"        // 8  Wirkleistung L2
//0x77,0x07,0x01,0x00,0x4c,0x07,0x00,0xff
"3,770701004c0700ff@1," D_TPWRCURR3 ",W," DJ_TPWRCURR1 ",2|"        // 9  Wirkleistung L3
"3,=h -------------------------------|"
//0x77,0x07,0x01,0x00,0x1f,0x07,0x00,0xff
"3,770701001f0700ff@100," D_Strom_L1 ",A," DJ_CURR1 ",2|"        // 10 Strom L1
//0x77,0x07,0x01,0x00,0x33,0x07,0x00,0xff
"3,77070100330700ff@100," D_Strom_L2 ",A," DJ_CURR2 ",2|"        // 11 Strom L2
//0x77,0x07,0x01,0x00,0x47,0x07,0x00,0xff
"3,77070100470700ff@100," D_Strom_L3 ",A," DJ_CURR3 ",2|"        // 12 Strom L3
"3,=h -------------------------------|"
//0x77,0x07,0x01,0x00,0x20,0x07,0x00,0xff
"3,77070100200700ff@100," D_Spannung_L1 ",V," DJ_VOLT1 ",2|"  // 13 Spannung L1
//0x77,0x07,0x01,0x00,0x34,0x07,0x00,0xff
"3,77070100340700ff@100," D_Spannung_L2 ",V," DJ_VOLT2 ",2|"  // 14 Spannung L2
//0x77,0x07,0x01,0x00,0x48,0x07,0x00,0xff
"3,77070100480700ff@100," D_Spannung_L3 ",V," DJ_VOLT3 ",2|"  // 15 Spannung L3
"3,=h==================|"
//0x77,0x07,0x01,0x00,0x00,0x00,0x09,0xff
"3,77070100000009ff@#," D_METERSID ",," DJ_METERSID ",0|"           // 16 Service ID
"3,=h--------------------------------";                             // letzte Zeile
#endif


//=====================================================

// median filter elimiert Ausreisser, braucht aber viel RAM und Rechenzeit
// 672 bytes extra RAM bei MAX_VARS = 16
//#define USE_MEDIAN_FILTER

// maximale Anzahl der möglichen Variablen, gegebenfalls anpassen
// um möglichst viel RAM zu sparen sollte MAX_VARS der Anzahl der Zeilen
// in der Defintion entsprechen, insbesondere bei Verwendung des Medianfilters.
#define MAX_VARS 16
double meter_vars[MAX_VARS];
// deltas berechnen
#define MAX_DVARS METERS_USED*2
double dvalues[MAX_DVARS];
uint32_t dtimes[MAX_DVARS];

// software serial pointers
TasmotaSerial *meter_ss[METERS_USED];

// serial buffers
#define SML_BSIZ 32
uint8_t smltbuf[METERS_USED][SML_BSIZ];

// meter nr as string
#define METER_ID_SIZE 24
char meter_id[METERS_USED][METER_ID_SIZE];

#ifdef USE_MEDIAN_FILTER
// median filter, should be odd size
#define MEDIAN_SIZE 5
struct MEDIAN_FILTER {
double buffer[MEDIAN_SIZE];
int8_t index;
} sml_mf[MAX_VARS];

// calc median
double median(struct MEDIAN_FILTER* mf, double in) {
  double tbuff[MEDIAN_SIZE],tmp;
  uint8_t flag;
  mf->buffer[mf->index]=in;
  mf->index++;
  if (mf->index>=MEDIAN_SIZE) mf->index=0;
  // sort list and take median
  memmove(tbuff,mf->buffer,sizeof(tbuff));
  for (byte ocnt=0; ocnt<MEDIAN_SIZE; ocnt++) {
    flag=0;
    for (byte count=0; count<MEDIAN_SIZE-1; count++) {
      if (tbuff[count]>tbuff[count+1]) {
        tmp=tbuff[count];
        tbuff[count]=tbuff[count+1];
        tbuff[count+1]=tmp;
        flag=1;
      }
    }
    if (!flag) break;
  }
  return tbuff[MEDIAN_SIZE/2];
}
#endif

#ifdef ANALOG_OPTO_SENSOR
// sensor über AD Konverter ADS1115 an i2c Bus
uint8_t ads1115_up;

// ads1115 driver
#define SAMPLE_BIT (0x8000)

#define ADS1115_COMP_QUEUE_SHIFT 0
#define ADS1115_COMP_LATCH_SHIFT 2
#define ADS1115_COMP_POLARITY_SHIFT 3
#define ADS1115_COMP_MODE_SHIFT 4
#define ADS1115_DATA_RATE_SHIFT 5
#define ADS1115_MODE_SHIFT 8
#define ADS1115_PGA_SHIFT 9
#define ADS1115_MUX_SHIFT 12

enum ads1115_comp_queue {
	ADS1115_COMP_QUEUE_AFTER_ONE = 0,
	ADS1115_COMP_QUEUE_AFTER_TWO = 0x1 << ADS1115_COMP_QUEUE_SHIFT,
	ADS1115_COMP_QUEUE_AFTER_FOUR = 0x2 << ADS1115_COMP_QUEUE_SHIFT,
	ADS1115_COMP_QUEUE_DISABLE = 0x3 << ADS1115_COMP_QUEUE_SHIFT,
	ADS1115_COMP_QUEUE_MASK = 0x3 << ADS1115_COMP_QUEUE_SHIFT,
};

enum ads1115_comp_latch {
	ADS1115_COMP_LATCH_NO = 0,
	ADS1115_COMP_LATCH_YES = 1 << ADS1115_COMP_LATCH_SHIFT,
	ADS1115_COMP_LATCH_MASK = 1 << ADS1115_COMP_LATCH_SHIFT,
};

enum ads1115_comp_polarity {
	ADS1115_COMP_POLARITY_ACTIVE_LOW = 0,
	ADS1115_COMP_POLARITY_ACTIVE_HIGH = 1 << ADS1115_COMP_POLARITY_SHIFT,
	ADS1115_COMP_POLARITY_MASK = 1 << ADS1115_COMP_POLARITY_SHIFT,
};

enum ads1115_comp_mode {
	ADS1115_COMP_MODE_WINDOW = 0,
	ADS1115_COMP_MODE_HYSTERESIS = 1 << ADS1115_COMP_MODE_SHIFT,
	ADS1115_COMP_MODE_MASK = 1 << ADS1115_COMP_MODE_SHIFT,
};

enum ads1115_data_rate {
	ADS1115_DATA_RATE_8_SPS = 0,
	ADS1115_DATA_RATE_16_SPS = 0x1 << ADS1115_DATA_RATE_SHIFT,
	ADS1115_DATA_RATE_32_SPS = 0x2 << ADS1115_DATA_RATE_SHIFT,
	ADS1115_DATA_RATE_64_SPS = 0x3 << ADS1115_DATA_RATE_SHIFT,
	ADS1115_DATA_RATE_128_SPS = 0x4 << ADS1115_DATA_RATE_SHIFT,
	ADS1115_DATA_RATE_250_SPS = 0x5 << ADS1115_DATA_RATE_SHIFT,
	ADS1115_DATA_RATE_475_SPS = 0x6 << ADS1115_DATA_RATE_SHIFT,
	ADS1115_DATA_RATE_860_SPS = 0x7 << ADS1115_DATA_RATE_SHIFT,
	ADS1115_DATA_RATE_MASK = 0x7 << ADS1115_DATA_RATE_SHIFT,
};

enum ads1115_mode {
	ADS1115_MODE_CONTINUOUS = 0,
	ADS1115_MODE_SINGLE_SHOT = 1 << ADS1115_MODE_SHIFT,
	ADS1115_MODE_MASK = 1 << ADS1115_MODE_SHIFT,
};

enum ads1115_pga {
	ADS1115_PGA_TWO_THIRDS = 0, //±6.144 V
	ADS1115_PGA_ONE = 0x1 << ADS1115_PGA_SHIFT, //±4.096 V
	ADS1115_PGA_TWO = 0x2 << ADS1115_PGA_SHIFT, //±2.048 V
	ADS1115_PGA_FOUR = 0x3 << ADS1115_PGA_SHIFT, //±1.024 V
	ADS1115_PGA_EIGHT = 0x4 << ADS1115_PGA_SHIFT, //±0.512 V
	ADS1115_PGA_SIXTEEN = 0x5 << ADS1115_PGA_SHIFT, //±0.256 V
	ADS1115_PGA_MASK = 0x7 << ADS1115_PGA_SHIFT,
};


enum ads1115_mux {
	ADS1115_MUX_DIFF_AIN0_AIN1 = 0,
	ADS1115_MUX_DIFF_AIN0_AIN3 = 0x1 << ADS1115_MUX_SHIFT,
	ADS1115_MUX_DIFF_AIN1_AIN3 = 0x2 << ADS1115_MUX_SHIFT,
	ADS1115_MUX_DIFF_AIN2_AIN3 = 0x3 << ADS1115_MUX_SHIFT,
	ADS1115_MUX_GND_AIN0 = 0x4 << ADS1115_MUX_SHIFT,
	ADS1115_MUX_GND_AIN1 = 0x5 << ADS1115_MUX_SHIFT,
	ADS1115_MUX_GND_AIN2 = 0x6 << ADS1115_MUX_SHIFT,
	ADS1115_MUX_GND_AIN3 = 0x7 << ADS1115_MUX_SHIFT,
	ADS1115_MUX_MASK = 0x7 << ADS1115_MUX_SHIFT,
};

class ADS1115 {
public:
	ADS1115(uint8_t address = 0x48);

	void begin();
	uint8_t trigger_sample();
	uint8_t reset();
	bool is_sample_in_progress();
	int16_t read_sample();
	float sample_to_float(int16_t val);
	float read_sample_float();

	void set_comp_queue(enum ads1115_comp_queue val) { set_config(val, ADS1115_COMP_QUEUE_MASK); }
	void set_comp_latching(enum ads1115_comp_latch val) { set_config(val, ADS1115_COMP_LATCH_MASK); }
	void set_comp_polarity(enum ads1115_comp_polarity val) { set_config(val, ADS1115_COMP_POLARITY_MASK); }
	void set_comp_mode(enum ads1115_comp_mode val) { set_config(val, ADS1115_COMP_MODE_MASK); }
	void set_data_rate(enum ads1115_data_rate val) { set_config(val, ADS1115_DATA_RATE_MASK); }
	void set_mode(enum ads1115_mode val) { set_config(val, ADS1115_MODE_MASK); }
	void set_pga(enum ads1115_pga val) { set_config(val, ADS1115_PGA_MASK); m_voltage_range = val >> ADS1115_PGA_SHIFT; }
	void set_mux(enum ads1115_mux val) { set_config(val, ADS1115_MUX_MASK); }

private:
	void set_config(uint16_t val, uint16_t mask) {
		m_config = (m_config & ~mask) | val;
	}

	uint8_t write_register(uint8_t reg, uint16_t val);
	uint16_t read_register(uint8_t reg);

	uint8_t m_address;
	uint16_t m_config;
	int m_voltage_range;
};


enum ads1115_register {
	ADS1115_REGISTER_CONVERSION = 0,
	ADS1115_REGISTER_CONFIG = 1,
	ADS1115_REGISTER_LOW_THRESH = 2,
	ADS1115_REGISTER_HIGH_THRESH = 3,
};

#define FACTOR 32768.0
static float ranges[] = { 6.144 / FACTOR, 4.096 / FACTOR, 2.048 / FACTOR, 1.024 / FACTOR, 0.512 / FACTOR, 0.256 / FACTOR};

ADS1115::ADS1115(uint8_t address)
{
        m_address = address;
        m_config = ADS1115_COMP_QUEUE_AFTER_ONE |
                   ADS1115_COMP_LATCH_NO |
                   ADS1115_COMP_POLARITY_ACTIVE_LOW |
                   ADS1115_COMP_MODE_WINDOW |
                   ADS1115_DATA_RATE_128_SPS |
                   ADS1115_MODE_SINGLE_SHOT |
                   ADS1115_MUX_GND_AIN0;
        set_pga(ADS1115_PGA_ONE);
}

uint8_t ADS1115::write_register(uint8_t reg, uint16_t val)
{
        Wire.beginTransmission(m_address);
        Wire.write(reg);
        Wire.write(val>>8);
        Wire.write(val & 0xFF);
        return Wire.endTransmission();
}

uint16_t ADS1115::read_register(uint8_t reg)
{
        Wire.beginTransmission(m_address);
        Wire.write(reg);
        Wire.endTransmission();

        uint8_t result = Wire.requestFrom((int)m_address, 2, 1);
        if (result != 2) {
                Wire.flush();
                return 0;
        }

        uint16_t val;

        val = Wire.read() << 8;
        val |= Wire.read();
        return val;
}

void ADS1115::begin()
{
        Wire.begin();
}

uint8_t ADS1115::trigger_sample()
{
        return write_register(ADS1115_REGISTER_CONFIG, m_config | SAMPLE_BIT);
}

uint8_t ADS1115::reset()
{
	Wire.beginTransmission(0);
	Wire.write(0x6);
	return Wire.endTransmission();
}

bool ADS1115::is_sample_in_progress()
{
	uint16_t val = read_register(ADS1115_REGISTER_CONFIG);
	return (val & SAMPLE_BIT) == 0;
}

int16_t ADS1115::read_sample()
{
        return read_register(ADS1115_REGISTER_CONVERSION);
}

float ADS1115::sample_to_float(int16_t val)
{
	return val * ranges[m_voltage_range];
}

float ADS1115::read_sample_float()
{
	return sample_to_float(read_sample());
}

ADS1115 adc;

void ADS1115_init(void) {

  ads1115_up=0;
  if (!i2c_flg) return;

  adc.begin();
  adc.set_data_rate(ADS1115_DATA_RATE_128_SPS);
  adc.set_mode(ADS1115_MODE_CONTINUOUS);
  adc.set_mux(ADS1115_MUX_DIFF_AIN0_AIN3);
  adc.set_pga(ADS1115_PGA_TWO);

  int16_t val = adc.read_sample();
  ads1115_up=1;
}

#endif




// dump to log zeigt serielle Daten zu Testzwecken in der Konsole an
// muss aber für Normalbetrieb aus sein
// dazu in Konsole sensor95 d1,d2,d3 .. bzw. d0 für Ein und Ausschalten der jeweiligen Zähler dumps angeben

char sml_start;
uint8_t dump2log=0;

#define SML_SAVAILABLE Serial_available()
#define SML_SREAD Serial_read()
#define SML_SPEAK Serial_peek()

bool Serial_available() {
  uint8_t num=dump2log&7;
  if (num<1 || num>METERS_USED) return Serial.available();
  if (num==1) {
      return Serial.available();
  } else {
    return meter_ss[num-1]->available();
  }
}

uint8_t Serial_read() {
  uint8_t num=dump2log&7;
  if (num<1 || num>METERS_USED) return Serial.read();
  if (num==1) {
      return Serial.read();
  } else {
    return meter_ss[num-1]->read();
  }
}

uint8_t Serial_peek() {
  uint8_t num=dump2log&7;
  if (num<1 || num>METERS_USED) return Serial.peek();
  if (num==1) {
      return Serial.peek();
  } else {
    return meter_ss[num-1]->peek();
  }
}

void Dump2log(void) {

int16_t index=0,hcnt=0;
uint32_t d_lastms;
uint8_t dchars[16];

  if (!SML_SAVAILABLE) return;

  if (dump2log&8) {
    // combo mode
    while (SML_SAVAILABLE) {
      log_data[index]=':';
      index++;
      log_data[index]=' ';
      index++;
      d_lastms=millis();
      while ((millis()-d_lastms)<40) {
        if (SML_SAVAILABLE) {
          uint8_t c=SML_SREAD;
          sprintf(&log_data[index],"%02x ",c);
          dchars[hcnt]=c;
          index+=3;
          hcnt++;
          if (hcnt>15) {
            // line complete, build asci chars
            log_data[index]='=';
            index++;
            log_data[index]='>';
            index++;
            log_data[index]=' ';
            index++;
            for (uint8_t ccnt=0; ccnt<16; ccnt++) {
              if (isprint(dchars[ccnt])) {
                log_data[index]=dchars[ccnt];
              } else {
                log_data[index]=' ';
              }
              index++;
            }
            break;
          }
        }
      }
      if (index>0) {
        log_data[index]=0;
        AddLog(LOG_LEVEL_INFO);
        index=0;
        hcnt=0;
      }
    }
  } else {
    //while (SML_SAVAILABLE) {
      index=0;
      log_data[index]=':';
      index++;
      log_data[index]=' ';
      index++;
      d_lastms=millis();
      while ((millis()-d_lastms)<40) {
        if (SML_SAVAILABLE) {
          if (meter_desc[(dump2log&7)-1].type=='o') {
            char c=SML_SREAD&0x7f;
            if (c=='\n' || c=='\r') break;
            log_data[index]=c;
            index++;
          } else {
            unsigned char c;
            if (sml_start==0x77) {
              sml_start=0;
            } else {
              c=SML_SPEAK;
              if (c==0x77) {
                sml_start=c;
                break;
              }
            }
            c=SML_SREAD;
            sprintf(&log_data[index],"%02x ",c);
            index+=3;
          }
        }
      }
      if (index>0) {
        log_data[index]=0;
        AddLog(LOG_LEVEL_INFO);
      }
    }
  //}

}

// skip sml entries
uint8_t *skip_sml(uint8_t *cp,int16_t *res) {
    uint8_t len,len1,type;
    len=*cp&0xf;
    type=*cp&0x70;
    if (type==0x70) {
        // list, skip entries
        // list
        cp++;
        while (len--) {
            len1=*cp&0x0f;
            cp+=len1;
        }
         *res=0;
    } else {
        // skip len
        *res=(signed char)*(cp+1);
        cp+=len;
    }
    return cp;
}

// get sml binary value
// not defined for unsigned >0x7fff ffff ffff ffff (should never happen)
int64_t sml_getvalue(unsigned char *cp,uint8_t index) {
uint8_t len,unit,type;
int16_t scaler,result;
int64_t value;

  // scan for values
  // check status
  cp=skip_sml(cp,&result);
  // check time
  cp=skip_sml(cp,&result);
  // check unit
  cp=skip_sml(cp,&result);
  // check scaler
  cp=skip_sml(cp,&result);
  scaler=result;
  // get value
  type=*cp&0x70;
  len=*cp&0x0f;
  cp++;
    if (type==0x50 || type==0x60) {
        // shift into 64 bit
        uint64_t uvalue=0;
        uint8_t nlen=len;
        while (--nlen) {
            uvalue<<=8;
            uvalue|=*cp++;
        }
        if (type==0x50) {
            // signed
            switch (len-1) {
                case 1:
                    // byte
                    value=(signed char)uvalue;
                    break;
                case 2:
                    // signed 16 bit
                    value=(int16_t)uvalue;
                    break;
                case 3:
                case 4:
                    // signed 32 bit
                    value=(int32_t)uvalue;
                    break;
                case 5:
                case 6:
                case 7:
                case 8:
                    // signed 64 bit
                    value=(int64_t)uvalue;
                    break;
            }
        } else {
            // unsigned
            value=uvalue;
        }

    } else {
        if (!(type&0xf0)) {
            // octet string serial number
            // no coding found on the net
            // up to now 2 types identified on Hager
            if (len==9) {
              // serial number on hager => 24 bit - 24 bit
                cp++;
                uint32_t s1,s2;
                s1=*cp<<16|*(cp+1)<<8|*(cp+2);
                cp+=4;
                s2=*cp<<16|*(cp+1)<<8|*(cp+2);
                sprintf(&meter_id[index][0],"%u-%u",s1,s2);
            } else {
                // server id on hager
                char *str=&meter_id[index][0];
                for (type=0; type<len-1; type++) {
                    sprintf(str,"%02x",*cp++);
                    str+=2;
                }
            }
            value=0;
        } else {
            value=999999;
            scaler=0;
        }
    }
    if (scaler==-1) {
        value/=10;
    }
    return value;
}

uint8_t hexnibble(char chr) {
  uint8_t rVal = 0;
  if (isdigit(chr)) {
    rVal = chr - '0';
  } else  {
    chr=toupper(chr);
    if (chr >= 'A' && chr <= 'F') rVal = chr + 10 - 'A';
  }
  return rVal;
}

uint8_t sb_counter;

// because orig CharToDouble was defective
// fixed in Tasmota  6.4.1.19 20190222
double xCharToDouble(const char *str)
{
  // simple ascii to double, because atof or strtod are too large
  char strbuf[24];

  strlcpy(strbuf, str, sizeof(strbuf));
  char *pt = strbuf;
  while ((*pt != '\0') && isblank(*pt)) { pt++; }  // Trim leading spaces

  signed char sign = 1;
  if (*pt == '-') { sign = -1; }
  if (*pt == '-' || *pt=='+') { pt++; }            // Skip any sign

  double left = 0;
  if (*pt != '.') {
    left = atoi(pt);                               // Get left part
    while (isdigit(*pt)) { pt++; }                 // Skip number
  }

  double right = 0;
  if (*pt == '.') {
    pt++;
    right = atoi(pt);                              // Decimal part
    while (isdigit(*pt)) {
      pt++;
      right /= 10.0;
    }
  }

  double result = left + right;
  if (sign < 0) {
    return -result;                                // Add negative sign
  }
  return result;
}


// polled every 50 ms
void SML_Poll(void) {
    uint16_t count,meters;

    for (meters=0; meters<METERS_USED; meters++) {
      if (meter_desc[meters].type!='c') {
        // poll for serial input
        if (!meter_desc[meters].srcpin || meter_desc[meters].srcpin==3) {
          while (Serial.available()) {
            // shift in
            for (count=0; count<SML_BSIZ-1; count++) {
              smltbuf[meters][count]=smltbuf[meters][count+1];
            }
            if (meter_desc[meters].type=='o') {
              smltbuf[meters][SML_BSIZ-1]=(uint8_t)Serial.read()&0x7f;
            } else {
              smltbuf[meters][SML_BSIZ-1]=(uint8_t)Serial.read();
            }
            sb_counter++;
            SML_Decode(meters);
          }
        } else {
          while (meter_ss[meters]->available()) {
            // shift in
            for (count=0; count<SML_BSIZ-1; count++) {
              smltbuf[meters][count]=smltbuf[meters][count+1];
            }
            if (meter_desc[meters].type=='o') {
              smltbuf[meters][SML_BSIZ-1]=(uint8_t)meter_ss[meters]->read()&0x7f;
            } else {
              smltbuf[meters][SML_BSIZ-1]=(uint8_t)meter_ss[meters]->read();
            }
            sb_counter++;
            SML_Decode(meters);
          }
        }
      }
    }
}


void SML_Decode(uint8_t index) {
  const char *mp=(const char*)meter;
  int8_t mindex;
  uint8_t *cp;
  uint8_t dindex=0,vindex=0;
  delay(0);
  while (mp != NULL) {
    // check list of defines

    // new section
    mindex=((*mp)&7)-1;

    if (mindex<0 || mindex>=METERS_USED) mindex=0;
    mp+=2;
    if (*mp=='=' && *(mp+1)=='h') {
      mp = strchr(mp, '|');
      if (mp) mp++;
      continue;
    }

    if (index!=mindex) goto nextsect;

    // start of serial source buffer
    cp=&smltbuf[mindex][0];

    // compare
    if (*mp=='=') {
      // calculated entry, check syntax
      mp++;
      // do math m 1+2+3
      if (*mp=='m' && !sb_counter) {
        // only every 256 th byte
        // else it would be calculated every single serial byte
        mp++;
        while (*mp==' ') mp++;
        // 1. index
        double dvar;
        uint8_t opr;
        uint32_t ind;
        ind=atoi(mp);
        while (*mp>='0' && *mp<='9') mp++;
        if (ind<1 || ind>MAX_VARS) ind=1;
        dvar=meter_vars[ind-1];
        for (uint8_t p=0;p<5;p++) {
          if (*mp=='@') {
            // store result
            meter_vars[vindex]=dvar;
            mp++;
            SML_Immediate_MQTT((const char*)mp,vindex,mindex);
            break;
          }
          opr=*mp;
          mp++;
          uint8_t iflg=0;
          if (*mp=='#') {
            iflg=1;
            mp++;
          }
          ind=atoi(mp);
          while (*mp>='0' && *mp<='9') mp++;
          if (ind<1 || ind>MAX_VARS) ind=1;
          switch (opr) {
              case '+':
                if (iflg) dvar+=ind;
                else dvar+=meter_vars[ind-1];
                break;
              case '-':
                if (iflg) dvar-=ind;
                else dvar-=meter_vars[ind-1];
                break;
              case '*':
                if (iflg) dvar*=ind;
                else dvar*=meter_vars[ind-1];
                break;
              case '/':
                if (iflg) dvar/=ind;
                else dvar/=meter_vars[ind-1];
                break;
          }
          while (*mp==' ') mp++;
          if (*mp=='@') {
            // store result
            meter_vars[vindex]=dvar;
            mp++;
            SML_Immediate_MQTT((const char*)mp,vindex,mindex);
            break;
          }
        }
      } else if (*mp=='d') {
        // calc deltas d ind 10 (eg every 10 secs)
        if (dindex<MAX_DVARS) {
          // only n indexes
          mp++;
          while (*mp==' ') mp++;
          uint8_t ind=atoi(mp);
          while (*mp>='0' && *mp<='9') mp++;
          if (ind<1 || ind>MAX_VARS) ind=1;
          uint32_t delay=atoi(mp)*1000;
          uint32_t dtime=millis()-dtimes[dindex];
          if (dtime>delay) {
            // calc difference
            dtimes[dindex]=millis();
            double vdiff = meter_vars[ind-1]-dvalues[dindex];
            dvalues[dindex]=meter_vars[ind-1];
            meter_vars[vindex]=(double)360000.0*vdiff/((double)dtime/10000.0);

            mp=strchr(mp,'@');
            if (mp) {
              mp++;
              SML_Immediate_MQTT((const char*)mp,vindex,mindex);
            }
          }
          dindex++;
        }
      } else if (*mp=='h') {
        // skip html tag line
        mp = strchr(mp, '|');
        if (mp) mp++;
        continue;
      }
    } else {
      // compare value
      uint8_t found=1;
      while (*mp!='@') {
        if (meter_desc[mindex].type=='o' || meter_desc[mindex].type=='c') {
          if (*mp++!=*cp++) {
            found=0;
          }
        } else {
          uint8_t val = hexnibble(*mp++) << 4;
          val |= hexnibble(*mp++);
          if (val!=*cp++) {
            found=0;
          }
        }
      }
      if (found) {
        // matches, get value
        mp++;
        if (*mp=='#') {
          // get string value
          mp++;
          if (meter_desc[mindex].type=='o') {
            for (uint8_t p=0;p<METER_ID_SIZE;p++) {
              if (*cp==*mp) {
                meter_id[mindex][p]=0;
                break;
              }
              meter_id[mindex][p]=*cp++;
            }
          } else {
            sml_getvalue(cp,mindex);
          }
        } else {
          double dval;
          // get numeric values
          if (meter_desc[mindex].type=='o' || meter_desc[mindex].type=='c') {
            dval=xCharToDouble((char*)cp);
          } else {
            dval=sml_getvalue(cp,mindex);
          }
#ifdef USE_MEDIAN_FILTER
          meter_vars[vindex]=median(&sml_mf[vindex],dval);
#else
          meter_vars[vindex]=dval;
#endif
          // get scaling factor
          double fac=xCharToDouble((char*)mp);
          meter_vars[vindex]/=fac;
          SML_Immediate_MQTT((const char*)mp,vindex,mindex);
        }
      }
    }
nextsect:
    // next section
    if (vindex<MAX_VARS-1) {
      vindex++;
    }
    mp = strchr(mp, '|');
    if (mp) mp++;
  }
}

//"1-0:1.8.0*255(@1," D_TPWRIN ",KWh," DJ_TPWRIN ",4|"
void SML_Immediate_MQTT(const char *mp,uint8_t index,uint8_t mindex) {
  char tpowstr[32];
  char jname[24];

  // we must skip sf,webname,unit
  char *cp=strchr(mp,',');
  if (cp) {
    cp++;
    // wn
    cp=strchr(cp,',');
    if (cp) {
      cp++;
      // unit
      cp=strchr(cp,',');
      if (cp) {
        cp++;
        // json mqtt
        for (uint8_t count=0;count<sizeof(jname);count++) {
          if (*cp==',') {
            jname[count]=0;
            break;
          }
          jname[count]=*cp++;
        }
        cp++;
        uint8_t dp=atoi(cp);
        if (dp&0x10) {
          // immediate mqtt
          dtostrfd(meter_vars[index],dp&0xf,tpowstr);
          snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_JSON_TIME "\":\"%s\""), GetDateAndTime(DT_LOCAL).c_str());
          snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("%s,\"%s\":{ \"%s\":%s}}"), mqtt_data,meter_desc[mindex].prefix,jname,tpowstr);
          MqttPublishPrefixTopic_P(TELE, PSTR(D_RSLT_SENSOR), Settings.flag.mqtt_sensor_retain);
        }
      }
    }
  }
}

// web + json interface
void SML_Show(boolean json) {
  int8_t count,mindex,cindex=0;
  char tpowstr[32];
  char name[24];
  char unit[8];
  char jname[24];
  int8_t index=0,mid=0;
  char *mp=(char*)meter;
  char *cp;

    int8_t lastmind=((*mp)&7)-1;
    if (lastmind<0 || lastmind>=METERS_USED) lastmind=0;
    while (mp != NULL) {
        // setup sections
        mindex=((*mp)&7)-1;
        if (mindex<0 || mindex>=METERS_USED) mindex=0;
        mp+=2;
        if (*mp=='=' && *(mp+1)=='h') {
          mp+=2;
          // html tag
          if (json) {
            mp = strchr(mp, '|');
            if (mp) mp++;
            continue;
          }
          // web ui export
          uint8_t i;
          for (i=0;i<sizeof(tpowstr)-2;i++) {
            if (*mp=='|' || *mp==0) break;
            tpowstr[i]=*mp++;
          }
          tpowstr[i]=0;
          // export html
          snprintf_P(mqtt_data, sizeof(mqtt_data), "%s{s}%s{e}", mqtt_data,tpowstr);
          // rewind, to ensure strchr
          mp--;
          mp = strchr(mp, '|');
          if (mp) mp++;
          continue;
        }
        // skip compare section
        cp=strchr(mp,'@');
        if (cp) {
          cp++;
          if (*cp=='#') {
            // meter id
            sprintf(tpowstr,"\"%s\"",&meter_id[mindex][0]);
            mid=1;
          } else {
            mid=0;
          }
          // skip scaling
          cp=strchr(cp,',');
          if (cp) {
            // this is the name in web UI
            cp++;
            for (count=0;count<sizeof(name);count++) {
              if (*cp==',') {
                name[count]=0;
                break;
              }
              name[count]=*cp++;
            }
            cp++;

            for (count=0;count<sizeof(unit);count++) {
              if (*cp==',') {
                unit[count]=0;
                break;
              }
              unit[count]=*cp++;
            }
            cp++;

            for (count=0;count<sizeof(jname);count++) {
              if (*cp==',') {
                jname[count]=0;
                break;
              }
              jname[count]=*cp++;
            }

            cp++;

            if (!mid) {
              uint8_t dp=atoi(cp)&0xf;
              dtostrfd(meter_vars[index],dp,tpowstr);
            }

            if (json) {
              // json export
              if (index==0) snprintf_P(mqtt_data, sizeof(mqtt_data), "%s,\"%s\":{\"%s\":%s", mqtt_data,meter_desc[mindex].prefix,jname,tpowstr);
              else {
                if (lastmind!=mindex) {
                  // meter changed, close mqtt
                  snprintf_P(mqtt_data, sizeof(mqtt_data), "%s}", mqtt_data);
                  // and open new
                  snprintf_P(mqtt_data, sizeof(mqtt_data), "%s,\"%s\":{\"%s\":%s", mqtt_data,meter_desc[mindex].prefix,jname,tpowstr);
                  lastmind=mindex;
                } else {
                  snprintf_P(mqtt_data, sizeof(mqtt_data), "%s,\"%s\":%s", mqtt_data,jname,tpowstr);
                }
              }
            } else {
              // web ui export
              snprintf_P(mqtt_data, sizeof(mqtt_data), "%s{s}%s %s: {m}%s %s{e}", mqtt_data,meter_desc[mindex].prefix,name,tpowstr,unit);
            }
          }
        }
        if (index<MAX_VARS-1) {
          index++;
        }
        // next section
        mp = strchr(cp, '|');
        if (mp) mp++;
    }
    if (json) snprintf_P(mqtt_data, sizeof(mqtt_data), "%s}", mqtt_data);
}

struct SML_COUNTER {
  uint8_t sml_cnt_debounce;
  uint8_t sml_cnt_old_state;
  uint32_t sml_cnt_last_ts;
#ifdef ANALOG_OPTO_SENSOR
  int16_t ana_curr;
  int16_t ana_max;
  int16_t ana_min;
  int16_t ana_cmpl;
  int16_t ana_cmph;
#endif
} sml_counters[MAX_COUNTERS];


void SML_Init(void) {
  uint8_t cindex=0;
  // preloud counters
  for (byte i = 0; i < MAX_COUNTERS; i++) {
      RtcSettings.pulse_counter[i]=Settings.pulse_counter[i];
      sml_counters[i].sml_cnt_last_ts=millis();
  }
  for (uint8_t meters=0; meters<METERS_USED; meters++) {
    if (meter_desc[meters].type=='c') {
        if (meter_desc[meters].flag&2) {
          // analog mode
#ifdef ANALOG_OPTO_SENSOR
          ADS1115_init();
          sml_counters[cindex].ana_max=-32768;
          sml_counters[cindex].ana_min=+32767;
#endif
        } else {
          // counters, set to input with pullup
          if (meter_desc[meters].flag&1) {
            pinMode(meter_desc[meters].srcpin,INPUT_PULLUP);
          } else {
            pinMode(meter_desc[meters].srcpin,INPUT);
          }
          InjektCounterValue(meters,RtcSettings.pulse_counter[cindex]);
          cindex++;
        }
    } else {
      // serial input, init
      if (!meter_desc[meters].srcpin || meter_desc[meters].srcpin==3) {
        ClaimSerial();
        SetSerialBaudrate(meter_desc[meters].params);
      } else {
#ifdef SPECIAL_SS
        meter_ss[meters] = new TasmotaSerial(meter_desc[meters].srcpin,-1,0,1);
#else
        meter_ss[meters] = new TasmotaSerial(meter_desc[meters].srcpin,-1);
#endif
        if (meter_ss[meters]->begin(meter_desc[meters].params)) {
          meter_ss[meters]->flush();
        }
      }
    }
  }

}


// fast counter polling
void SML_Counter_Poll(void) {
uint16_t meters,cindex=0;

  for (meters=0; meters<METERS_USED; meters++) {
    uint32_t ctime=millis();
    if (meter_desc[meters].type=='c') {
      // poll for counters and debouce
      if (ctime-sml_counters[cindex].sml_cnt_last_ts>meter_desc[meters].params) {
        sml_counters[cindex].sml_cnt_last_ts=millis();

        if (meter_desc[meters].flag&2) {
          // analog mode, get next value
#ifdef ANALOG_OPTO_SENSOR
          if (ads1115_up) {
            int16_t val = adc.read_sample();
            if (val>sml_counters[cindex].ana_max) sml_counters[cindex].ana_max=val;
            if (val<sml_counters[cindex].ana_min) sml_counters[cindex].ana_min=val;
            sml_counters[cindex].ana_curr=val;
            int16_t range=sml_counters[cindex].ana_max-sml_counters[cindex].ana_min;

          }
#endif
        } else {
          // poll digital input
  #ifdef DEBUG_CNT_LED1
          if (cindex==0) {
            pinMode(DEBUG_CNT_LED1, OUTPUT);
            if (digitalRead(meter_desc[meters].srcpin)) {
              digitalWrite(DEBUG_CNT_LED1,HIGH);
            } else {
              digitalWrite(DEBUG_CNT_LED1,LOW);
            }
          }
  #endif
  #ifdef DEBUG_CNT_LED2
          if (cindex==1) {
            pinMode(DEBUG_CNT_LED2, OUTPUT);
            if (digitalRead(meter_desc[meters].srcpin)) {
              digitalWrite(DEBUG_CNT_LED2,HIGH);
            } else {
              digitalWrite(DEBUG_CNT_LED2,LOW);
            }
          }
  #endif
          uint8_t state;
          sml_counters[cindex].sml_cnt_debounce<<=1;
          sml_counters[cindex].sml_cnt_debounce|=(digitalRead(meter_desc[meters].srcpin)&1)|0x80;
          if (sml_counters[cindex].sml_cnt_debounce==0xc0) {
            // is 1
            state=1;
          } else {
            // is 0, means switch down
            state=0;
          }
          if (sml_counters[cindex].sml_cnt_old_state!=state) {
            // state has changed
            sml_counters[cindex].sml_cnt_old_state=state;
            if (state==0) {
              // inc counter
              RtcSettings.pulse_counter[cindex]++;
              InjektCounterValue(meters,RtcSettings.pulse_counter[cindex]);
            }
          }
        }
      }
      cindex++;
    }
  }
}

#ifdef SML_SEND_SEQ
#define SML_SEQ_PERIOD 5
uint8_t sml_seq_cnt;
void SendSeq(void) {
  sml_seq_cnt++;
  if (sml_seq_cnt>SML_SEQ_PERIOD) {
    sml_seq_cnt=0;
    // send sequence every N Seconds
    uint8_t sequence[]={0x2F,0x3F,0x21,0x0D,0x0A,0};
    uint8_t *ucp=sequence;
    while (*ucp) {
      uint8_t iob=*ucp++;
      // for no parity disable next line
      iob|=(CalcEvenParity(iob)<<7);
      Serial.write(iob);
    }
  }
}

// for odd parity init with 1
uint8_t CalcEvenParity(uint8_t data) {
uint8_t parity=0;

  while(data) {
    parity^=(data &1);
    data>>=1;
  }
  return parity;
}
#endif

bool XSNS_95_cmd(void) {
  boolean serviced = true;
  const char S_JSON_SML[] = "{\"" D_CMND_SENSOR "%d\":%s:%d}";
  const char S_JSON_CNT[] = "{\"" D_CMND_SENSOR "%d\":%s%d:%d}";
  if (XdrvMailbox.data_len > 0) {
      char *cp=XdrvMailbox.data;
      if (*cp=='d') {
        // set dump mode
        cp++;
        dump2log=atoi(cp);
        snprintf_P(mqtt_data, sizeof(mqtt_data), S_JSON_SML, XSNS_95,"dump_mode",dump2log);
      } else if (*cp=='c') {
          // set ounter
          cp++;
          uint8_t index=*cp&7;
          if (index<1 || index>MAX_COUNTERS) index=1;
          cp++;
          while (*cp==' ') cp++;
          if (isdigit(*cp)) {
            uint32_t cval=atoi(cp);
            while (isdigit(*cp)) cp++;
            RtcSettings.pulse_counter[index-1]=cval;
            uint8_t cindex=0;
            for (uint8_t meters=0; meters<METERS_USED; meters++) {
              if (meter_desc[meters].type=='c') {
                InjektCounterValue(meters,RtcSettings.pulse_counter[cindex]);
                cindex++;
              }
            }
          }
          snprintf_P(mqtt_data, sizeof(mqtt_data), S_JSON_CNT, XSNS_95,"Counter",index,RtcSettings.pulse_counter[index-1]);
      } else {
        serviced=false;
      }
  }
  return serviced;
}

void InjektCounterValue(uint8_t meter,uint32_t counter) {
  sprintf((char*)&smltbuf[meter][0],"1-0:1.8.0*255(%d)",counter);
  SML_Decode(meter);
}

void SML_CounterSaveState(void) {
  for (byte i = 0; i < MAX_COUNTERS; i++) {
      Settings.pulse_counter[i] = RtcSettings.pulse_counter[i];
  }
}

/*********************************************************************************************\
 * Interface
\*********************************************************************************************/


// bei neuer Tasmota Version muss hier bool stehen statt boolean
boolean Xsns95(byte function) {
//bool Xsns95(byte function) {
  boolean result = false;
    switch (function) {
      case FUNC_INIT:
        SML_Init();
        break;
      case FUNC_LOOP:
        SML_Counter_Poll();
        break;
      case FUNC_EVERY_50_MSECOND:
        if (dump2log) Dump2log();
        else SML_Poll();
        break;
#ifdef SML_SEND_SEQ
      case FUNC_EVERY_SECOND:
        SendSeq();
        break;
#endif
      case FUNC_JSON_APPEND:
        SML_Show(1);
        break;
#ifdef USE_WEBSERVER
#if VERSION < 0x06050008
      case FUNC_WEB_APPEND:
        SML_Show(0);
        break;
#else
      case FUNC_WEB_SENSOR:
        SML_Show(0);
        break;
#endif
#endif  // USE_WEBSERVER
      case FUNC_COMMAND:
        if (XSNS_95 == XdrvMailbox.index) {
          result = XSNS_95_cmd();
        }
        break;
      case FUNC_SAVE_BEFORE_RESTART:
        SML_CounterSaveState();
        break;
    }
  return result;
}

#endif  // USE_SML
