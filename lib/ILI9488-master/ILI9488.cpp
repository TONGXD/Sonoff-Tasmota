/***************************************************
  STM32 Support added by Jaret Burkett at OSHlab.com

  This is our library for the Adafruit ILI9488 Breakout and Shield
  ----> http://www.adafruit.com/products/1651

  Check out the links above for our tutorials and wiring diagrams
  These displays use SPI to communicate, 4 or 5 pins are required to
  interface (RST is optional)
  Adafruit invests time and resources providing this open source code,
  please support Adafruit and open-source hardware by purchasing
  products from Adafruit!

  Written by Limor Fried/Ladyada for Adafruit Industries.
  MIT license, all text above must be included in any redistribution
 ****************************************************/

#include "ILI9488.h"
#include <limits.h>

// if using software spi this optimizes the code
#define SWSPI_OPTMODE


const uint16_t ili9488_colors[]={ILI9488_BLACK,ILI9488_WHITE,ILI9488_RED,ILI9488_GREEN,ILI9488_BLUE,ILI9488_CYAN,ILI9488_MAGENTA,\
  ILI9488_YELLOW,ILI9488_NAVY,ILI9488_DARKGREEN,ILI9488_DARKCYAN,ILI9488_MAROON,ILI9488_PURPLE,ILI9488_OLIVE,\
ILI9488_LIGHTGREY,ILI9488_DARKGREY,ILI9488_ORANGE,ILI9488_GREENYELLOW,ILI9488_PINK};

// Constructor when using software SPI.  All output pins are configurable.
ILI9488::ILI9488(int8_t cs,int8_t mosi,int8_t sclk,int8_t bp) : Renderer(ILI9488_TFTWIDTH, ILI9488_TFTHEIGHT) {
  _cs   = cs;
  _mosi  = mosi;
  _sclk = sclk;
  _bp = bp;
  _hwspi = 0;
}


#include "spi_register.h"
#include <spi.h>
/*

CPU Clock = 80 Mhz
max clock of display is 15 Mhz (66ns sclk cycle)
so cpu/8 => 10 Mhz should be ok

HSPI CLK	5	GPIO14
HSPI /CS	8	GPIO15
HSPI MOSI	7	GPIO13
HSPI MISO	6	GPIO12


GPIO names for your easy reference:
GPIO0:	PERIPHS_IO_MUX_GPIO0_U
GPIO1:	PERIPHS_IO_MUX_U0TXD_U
GPIO2:	PERIPHS_IO_MUX_GPIO2_U
GPIO3:	PERIPHS_IO_MUX_U0RXD_U
GPIO4:	PERIPHS_IO_MUX_GPIO4_U
GPIO5:	PERIPHS_IO_MUX_GPIO5_U
GPIO6:	PERIPHS_IO_MUX_SD_CLK_U
GPIO7:	PERIPHS_IO_MUX_SD_DATA0_U
GPIO8:	PERIPHS_IO_MUX_SD_DATA1_U
GPIO9:	PERIPHS_IO_MUX_SD_DATA2_U
GPIO10:	PERIPHS_IO_MUX_SD_DATA3_U
GPIO11:	PERIPHS_IO_MUX_SD_CMD_U
GPIO12:	PERIPHS_IO_MUX_MTDI_U
GPIO13:	PERIPHS_IO_MUX_MTCK_U
GPIO14:	PERIPHS_IO_MUX_MTMS_U
GPIO15:	PERIPHS_IO_MUX_MTDO_U
*/

// code from espressif SDK
/******************************************************************************
 * FunctionName : spi_lcd_mode_init
 * Description  : SPI master initial function for driving LCD 3 wire spi
*******************************************************************************/
void spi_lcd_mode_init()
{
	uint32 regvalue;
	//bit9 of PERIPHS_IO_MUX should be cleared when HSPI clock doesn't equal CPU clock
	//bit8 of PERIPHS_IO_MUX should be cleared when SPI clock doesn't equal CPU clock

	WRITE_PERI_REG(PERIPHS_IO_MUX, 0x105); //clear bit9
		//PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDI_U, 2);//configure miso to spi mode
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTCK_U, 2);//configure mosi to spi mode
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTMS_U, 2);//configure sclk to spi mode
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDO_U, 2);//configure cs to spi mode

// the current implementation leaves about 1 us between transfers ????
// due to lack of documentation i could not find the reason
// skipping this would double the speed !!!

	//SET_PERI_REG_MASK(SPI_USER(1), SPI_CS_SETUP|SPI_CS_HOLD|SPI_USR_COMMAND);

  SPI1U1=0;

  SET_PERI_REG_MASK(SPI_USER(1), SPI_USR_COMMAND);

	CLEAR_PERI_REG_MASK(SPI_USER(1), SPI_FLASH_MODE);
	// SPI clock=CPU clock/8 => 10 Mhz
	WRITE_PERI_REG(SPI_CLOCK(1),
					((1&SPI_CLKDIV_PRE)<<SPI_CLKDIV_PRE_S)|
					((3&SPI_CLKCNT_N)<<SPI_CLKCNT_N_S)|
					((1&SPI_CLKCNT_H)<<SPI_CLKCNT_H_S)|
					((3&SPI_CLKCNT_L)<<SPI_CLKCNT_L_S)); //clear bit 31,set SPI clock div
// will result in 80/6 = 13,3 Mhz
 SPI.setFrequency(15000000);
}

