**DisplayText commands:**

(You must set DisplayMode to zero to use DisplayText, or disable other
modes before compilation with \#undef USE\_DISPLAY\_MODES1TO5), then
select your display with DisplayModel (see list below) dont forget to
select the gpio pins you need.

DisplayText not only draws Text but via escapes also line primitives

commands are embedded within brackets \[\]

p stands for parameter and may be a number from 1 to n digits

on monochrome graphics displays things are drawn into a local frame
buffer and send to the display either via the „d" command or
automatically at the end of the command

**command list:**

positioning:

l*p* = sets a character line to print at

c*p* = sets a character column to print at

x*p* = sets the x position for consecutive prints

y*p* = sets the y position for consecutive prints

line primitives:

h*p* = draws a horizontal line with lenght p (x is advanced)

v*p* = draws a vertical line with lenght p (y is advanced)

*Lp:p* = draws a line to p(x) and p(y) (x,y are advanced)

k*p* = draws a circle with radius p

K*p* = draws a filled circle with radius p

r*w:h* = draws a rectangle with width and height

R*w:h* = draws a filled rectangle with width and height

u*w:h:r* = draws a rounded rectangle width, height and radius

U*w:h:r* = draws a filled rounded rectangle width, height and radius

miscelanious:

z = clears the display

i = (re)init the display (in e-paper mode with partial update)

I = (re)init the display (in e-paper mode with full update)

d = draw actually updates the display

D*p* = switch display auto updates on/off, when off must be updated via
cmd d

o = switch display off

O = switch display on

t = display Tasmota time in HH:MM

T = display Tasmota date in DD.MM.YY

p*p =* pad text with spaces, positiv values align left, negativ values
align right

s*p* = set text size (scaling factor 1\...4) (only for classic GFX font)

f*p* = set font (1=12, 2=24,(opt 3=8)) if font==0 the classic GFX font
is used

C*p* = set foreground color (0,1) for black or white or 16 bit color
code

B*p* = set background color (0,1) for black or white or 16 bit color
code

Ci*p* or Bi*p* = set colors from index currently 0-18 see table below

w*p* = draws an analog watch with radius p

Buttons:

Draw GFX Buttons (up to 16)

b \...

n=button number 0-15

xp = x position

yp = y position

xs = x size

ys = y size

oc = outline color

fc = fill color

tc = text color

ts = text size

bt = button text (must end with a colon :) (max 9 chars)

Line chart:

> Up to 4 line charts may be defined. Ticks may be defined by adding
> tick numbers to the 1. parameter n like this =\> n = graph number
> (0-3) + x ticks (4\*number of x ticks) + y ticks (256\*number of y
> ticks)

*Gn:xp:yp:xs:ys:t:fmax:fmin* = defines a line chart.

n = number up to 4 charts (0-3) + optional ticks

xp = x position

yp = y position

xs = x size

ys = y size

t = time in minutes for total chart

ymin = float chart minimum y

ymax = float chat maximum y

icol = line indexcolor (only for color graphs)

gn:v Adds a value to the chart buffer

n = number up to 4 charts (0-3)

v = float value to add

remarks on e paper displays:

======================

epaper displays have 2 operating modes: full update and partial update.

While full update delivers a clean and sharp picture it has the
disadvantage of taking several seconds for the screen update and a
severe flickering during update.

Partial update is quite fast (300 ms) with no flickering but there is
the possiblity that erased content is still slightly visible .

To „whiten" the display it is therefore usefull to „full update" the
display in regular intervals (e.g each hour)

Defines =\> USE\_SPI , USE\_DISPLAY\_EPAPER29,
USE\_DISPLAY\_EPAPER42

To unify the color interface on epaper 1 (white) is actually black and 0
(black) is white.

Hardware connections:

==================

I2C displays are connected in the usual manner and defined by tasmota
pin selection

The I2C Adress must be given by DisplayAdress XX e.g. 60 and the Model
set with DisplayModel e.g. 2 (for SSD1306) to permanently turn the
display on set DisplayDimmer 100. Display rotation can be permantly set
by DisplayRotate X (0-3)

E-paper displays are connected via 3 wire SPI (CS,SCLK,MOSI) the other 3
Interface lines of the display (DC,Reset,busy) may be left unconnected.
The jumper on the circuit board of the display should be set to 3 wire
SPI. You can NOT use GPIO16 for software spi!

ILI9488 driver

===========

This color display (480x320) also uses a 3 wire SPI interface. If you
select the true SPI lines the driver uses hardware SPI else software
SPI. (MOSI=GPIO13, SCLK=GPIO14,CS=GPIO15) the backpanel ist fixed in the
driver at GPIO2 if not defined by gpio selector

You can NOT use GPIO16 for software spi!

The capacitive touch panel is connected via I2C.

Remarks on fonts:

==============

The EPD fonts contains 95 chars starting from code 32 while the classic
GFX font containts 256 chars ranging from 0 to 255.

To display chars above code 127 you must specify an escape sequence
(standard octal escapes dont work here) the \~character followed by a
hex byte can define any character code. So custom characters above 127
can be displayed

examples:

========

// print Text at line

// size 1 on line 1, column 1

DisplayText \[s1l1c1\]Hallo how are you?

// draw a rectangle and draw text inside with size2 and 7 chars padded
with spaces

DisplayText \[x85y95h130v30h-130v-30s2p7x90y100\]37.25 C

// clear screen

DisplayText \[z\]

// draw rectangle from x,y with width and height

DisplayText \[x50y50r200:100\]

you can display local sensors via rules =\>

// show sensor values and time and separation line, whiten display every
60 minutes

// on 296x128

====================================

rule1 on tele-SHT3X-0x44\#Temperature do DisplayText \[f1p7x0y5\]%value%
C endon on tele-SHT3X-0x44\#Humidity do DisplayText
\[f1p10x70y5\]%value% % \[x0y20h296x250y5t\] endon on
tele-BMP280\#Pressure do DisplayText \[f1p10x140y5\]%value% hPa endon on
Time\#Minute\|60 do DisplayText \[Ii\] endon

// show 4 analog channels (on128x64)

====================================

rule1 on tele-ADS1115\#A0 do DisplayText \[s1p21c1l01\]Analog1: %value%
adc endon on tele-ADS1115\#A1 do DisplayText \[s1p21c1l3\]Analog2:
%value% adc endon on tele-ADS1115\#A2 do DisplayText
\[s1p21c1l5\]Analog3: %value% adc endon on tele-ADS1115\#A3 do
DisplayText \[s1p21c1l7\]Analog4: %value% adc endon

// show BME280 + SGP30 (on 128x64)

====================================

rule1 on tele-BME280\#Temperature do DisplayText \[s1p21x0y0\]Temp:
%value% C endon on tele-BME280\#Humidity do DisplayText
\[s1p21x0y10\]Hum : %value% % endon on BME280\#Pressure do DisplayText
\[s1p21x0y20\]Prss: %value% hPa endon on tele-SGP30\#TVOC do DisplayText
\[s1p21x0y30\]TVOC: %value% ppb endon on tele-SGP30\#eCO2 do DisplayText
\[s1p21x0y40\]eCO2: %value% ppm \[s1p0x0y50\]Time: \[x35y50t\] endon

// show graphs etc on 400 x 300 epaper % sign must be %% in arduino 2.42
!!!

// must use arduino 2.42 due to ram size restrictions !!!

====================================

rule1 on tele-SHT3X-0x44\#Temperature do DisplayText \[f1p7x0y5\]%value%
C endon on tele-SHT3X-0x44\#Humidity do DisplayText
\[x0y20h400x250y5T\]\[x350t\]\[f1p10x70y5\]%value% %% endon on
tele-BMP280\#Pressure do DisplayText \[f1p10x140y5\]%value% hPa endon on
Time\#Minute\|60 do DisplayText \[Ii\] endon

rule2 on System\#Boot do DisplayText
\[zG2656:5:40:300:80:1440:-5000:5000f3x310y40\]+5000 W\[x310y115\]-5000
W endon on System\#Boot do DisplayText \[f1x60y25\]Zweirichtungszaehler
- 24 Stunden\[f1x330y75\] 3500 W endon on System\#Boot do DisplayText
\[G2657:5:140:300:80:1440:0:5000f3x310y140\]+5000 W\[x310y215\]0 W endon
on System\#Boot do DisplayText \[f1x70y125\]Volleinspeisung - 24
Stunden\[f1x330y180\] 3500 W endon

or distant sensors via a broker script or on the distant device via
rules and WebSend

remarks to display drivers:

======================

Waveshare has 2 kinds of display controllers: with partial update and
without partial update. The 2.9 inch driver is for partial update and
should support also other waveshare partial update models with modified
WIDTH and HEIGHT parameters

The 4.2 inch driver is a hack which makes the full update display behave
like a partial update and should probably work with other full update
displays.

the drivers are subclasses of GFX library

the class hirarchie is LOWLEVEL :: Paint :: Renderer :: GFX

GFX: library unmodified

Renderer: the interface for Tasmota

Paint: the modified pixel driver for epaper

there are several virtual functions that can be subclassed down to
LOWLEVEL.

The display dispatcher only does the class init call

All other calls go to the Renderer class

In black and white displays a local ram buffer must be allocated bevor
calling the driver

This must be set to zero on character or TFT color displays.

The GFX fonts can alternativly be used instead of the EPD fonts by
selecting the fonts with \#define in the renderer driver file.

Remark for the 400x300 epaper:

This display requires 15k RAM which only fits when using the latest
Arduino library because this lib leaves much more free ram than previous
versions

About 28 k flash is used by these 4 drivers, (epd fonts use about 9k
space. Can be ifdefd)

SSD1306 = 1,15 k

SH1106 = 1,18 k

EPD42 = 2,57 k

EPD29 = 2,1 k

Display and Render class about 12 k

List of TASMOTA display codes

1 = LCD

2 = SSD1306

3 = MATRIX

4 = ILI934 TFT

5 = Waveshare E-PAPER 2.9 inch

6 = Waveshare E-PAPER 4.2 inch

7 = SH1106

8 = ILI9488 =\> ER-TFTM035-6

Index colors selected in the color panel ILI9488 with Ci and Bi

0=ILI9488\_BLACK

1=ILI9488\_WHITE

2=ILI9488\_RED

3=ILI9488\_GREEN

4=ILI9488\_BLUE

5=ILI9488\_CYAN

6=ILI9488\_MAGENTA

7=ILI9488\_YELLOW

8=ILI9488\_NAVY

9=ILI9488\_DARKGREEN

10=ILI9488\_DARKCYAN

11=ILI9488\_MAROON

12=ILI9488\_PURPLE

13=ILI9488\_OLIVE

14=ILI9488\_LIGHTGREY

15=ILI9488\_DARKGREY

16=ILI9488\_ORANGE

17=ILI9488\_GREENYELLOW

18=ILI9488\_PINK