#if 0
// code from espressif SDK
/******************************************************************************
 * FunctionName : spi_lcd_9bit_write
 * Description  : SPI 9bits transmission function for driving LCD TM035PDZV36
 * Parameters   : 	uint8 spi_no - SPI module number, Only "SPI" and "HSPI" are valid
 *				uint8 high_bit - first high bit of the data, 0 is for "0",the other value 1-255 is for "1"
 *				uint8 low_8bit- the rest 8bits of the data.
*******************************************************************************/
void spi_lcd_9bit_write(uint8_t high_bit,uint8_t low_8bit)
{
	uint32_t regvalue;
	uint8_t bytetemp;

	if(high_bit)		bytetemp=(low_8bit>>1)|0x80;
	else				bytetemp=(low_8bit>>1)&0x7f;

	regvalue= ((8&SPI_USR_COMMAND_BITLEN)<<SPI_USR_COMMAND_BITLEN_S)|((uint32)bytetemp);		//configure transmission variable,9bit transmission length and first 8 command bit
	if(low_8bit&0x01) 	regvalue|=BIT15;        //write the 9th bit
	while(READ_PERI_REG(SPI_CMD(1))&SPI_USR);		//waiting for spi module available
	WRITE_PERI_REG(SPI_USER2(1), regvalue);				//write  command and command length into spi reg
	SET_PERI_REG_MASK(SPI_CMD(1), SPI_USR);		//transmission start
}
#endif

// dc = 0
void ILI9488::writecommand(uint8_t c) {
  if (_hwspi) {
    uint32_t regvalue;
    uint8_t bytetemp;
    bytetemp=(c>>1)&0x7f;

//#define SPI_USR_COMMAND_BITLEN 0x0000000F
//#define SPI_USR_COMMAND_BITLEN_S 28

    regvalue= ((8&SPI_USR_COMMAND_BITLEN)<<SPI_USR_COMMAND_BITLEN_S)|((uint32)bytetemp);		//configure transmission variable,9bit transmission length and first 8 command bit
    if(c&0x01) 	regvalue|=BIT15;        //write the 9th bit
    while(READ_PERI_REG(SPI_CMD(1))&SPI_USR);		//waiting for spi module available
    WRITE_PERI_REG(SPI_USER2(1), regvalue);				//write  command and command length into spi reg
    SET_PERI_REG_MASK(SPI_CMD(1), SPI_USR);		//transmission start
  } else fastSPIwrite(c,0);
}

// dc = 1
void ILI9488::writedata(uint8_t d) {
  if (_hwspi) {
    uint32_t regvalue;
    uint8_t bytetemp;
    bytetemp=(d>>1)|0x80;

    regvalue= ((8&SPI_USR_COMMAND_BITLEN)<<SPI_USR_COMMAND_BITLEN_S)|((uint32)bytetemp);		//configure transmission variable,9bit transmission length and first 8 command bit
    if(d&0x01) 	regvalue|=BIT15;        //write the 9th bit
    while(READ_PERI_REG(SPI_CMD(1))&SPI_USR);		//waiting for spi module available
    WRITE_PERI_REG(SPI_USER2(1), regvalue);				//write  command and command length into spi reg
    SET_PERI_REG_MASK(SPI_CMD(1), SPI_USR);		//transmission start
  } else  fastSPIwrite(d,1);
}

// Rather than a bazillion writecommand() and writedata() calls, screen
// initialization commands and arguments are organized in these tables
// stored in PROGMEM.  The table may look bulky, but that's mostly the
// formatting -- storage-wise this is hundreds of bytes more compact
// than the equivalent code.  Companion function follows.
#define DELAY 0x80


// Companion code to the above tables.  Reads and issues
// a series of LCD commands stored in PROGMEM byte array.
void ILI9488::commandList(uint8_t *addr) {

  uint8_t  numCommands, numArgs;
  uint16_t ms;

  numCommands = pgm_read_byte(addr++);   // Number of commands to follow
  while(numCommands--) {                 // For each command...
    writecommand(pgm_read_byte(addr++)); //   Read, issue command
    numArgs  = pgm_read_byte(addr++);    //   Number of args to follow
    ms       = numArgs & DELAY;          //   If hibit set, delay follows args
    numArgs &= ~DELAY;                   //   Mask out delay bit
    while(numArgs--) {                   //   For each argument...
      writedata(pgm_read_byte(addr++));  //     Read, issue argument
    }

    if(ms) {
      ms = pgm_read_byte(addr++); // Read post-command delay time (ms)
      if(ms == 255) ms = 500;     // If 255, delay for 500 ms
      delay(ms);
    }
  }
}

uint16_t ILI9488::GetColorFromIndex(uint8_t index) {
  if (index>=sizeof(ili9488_colors)/2) index=0;
  return ili9488_colors[index];
}

void ILI9488::DisplayInit(int8_t p,int8_t size,int8_t rot,int8_t font) {
  setRotation(rot);
  invertDisplay(false);
  setTextWrap(false);         // Allow text to run off edges
  cp437(true);
  setTextFont(font&3);
  setTextSize(size&7);
  setTextColor(ILI9488_WHITE,ILI9488_BLACK);
  setCursor(0,0);
  fillScreen(ILI9488_BLACK);
}

void ILI9488::DisplayOnff(int8_t on) {
  if (on) {
    writecommand(ILI9488_DISPON);    //Display on
    if (_bp>=0) {
      digitalWrite(_bp,HIGH);
      /*
      writecommand(ILI9488_WRCTRLD);
    	writedata(0x0c);
      writecommand(ILI9488_CAPC9);
      writedata(0x3f);*/
    }
  } else {
    writecommand(ILI9488_DISPOFF);
    if (_bp>=0) {
      digitalWrite(_bp,LOW);
      //writecommand(ILI9488_WRCTRLD);
    	//writedata(0x04);
    }
  }
}


void ILI9488::begin(void) {
  pinMode(_cs, OUTPUT);
  pinMode(_sclk, OUTPUT);
  pinMode(_mosi, OUTPUT);
  if (_bp>=0) {
    pinMode(_bp, OUTPUT);
    digitalWrite(_bp,HIGH);
  }
  if ((_sclk==14) && (_mosi==13) && (_cs==15)) {
    // we use hardware spi
      _hwspi=1;
      spi_lcd_mode_init();
  } else {
    // we must use software spi
      _hwspi=0;
  }

  writecommand(0xE0);
	writedata(0x00);
	writedata(0x03);
	writedata(0x09);
	writedata(0x08);
	writedata(0x16);
	writedata(0x0A);
	writedata(0x3F);
	writedata(0x78);
	writedata(0x4C);
	writedata(0x09);
	writedata(0x0A);
	writedata(0x08);
	writedata(0x16);
	writedata(0x1A);
	writedata(0x0F);


	writecommand(0XE1);
	writedata(0x00);
	writedata(0x16);
	writedata(0x19);
	writedata(0x03);
	writedata(0x0F);
	writedata(0x05);
	writedata(0x32);
	writedata(0x45);
	writedata(0x46);
	writedata(0x04);
	writedata(0x0E);
	writedata(0x0D);
	writedata(0x35);
	writedata(0x37);
	writedata(0x0F);

	writecommand(0XC0);      //Power Control 1
	writedata(0x17);    //Vreg1out
	writedata(0x15);    //Verg2out

	writecommand(0xC1);      //Power Control 2
	writedata(0x41);    //VGH,VGL

	writecommand(0xC5);      //Power Control 3
	writedata(0x00);
	writedata(0x12);    //Vcom
	writedata(0x80);

	writecommand(0x36);      //Memory Access
	writedata(0x48);

	writecommand(0x3A);      // Interface Pixel Format
	writedata(0x66); 	  //18 bit

	writecommand(0XB0);      // Interface Mode Control
	writedata(0x80);     			 //SDO NOT USE

	writecommand(0xB1);      //Frame rate
	writedata(0xA0);    //60Hz

	writecommand(0xB4);      //Display Inversion Control
	writedata(0x02);    //2-dot

	writecommand(0XB6);      //Display Function Control  RGB/MCU Interface Control

	writedata(0x02);    //MCU
	writedata(0x02);    //Source,Gate scan dieection

	writecommand(0XE9);      // Set Image Functio
	writedata(0x00);    // Disable 24 bit data

	writecommand(0xF7);      // Adjust Control
	writedata(0xA9);
	writedata(0x51);
	writedata(0x2C);
	writedata(0x82);    // D7 stream, loose

  writecommand(ILI9488_SLPOUT);    //Exit Sleep
  delay(120);
  writecommand(ILI9488_DISPON);    //Display on

}

void ILI9488::setScrollArea(uint16_t topFixedArea, uint16_t bottomFixedArea){
  writecommand(0x33); // Vertical scroll definition
  writedata(topFixedArea >> 8);
  writedata(topFixedArea);
  writedata((_height - topFixedArea - bottomFixedArea) >> 8);
  writedata(_height - topFixedArea - bottomFixedArea);
  writedata(bottomFixedArea >> 8);
  writedata(bottomFixedArea);
}
void ILI9488::scroll(uint16_t pixels){
  writecommand(0x37); // Vertical scrolling start address
  writedata(pixels >> 8);
  writedata(pixels);
}

void ILI9488::setAddrWindow(uint16_t x0, uint16_t y0, uint16_t x1,
 uint16_t y1) {

  writecommand(ILI9488_CASET); // Column addr set
  writedata(x0 >> 8);
  writedata(x0 & 0xFF);     // XSTART
  writedata(x1 >> 8);
  writedata(x1 & 0xFF);     // XEND

  writecommand(ILI9488_PASET); // Row addr set
  writedata(y0>>8);
  writedata(y0 &0xff);     // YSTART
  writedata(y1>>8);
  writedata(y1 &0xff);     // YEND

  writecommand(ILI9488_RAMWR); // write to RAM

}

/*
void ILI9488::drawImage(const uint8_t* img, uint16_t x, uint16_t y, uint16_t w, uint16_t h){
return;
    // rudimentary clipping (drawChar w/big text requires this)
    if((x >= _width) || (y >= _height)) return;
    if((x + w - 1) >= _width)  w = _width  - x;
    if((y + h - 1) >= _height) h = _height - y;

    setAddrWindow(x, y, x+w-1, y+h-1);

    // uint8_t hi = color >> 8, lo = color;

  #if defined(USE_FAST_PINIO) && !defined (_VARIANT_ARDUINO_STM32_)
    *dcport |=  dcpinmask;
    *csport &= ~cspinmask;
  #else
    digitalWrite(_dc, HIGH);
    digitalWrite(_cs, LOW);
  #endif
  uint8_t linebuff[w*3+1];
  uint16_t pixels = w*h;
  // uint16_t count = 0;
  uint32_t count = 0;
  for (uint16_t i = 0; i < h; i++) {
    uint16_t pixcount = 0;
    for (uint16_t o = 0; o <  w; o++) {
      uint8_t b1 = img[count];
      count++;
      uint8_t b2 = img[count];
      count++;
      uint16_t color = b1 << 8 | b2;
      linebuff[pixcount] = (((color & 0xF800) >> 11)* 255) / 31;
      pixcount++;
      linebuff[pixcount] = (((color & 0x07E0) >> 5) * 255) / 63;
      pixcount++;
      linebuff[pixcount] = ((color & 0x001F)* 255) / 31;
      pixcount++;
    } // for row
    #if defined (__STM32F1__)
      SPI.dmaSend(linebuff, w*3);
    #else
      for(uint16_t b = 0; b < w*3; b++){
        spiwrite(linebuff[b]);
      }
    #endif

  }// for col
  #if defined(USE_FAST_PINIO) && !defined (_VARIANT_ARDUINO_STM32_)
    *csport |= cspinmask;
  #else
    digitalWrite(_cs, HIGH);
  #endif

    if (hwSPI) spi_end();
}
*/

void ILI9488::pushColor(uint16_t color) {
  write16BitColor(color);
}

void ILI9488::pushColors(uint16_t *data, uint8_t len, boolean first) {
  uint16_t color;
  uint8_t  buff[len*3+1];
  uint16_t count = 0;
  uint8_t lencount = len;
  while(lencount--) {
    color = *data++;
    buff[count] = (((color & 0xF800) >> 11)* 255) / 31;
    count++;
    buff[count] = (((color & 0x07E0) >> 5) * 255) / 63;
    count++;
    buff[count] = ((color & 0x001F)* 255) / 31;
    count++;
  }

    for(uint16_t b = 0; b < len*3; b++){
      writedata(buff[b]);
    }

}

void ILI9488::write16BitColor(uint16_t color){
  // #if (__STM32F1__)
  //     uint8_t buff[4] = {
  //       (((color & 0xF800) >> 11)* 255) / 31,
  //       (((color & 0x07E0) >> 5) * 255) / 63,
  //       ((color & 0x001F)* 255) / 31
  //     };
  //     SPI.dmaSend(buff, 3);
  // #else
  uint8_t r = (color & 0xF800) >> 11;
  uint8_t g = (color & 0x07E0) >> 5;
  uint8_t b = color & 0x001F;

  r = (r * 255) / 31;
  g = (g * 255) / 63;
  b = (b * 255) / 31;

  #ifndef SWSPI_OPTMODE
      writedata(r);
      writedata(g);
      writedata(b);
  #else
      if (_hwspi) {
        writedata(r);
        writedata(g);
        writedata(b);
      } else {
        WRITE_PERI_REG( PIN_OUT_CLEAR, 1<<_cs);
        WRITE_PERI_REG( PIN_OUT_CLEAR, 1<<_sclk);
        WRITE_PERI_REG( PIN_OUT_SET, 1<<_mosi);
        WRITE_PERI_REG( PIN_OUT_SET, 1<<_sclk);

        for(uint8_t bit = 0x80; bit; bit >>= 1) {
          WRITE_PERI_REG( PIN_OUT_CLEAR, 1<<_sclk);
          if(r&bit) WRITE_PERI_REG( PIN_OUT_SET, 1<<_mosi);
          else   WRITE_PERI_REG( PIN_OUT_CLEAR, 1<<_mosi);
          WRITE_PERI_REG( PIN_OUT_SET, 1<<_sclk);
        }

        WRITE_PERI_REG( PIN_OUT_CLEAR, 1<<_sclk);
        WRITE_PERI_REG( PIN_OUT_SET, 1<<_mosi);
        WRITE_PERI_REG( PIN_OUT_SET, 1<<_sclk);

        for(uint8_t bit = 0x80; bit; bit >>= 1) {
          WRITE_PERI_REG( PIN_OUT_CLEAR, 1<<_sclk);
          if(g&bit) WRITE_PERI_REG( PIN_OUT_SET, 1<<_mosi);
          else   WRITE_PERI_REG( PIN_OUT_CLEAR, 1<<_mosi);
          WRITE_PERI_REG( PIN_OUT_SET, 1<<_sclk);
        }

        WRITE_PERI_REG( PIN_OUT_CLEAR, 1<<_sclk);
        WRITE_PERI_REG( PIN_OUT_SET, 1<<_mosi);
        WRITE_PERI_REG( PIN_OUT_SET, 1<<_sclk);

        for(uint8_t bit = 0x80; bit; bit >>= 1) {
          WRITE_PERI_REG( PIN_OUT_CLEAR, 1<<_sclk);
          if(b&bit) WRITE_PERI_REG( PIN_OUT_SET, 1<<_mosi);
          else   WRITE_PERI_REG( PIN_OUT_CLEAR, 1<<_mosi);
          WRITE_PERI_REG( PIN_OUT_SET, 1<<_sclk);
        }
        WRITE_PERI_REG( PIN_OUT_SET, 1<<_cs);
      }

   #endif
}

void ILI9488::drawPixel(int16_t x, int16_t y, uint16_t color) {

  if((x < 0) ||(x >= _width) || (y < 0) || (y >= _height)) return;

  setAddrWindow(x,y,x+1,y+1);

  write16BitColor(color);

}

void ILI9488::drawFastVLine(int16_t x, int16_t y, int16_t h,
 uint16_t color) {

  // Rudimentary clipping
  if((x >= _width) || (y >= _height)) return;

  if((y+h-1) >= _height)
    h = _height-y;

  setAddrWindow(x, y, x, y+h-1);

  uint8_t r = (color & 0xF800) >> 11;
  uint8_t g = (color & 0x07E0) >> 5;
  uint8_t b = color & 0x001F;

  r = (r * 255) / 31;
  g = (g * 255) / 63;
  b = (b * 255) / 31;

  while (h--) {
    #ifndef SWSPI_OPTMODE
        writedata(r);
        writedata(g);
        writedata(b);
    #else
        if (_hwspi) {
          writedata(r);
          writedata(g);
          writedata(b);
        } else {
          WRITE_PERI_REG( PIN_OUT_CLEAR, 1<<_cs);
          WRITE_PERI_REG( PIN_OUT_CLEAR, 1<<_sclk);
          WRITE_PERI_REG( PIN_OUT_SET, 1<<_mosi);
          WRITE_PERI_REG( PIN_OUT_SET, 1<<_sclk);

          for(uint8_t bit = 0x80; bit; bit >>= 1) {
            WRITE_PERI_REG( PIN_OUT_CLEAR, 1<<_sclk);
            if(r&bit) WRITE_PERI_REG( PIN_OUT_SET, 1<<_mosi);
            else   WRITE_PERI_REG( PIN_OUT_CLEAR, 1<<_mosi);
            WRITE_PERI_REG( PIN_OUT_SET, 1<<_sclk);
          }

          WRITE_PERI_REG( PIN_OUT_CLEAR, 1<<_sclk);
          WRITE_PERI_REG( PIN_OUT_SET, 1<<_mosi);
          WRITE_PERI_REG( PIN_OUT_SET, 1<<_sclk);

          for(uint8_t bit = 0x80; bit; bit >>= 1) {
            WRITE_PERI_REG( PIN_OUT_CLEAR, 1<<_sclk);
            if(g&bit) WRITE_PERI_REG( PIN_OUT_SET, 1<<_mosi);
            else   WRITE_PERI_REG( PIN_OUT_CLEAR, 1<<_mosi);
            WRITE_PERI_REG( PIN_OUT_SET, 1<<_sclk);
          }

          WRITE_PERI_REG( PIN_OUT_CLEAR, 1<<_sclk);
          WRITE_PERI_REG( PIN_OUT_SET, 1<<_mosi);
          WRITE_PERI_REG( PIN_OUT_SET, 1<<_sclk);

          for(uint8_t bit = 0x80; bit; bit >>= 1) {
            WRITE_PERI_REG( PIN_OUT_CLEAR, 1<<_sclk);
            if(b&bit) WRITE_PERI_REG( PIN_OUT_SET, 1<<_mosi);
            else   WRITE_PERI_REG( PIN_OUT_CLEAR, 1<<_mosi);
            WRITE_PERI_REG( PIN_OUT_SET, 1<<_sclk);
          }
          WRITE_PERI_REG( PIN_OUT_SET, 1<<_cs);
        }
    #endif
  }

}

void ILI9488::drawFastHLine(int16_t x, int16_t y, int16_t w,
  uint16_t color) {

  // Rudimentary clipping
  if((x >= _width) || (y >= _height)) return;
  if((x+w-1) >= _width)  w = _width-x;

  setAddrWindow(x, y, x+w-1, y);

  uint8_t r = (color & 0xF800) >> 11;
  uint8_t g = (color & 0x07E0) >> 5;
  uint8_t b = color & 0x001F;

  r = (r * 255) / 31;
  g = (g * 255) / 63;
  b = (b * 255) / 31;

  while (w--) {
#ifndef SWSPI_OPTMODE
    writedata(r);
    writedata(g);
    writedata(b);
#else
    if (_hwspi) {
      writedata(r);
      writedata(g);
      writedata(b);
    } else {
      WRITE_PERI_REG( PIN_OUT_CLEAR, 1<<_cs);
      WRITE_PERI_REG( PIN_OUT_CLEAR, 1<<_sclk);
      WRITE_PERI_REG( PIN_OUT_SET, 1<<_mosi);
      WRITE_PERI_REG( PIN_OUT_SET, 1<<_sclk);

      for(uint8_t bit = 0x80; bit; bit >>= 1) {
        WRITE_PERI_REG( PIN_OUT_CLEAR, 1<<_sclk);
        if(r&bit) WRITE_PERI_REG( PIN_OUT_SET, 1<<_mosi);
        else   WRITE_PERI_REG( PIN_OUT_CLEAR, 1<<_mosi);
        WRITE_PERI_REG( PIN_OUT_SET, 1<<_sclk);
      }

      WRITE_PERI_REG( PIN_OUT_CLEAR, 1<<_sclk);
      WRITE_PERI_REG( PIN_OUT_SET, 1<<_mosi);
      WRITE_PERI_REG( PIN_OUT_SET, 1<<_sclk);

      for(uint8_t bit = 0x80; bit; bit >>= 1) {
        WRITE_PERI_REG( PIN_OUT_CLEAR, 1<<_sclk);
        if(g&bit) WRITE_PERI_REG( PIN_OUT_SET, 1<<_mosi);
        else   WRITE_PERI_REG( PIN_OUT_CLEAR, 1<<_mosi);
        WRITE_PERI_REG( PIN_OUT_SET, 1<<_sclk);
      }

      WRITE_PERI_REG( PIN_OUT_CLEAR, 1<<_sclk);
      WRITE_PERI_REG( PIN_OUT_SET, 1<<_mosi);
      WRITE_PERI_REG( PIN_OUT_SET, 1<<_sclk);

      for(uint8_t bit = 0x80; bit; bit >>= 1) {
        WRITE_PERI_REG( PIN_OUT_CLEAR, 1<<_sclk);
        if(b&bit) WRITE_PERI_REG( PIN_OUT_SET, 1<<_mosi);
        else   WRITE_PERI_REG( PIN_OUT_CLEAR, 1<<_mosi);
        WRITE_PERI_REG( PIN_OUT_SET, 1<<_sclk);
      }
      WRITE_PERI_REG( PIN_OUT_SET, 1<<_cs);
    }
#endif
  }

}

// this moves 460 kbytes
// now at 475 ms with 13,3 Mhz clock
void ILI9488::fillScreen(uint16_t color) {
  //uint32_t time=millis();
  fillRect(0, 0,  _width, _height, color);
  //time=millis()-time;
  //Serial.printf("time %d ms\n",time);
}

#if 0
static inline void asm_nop_9bits() {
    // one bit time
    asm volatile ("nop");
    asm volatile ("nop");
    asm volatile ("nop");
    asm volatile ("nop");
    asm volatile ("nop");
    asm volatile ("nop");
    asm volatile ("nop");
    asm volatile ("nop");

    // one bit time
    asm volatile ("nop");
    asm volatile ("nop");
    asm volatile ("nop");
    asm volatile ("nop");
    asm volatile ("nop");
    asm volatile ("nop");
    asm volatile ("nop");
    asm volatile ("nop");

    // one bit time
    asm volatile ("nop");
    asm volatile ("nop");
    asm volatile ("nop");
    asm volatile ("nop");
    asm volatile ("nop");
    asm volatile ("nop");
    asm volatile ("nop");
    asm volatile ("nop");

    // one bit time
    asm volatile ("nop");
    asm volatile ("nop");
    asm volatile ("nop");
    asm volatile ("nop");
    asm volatile ("nop");
    asm volatile ("nop");
    asm volatile ("nop");
    asm volatile ("nop");

    // one bit time
    asm volatile ("nop");
    asm volatile ("nop");
    asm volatile ("nop");
    asm volatile ("nop");
    asm volatile ("nop");
    asm volatile ("nop");
    asm volatile ("nop");
    asm volatile ("nop");

    // one bit time
    asm volatile ("nop");
    asm volatile ("nop");
    asm volatile ("nop");
    asm volatile ("nop");
    asm volatile ("nop");
    asm volatile ("nop");
    asm volatile ("nop");
    asm volatile ("nop");

    // one bit time
    asm volatile ("nop");
    asm volatile ("nop");
    asm volatile ("nop");
    asm volatile ("nop");
    asm volatile ("nop");
    asm volatile ("nop");
    asm volatile ("nop");
    asm volatile ("nop");

    // one bit time
    asm volatile ("nop");
    asm volatile ("nop");
    asm volatile ("nop");
    asm volatile ("nop");
    asm volatile ("nop");
    asm volatile ("nop");
    asm volatile ("nop");
    asm volatile ("nop");

    // one bit time
    asm volatile ("nop");
    asm volatile ("nop");
    asm volatile ("nop");
    asm volatile ("nop");
    asm volatile ("nop");
    asm volatile ("nop");
    asm volatile ("nop");
    asm volatile ("nop");

    // extra bit
    asm volatile ("nop");
    asm volatile ("nop");
    asm volatile ("nop");
    asm volatile ("nop");
    asm volatile ("nop");
    asm volatile ("nop");
    asm volatile ("nop");
    asm volatile ("nop");

    // extra bit
    asm volatile ("nop");
    asm volatile ("nop");
    asm volatile ("nop");
    asm volatile ("nop");
    asm volatile ("nop");
    asm volatile ("nop");
    asm volatile ("nop");
    asm volatile ("nop");
}
#endif

//#define WAIT_9BITS asm_nop_9bits();
#define WAIT_9BITS
//#define WAIT_BEFORE while(*((uint32_t *)0x60000100)&SPI_USR);		//waiting for spi module available
#define WAIT_SPI_READY while(READ_PERI_REG(SPI_CMD(1))&SPI_USR);

//#define WAIT_SPI_READY

//#define DIAG_PIN_SET WRITE_PERI_REG( PIN_OUT_SET, 1<<2);
//#define DIAG_PIN_CLR WRITE_PERI_REG( PIN_OUT_CLEAR, 1<<2);
#define DIAG_PIN_SET
#define DIAG_PIN_CLR

//#define WRITE_PERI_REG(addr, val) (*((volatile uint32_t *)ETS_UNCACHED_ADDR(addr))) = (uint32_t)(val)


// CMD 0x60000200-1*0x100
// SPI_USER2 0x60000200-1*0x100 + 0x24

//#define WRITE_SPI_REG WRITE_PERI_REG(0x60000124, regvalue_r);SET_PERI_REG_MASK(0x60000100, SPI_USR);
// THIS TAKES 1 us => 80 cpu clock cycles !!!!!!!!!!!!!!!!!!!!!!!!!!
// probably the memw causes this delay
#define WRITE_SPI_REG(A) WRITE_PERI_REG(SPI_USER2(1), A);	SET_PERI_REG_MASK(SPI_CMD(1), SPI_USR);

//#define WRITE_SPI_REG(A) *((uint32_t *)0x60000124)=A; *((uint32_t *)0x60000100)|=SPI_USR;

//#define WRITE_SPI_REG

// this enables the 27 bit packed mode
#define RGB_PACK_MODE

// extremely strange => if this code is merged into pack_rgb() the software crashes
// swap bytes
uint32_t ulswap(uint32_t data) {
union {
        uint32_t l;
        uint8_t b[4];
} data_;

  data_.l = data;
  // MSBFIRST Byte first
  data = (data_.b[3] | (data_.b[2] << 8) | (data_.b[1] << 16) | (data_.b[0] << 24));
  return data;
}

// pack RGB into uint32
uint32_t pack_rgb(uint32_t r, uint32_t g, uint32_t b) {
  uint32_t data;
  data=r<<23;
  data|=g<<14;
  data|=b<<5;
  data|=0b10000000010000000010000000000000;
  return ulswap(data);
}

// fill a rectangle
void ILI9488::fillRect(int16_t x, int16_t y, int16_t w, int16_t h,
  uint16_t color) {

  // rudimentary clipping (drawChar w/big text requires this)
  if((x >= _width) || (y >= _height)) return;
  if((x + w - 1) >= _width)  w = _width  - x;
  if((y + h - 1) >= _height) h = _height - y;

  setAddrWindow(x, y, x+w-1, y+h-1);

  uint8_t r = (color & 0xF800) >> 11;
  uint8_t g = (color & 0x07E0) >> 5;
  uint8_t b = color & 0x001F;

  r = (r * 255) / 31;
  g = (g * 255) / 63;
  b = (b * 255) / 31;

  uint32_t regvalue_r,regvalue_g,regvalue_b;
  uint32_t data;

  if (_hwspi) {
    // precalculate the register values for rgb

#ifndef RGB_PACK_MODE
    uint8_t bytetemp;
    bytetemp=(r>>1)|0x80;
    regvalue_r= ((8&SPI_USR_COMMAND_BITLEN)<<SPI_USR_COMMAND_BITLEN_S)|((uint32)bytetemp);		//configure transmission variable,9bit transmission length and first 8 command bit
    if(r&0x01) 	regvalue_r|=BIT15;        //write the 9th bit

    bytetemp=(g>>1)|0x80;
    regvalue_g= ((8&SPI_USR_COMMAND_BITLEN)<<SPI_USR_COMMAND_BITLEN_S)|((uint32)bytetemp);		//configure transmission variable,9bit transmission length and first 8 command bit
    if(g&0x01) 	regvalue_g|=BIT15;        //write the 9th bit

    bytetemp=(b>>1)|0x80;
    regvalue_b= ((8&SPI_USR_COMMAND_BITLEN)<<SPI_USR_COMMAND_BITLEN_S)|((uint32)bytetemp);		//configure transmission variable,9bit transmission length and first 8 command bit
    if(b&0x01) 	regvalue_b|=BIT15;        //write the 9th bit

#else
    // init 27 bit mode
    uint16_t bits=27;
    //const uint32_t mask = ~((SPIMMOSI << SPILMOSI) | (SPIMMISO << SPILMISO));
    const uint32_t mask = ~((SPIMMOSI << SPILMOSI) );
    bits--;
    SPI1U1 = ((SPI1U1 & mask) | ((bits << SPILMOSI)));
    SPI1C = 0;
    SPI1C &= ~(SPICWBO | SPICRBO); // => MSBFIRST
    SPI1U = SPIUMOSI;
    SPI1C1 = 0;
    data=pack_rgb(r,g,b);

#endif
  }

  for(y=h; y>0; y--) {
    delay(0);
    for(x=w; x>0; x--) {

#ifndef SWSPI_OPTMODE
      writedata(r);
      writedata(g);
      writedata(b);
#else
      if (_hwspi) {
        //noInterrupts();
#ifdef RGB_PACK_MODE
        while(SPI1CMD & SPIBUSY) {}
        SPI1W0 = data;
        SPI1CMD |= SPIBUSY;
#else
        DIAG_PIN_CLR
        WAIT_SPI_READY
        DIAG_PIN_SET

        WRITE_SPI_REG(regvalue_r)
        WAIT_9BITS

        DIAG_PIN_CLR
        WAIT_SPI_READY
        DIAG_PIN_SET

        WRITE_SPI_REG(regvalue_g)
        WAIT_9BITS

        DIAG_PIN_CLR
        WAIT_SPI_READY
        DIAG_PIN_SET

        WRITE_SPI_REG(regvalue_b)
        WAIT_9BITS

        //interrupts();
#endif
      } else {
        WRITE_PERI_REG( PIN_OUT_CLEAR, 1<<_cs);

        WRITE_PERI_REG( PIN_OUT_CLEAR, 1<<_sclk);
        WRITE_PERI_REG( PIN_OUT_SET, 1<<_mosi);
        WRITE_PERI_REG( PIN_OUT_SET, 1<<_sclk);

        for(uint8_t bit = 0x80; bit; bit >>= 1) {
          WRITE_PERI_REG( PIN_OUT_CLEAR, 1<<_sclk);
          if(r&bit) WRITE_PERI_REG( PIN_OUT_SET, 1<<_mosi);
          else   WRITE_PERI_REG( PIN_OUT_CLEAR, 1<<_mosi);
          WRITE_PERI_REG( PIN_OUT_SET, 1<<_sclk);
        }

        WRITE_PERI_REG( PIN_OUT_CLEAR, 1<<_sclk);
        WRITE_PERI_REG( PIN_OUT_SET, 1<<_mosi);
        WRITE_PERI_REG( PIN_OUT_SET, 1<<_sclk);

        for(uint8_t bit = 0x80; bit; bit >>= 1) {
          WRITE_PERI_REG( PIN_OUT_CLEAR, 1<<_sclk);
          if(g&bit) WRITE_PERI_REG( PIN_OUT_SET, 1<<_mosi);
          else   WRITE_PERI_REG( PIN_OUT_CLEAR, 1<<_mosi);
          WRITE_PERI_REG( PIN_OUT_SET, 1<<_sclk);
        }

        WRITE_PERI_REG( PIN_OUT_CLEAR, 1<<_sclk);
        WRITE_PERI_REG( PIN_OUT_SET, 1<<_mosi);
        WRITE_PERI_REG( PIN_OUT_SET, 1<<_sclk);

        for(uint8_t bit = 0x80; bit; bit >>= 1) {
          WRITE_PERI_REG( PIN_OUT_CLEAR, 1<<_sclk);
          if(b&bit) WRITE_PERI_REG( PIN_OUT_SET, 1<<_mosi);
          else   WRITE_PERI_REG( PIN_OUT_CLEAR, 1<<_mosi);
          WRITE_PERI_REG( PIN_OUT_SET, 1<<_sclk);
        }
        WRITE_PERI_REG( PIN_OUT_SET, 1<<_cs);
      }
      #endif

    }

  }
#ifdef RGB_PACK_MODE
  // reinit old mode
  while(SPI1CMD & SPIBUSY) {}
  spi_lcd_mode_init();
#endif
}


// Pass 8-bit (each) R,G,B, get back 16-bit packed color
uint16_t ILI9488::color565(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}


#define MADCTL_MY  0x80
#define MADCTL_MX  0x40
#define MADCTL_MV  0x20
#define MADCTL_ML  0x10
#define MADCTL_RGB 0x00
#define MADCTL_BGR 0x08
#define MADCTL_MH  0x04

void ILI9488::setRotation(uint8_t m) {

  writecommand(ILI9488_MADCTL);
  rotation = m % 4; // can't be higher than 3
  switch (rotation) {
   case 0:
     writedata(MADCTL_MX | MADCTL_BGR);
     _width  = ILI9488_TFTWIDTH;
     _height = ILI9488_TFTHEIGHT;
     break;
   case 1:
     writedata(MADCTL_MV | MADCTL_BGR);
     _width  = ILI9488_TFTHEIGHT;
     _height = ILI9488_TFTWIDTH;
     break;
  case 2:
    writedata(MADCTL_MY | MADCTL_BGR);
     _width  = ILI9488_TFTWIDTH;
     _height = ILI9488_TFTHEIGHT;
    break;
   case 3:
     writedata(MADCTL_MX | MADCTL_MY | MADCTL_MV | MADCTL_BGR);
     _width  = ILI9488_TFTHEIGHT;
     _height = ILI9488_TFTWIDTH;
     break;
  }
}


void ILI9488::invertDisplay(boolean i) {
  writecommand(i ? ILI9488_INVON : ILI9488_INVOFF);
}

void ICACHE_RAM_ATTR ILI9488::fastSPIwrite(uint8_t d,uint8_t dc) {

  WRITE_PERI_REG( PIN_OUT_CLEAR, 1<<_cs);
  WRITE_PERI_REG( PIN_OUT_CLEAR, 1<<_sclk);
  if(dc) WRITE_PERI_REG( PIN_OUT_SET, 1<<_mosi);
  else   WRITE_PERI_REG( PIN_OUT_CLEAR, 1<<_mosi);
  WRITE_PERI_REG( PIN_OUT_SET, 1<<_sclk);

  for(uint8_t bit = 0x80; bit; bit >>= 1) {
    WRITE_PERI_REG( PIN_OUT_CLEAR, 1<<_sclk);
    if(d&bit) WRITE_PERI_REG( PIN_OUT_SET, 1<<_mosi);
    else   WRITE_PERI_REG( PIN_OUT_CLEAR, 1<<_mosi);
    WRITE_PERI_REG( PIN_OUT_SET, 1<<_sclk);
  }
  WRITE_PERI_REG( PIN_OUT_SET, 1<<_cs);
}

/*

 uint16_t ILI9488::readcommand16(uint8_t c) {
 digitalWrite(_dc, LOW);
 if (_cs)
 digitalWrite(_cs, LOW);

 spiwrite(c);
 pinMode(_sid, INPUT); // input!
 uint16_t r = spiread();
 r <<= 8;
 r |= spiread();
 if (_cs)
 digitalWrite(_cs, HIGH);

 pinMode(_sid, OUTPUT); // back to output
 return r;
 }

 uint32_t ILI9488::readcommand32(uint8_t c) {
 digitalWrite(_dc, LOW);
 if (_cs)
 digitalWrite(_cs, LOW);
 spiwrite(c);
 pinMode(_sid, INPUT); // input!

 dummyclock();
 dummyclock();

 uint32_t r = spiread();
 r <<= 8;
 r |= spiread();
 r <<= 8;
 r |= spiread();
 r <<= 8;
 r |= spiread();
 if (_cs)
 digitalWrite(_cs, HIGH);

 pinMode(_sid, OUTPUT); // back to output
 return r;
 }

 */
