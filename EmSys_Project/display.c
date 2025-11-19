#include "display.h"
#include <util/delay.h>
#include <string.h>
#include <avr/pgmspace.h>
#include "glcdfont.c"
#include "timers.h"

#define CS_LOW()   DISPLAY_PORT &= ~(1 << DISPLAY_CS)                                                                                    // driver for setting Chip Select low
#define CS_HIGH()  DISPLAY_PORT |=  (1 << DISPLAY_CS)                                                                                    // driver for setting Chip Select high
#define DC_LOW()   DISPLAY_PORT &= ~(1 << DISPLAY_DC)                                                                                    // driver for setting Data/Command low     low  -> command
#define DC_HIGH()  DISPLAY_PORT |=  (1 << DISPLAY_DC)                                                                                    // driver for setting Data/Command high    high -> data
#define RST_LOW()  DISPLAY_PORT &= ~(1 << DISPLAY_RST)                                                                                   // driver for setting Reset low
#define RST_HIGH() DISPLAY_PORT |=  (1 << DISPLAY_RST)                                                                                   // driver for setting Reset high

#define PLOT_X_START   20                                                                                                                // starting x coord for the waveform viewer area
#define PLOT_X_END    260                                                                                                                // end x coord for the waveform viewer area
#define PLOT_Y_TOP     67                                                                                                                // top y coord for the waveform viewer area
#define PLOT_Y_BOTTOM 207                                                                                                                // bottom y coord for the waveform viewer area
#define PLOT_X_SPACING 10                                                                                                                // space between points on the plot

#define VOLTAGE_X_BEGIN 0                                                                                                                // starting x coord for the voltmeter area
#define VOLTAGE_X_END 184                                                                                                                // end x coord for the voltmeter area
#define VOLTAGE_Y_BEGIN 0                                                                                                                // top y coord for the voltmeter area
#define VOLTAGE_Y_END 35                                                                                                                 // bottom y coord for the voltmeter area

#define MAX_POINTS 25                                                                                                                    // number of recorded points

typedef struct {                                                                                                                         // struct which holds the point data, saved after scaling to the voltage and plotting them
	uint16_t x;                                                                                                                          // x coord
	uint16_t y;                                                                                                                          // y coord
	float value;                                                                                                                         // voltage value
} PlotPoint;

PlotPoint plot_coords[MAX_POINTS];
int plot_index = 0;
int plot_count = 0;

uint16_t display_width = 240;
uint16_t display_height = 320;

static uint16_t fg_color = 0xFFFF;
static uint16_t bg_color = 0x0000;
static uint8_t text_size = 1;

uint8_t display_rotation = 0;
int cursor_position = 0;

extern bool is_cursor_on;
extern bool is_plot_on;
extern bool is_holding;
extern bool is_digital_line;

void display_send_command(uint8_t cmd) {
	CS_LOW();                                                                                                                           // pull cs low to signal we talk to the display
	DC_LOW();                                                                                                                           // pull dc low to enter command mode
	SPI_transfer(cmd);                                                                                                                  // transfer the command
	CS_HIGH();                                                                                                                          // pull cs high to signal we stopped talking to the display
}

void display_data(uint8_t data) {
	CS_LOW();                                                                                                                           // pull cs low to signal we talk to the display
	DC_HIGH();                                                                                                                          // pull dc high to enter data mode
	SPI_transfer(data);                                                                                                                 // transfer the data
	 CS_HIGH();                                                                                                                         // pull cs high to signal we stopped talking to the display
}

void display_command_start(void) {
	DC_LOW();                                                                                                                           // pull dc low to enter command mode
	CS_LOW();                                                                                                                           // pull cs low to signal we talk to the display 
}

void display_command_end(void) {
	CS_HIGH();                                                                                                                          // pull cs high to signal we stopped talking to the display
	timer_delay_us(20);
}

void display_data_start(void) {
	DC_HIGH();                                                                                                                          // pull dc high to enter data mode
	CS_LOW();                                                                                                                           // pull cs low to signal we talk to the display
}

void display_data_end(void) {
	CS_HIGH();                                                                                                                          // pull cs high to signal we stopped talking to the display
	timer_delay_us(20);
}

void display_write_byte(uint8_t data) {
	SPI_transfer(data);                                                                                                                 // transfer data to the display
}

static const uint8_t PROGMEM initcmd[] = {                                                                                              // display initiation sequence
	0xEF, 3, 0x03, 0x80, 0x02,
	0xCF, 3, 0x00, 0xC1, 0x30,
	0xED, 4, 0x64, 0x03, 0x12, 0x81,
	0xE8, 3, 0x85, 0x00, 0x78,
	0xCB, 5, 0x39, 0x2C, 0x00, 0x34, 0x02,
	0xF7, 1, 0x20,
	0xEA, 2, 0x00, 0x00,
	ILI9341_PWCTR1  , 1, 0x23,
	ILI9341_PWCTR2  , 1, 0x10,
	ILI9341_VMCTR1  , 2, 0x3e, 0x28,
	ILI9341_VMCTR2  , 1, 0x86,
	ILI9341_MADCTL  , 1, 0x48,
	ILI9341_VSCRSADD, 1, 0x00,
	ILI9341_PIXFMT  , 1, 0x55,
	ILI9341_FRMCTR1 , 2, 0x00, 0x18,
	ILI9341_DFUNCTR , 3, 0x08, 0x82, 0x27,
	0xF2, 1, 0x00,
	ILI9341_GAMMASET , 1, 0x01,
	ILI9341_GMCTRP1 , 15, 0x0F, 0x31, 0x2B, 0x0C, 0x0E, 0x08,
	0x4E, 0xF1, 0x37, 0x07, 0x10, 0x03, 0x0E, 0x09, 0x00,
	ILI9341_GMCTRN1 , 15, 0x00, 0x0E, 0x14, 0x03, 0x11, 0x07,
	0x31, 0xC1, 0x48, 0x08, 0x0F, 0x0C, 0x31, 0x36, 0x0F,
	ILI9341_SLPOUT  , 0x80,
	ILI9341_DISPON  , 0x80,
	0x00
};

void Display_Init(void) {
	DISPLAY_DDR |= (1 << DISPLAY_CS) | (1 << DISPLAY_DC) | (1 << DISPLAY_RST);                                                          // set pins as output
	CS_HIGH();	                                                                                                                        // and put them high (idle)
	DC_HIGH();
	RST_HIGH();

	RST_LOW();                                                                                                                          // hardware reset
	timer_delay_us(20);
	RST_HIGH();
	timer_delay_ms(150);

	const uint8_t *addr = initcmd;                                                                                                      // get the addr to the display init sequence
	uint8_t cmd, x, numArgs;
	while ((cmd = pgm_read_byte(addr++)) > 0) {                                                                                         // and go through it 
		x = pgm_read_byte(addr++);                                                                                                      // read the byte
		numArgs = x & 0x7F;                                                                                                             // and get the data bits (lower 7 bits)

		display_command_start();                                                                                                        // get the display into command mode
		display_write_byte(cmd);                                                                                                        // write the data byte
		display_command_end();                                                                                                          // get display out of command mode

		if (numArgs) {                                                                                                                  // if the command has arguments
			display_data_start();                                                                                                       // send them as data bytes
			for (uint8_t i = 0; i < numArgs; i++) {
				display_write_byte(pgm_read_byte(addr++));
			}
			display_data_end();
		}

		if (x & 0x80) {                                                                                                                 // if the MSB is set, wait a delay
			timer_delay_ms(150);
		}
	}
}

void display_set_rotation(uint8_t mode) {                                                                                               // set the display orientation using MADCTL (memory access control reg)
	display_rotation = mode % 4;                                                                                                        // limit the mode to 0-3 range
	uint8_t madctl = 0;                                                                                                                 // variable to hold the madctl config bits

	switch (display_rotation) {
		case PORTRAIT:
			madctl = MADCTL_MX | MADCTL_BGR;                                                                                            // mirror x axis, use bgr color order
			display_width = 240;                                                                                                        // set width and height accordingly
			display_height = 320;
			break;
		case LANDSCAPE:
			madctl = MADCTL_MV | MADCTL_BGR;                                                                                            // swap x and y axis, use bgr color order
			display_width = 320;
			display_height = 240;
			break;
		case PORTRAIT_INVERTED:
			madctl = MADCTL_MY | MADCTL_BGR;                                                                                            // mirror y axis, use bgr color order
			display_width = 240;
			display_height = 320;
			break;
		case LANDSCAPE_INVERTED:
			madctl = MADCTL_MX | MADCTL_MY | MADCTL_MV | MADCTL_BGR;                                                                    // mirror both axes and swap x/y
			display_width = 320;
			display_height = 240;
			break;
	}

	display_send_command(ILI9341_MADCTL);                                                                                               // send the madctl command to the display
	display_data(madctl);                                                                                                               // send the orientation config byte
}

void display_set_window(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
	uint16_t x2 = x + w - 1;                                                                                                            // calculate the end x coordinate
	uint16_t y2 = y + h - 1;                                                                                                            // calculate the end y coordinate
                                                                                             
	display_send_command(ILI9341_CASET);                                                                                                // set column address range
	display_data(x >> 8);                                                                                                               // send high byte of x start
	display_data(x & 0xFF);                                                                                                             // send low byte of x start
	display_data(x2 >> 8);                                                                                                              // send high byte of x end
	display_data(x2 & 0xFF);                                                                                                            // send low byte of x end
                                                                                             
	display_send_command(ILI9341_PASET);                                                                                                // set page (row) address range
	display_data(y >> 8);                                                                                                               // send high byte of y start
	display_data(y & 0xFF);                                                                                                             // send low byte of y start
	display_data(y2 >> 8);                                                                                                              // send high byte of y end
	display_data(y2 & 0xFF);                                                                                                            // send low byte of y end
                                                                                             
	display_send_command(ILI9341_RAMWR);                                                                                                // prepare to write to display memory
}


void set_text_size(uint8_t size) {
    if (size < 1) size = 1;                                                                                                             // make sure the size is at least 1
    text_size = size;                                                                                                                   // set the global text size
}

void display_set_color(uint16_t color) {
    fg_color = color;                                                                                                                   // set the foreground color
}

void display_set_background(uint16_t color) {
    bg_color = color;                                                                                                                   // set the background color
}

void display_write_color(uint16_t color) {
    display_data_start();                                                                                                               // enter data mode
    display_write_byte(color >> 8);                                                                                                     // send high byte of color
    display_write_byte(color & 0xFF);                                                                                                   // send low byte of color
    display_data_end();                                                                                                                 // exit data mode
}

void display_write(void) {
    display_write_color(fg_color);                                                                                                      // write the foreground color
}

void display_write_background(void) {
    display_write_color(bg_color);                                                                                                      // write the background color
}


void display_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
	uint8_t hi = color >> 8;                                                                                                            // get high byte of the color
	uint8_t lo = color & 0xFF;                                                                                                          // get low byte of the color

	display_set_window(x, y, w, h);                                                                                                     // set the active window area on the display
	display_send_command(ILI9341_RAMWR);                                                                                                // tell the display we want to write to memory
	display_data_start();                                                                                                               // enter data mode

	for (uint32_t i = 0; i < (uint32_t)w * h; i++) {
		SPI_transfer(hi);                                                                                                               // send high byte of color
		SPI_transfer(lo);                                                                                                               // send low byte of color
	}

	display_data_end();                                                                                                                 // exit data mode
}


void display_draw_char(char c, uint16_t x, uint16_t y) {
	if (c < 32 || c > 127) return;                                                                                                      // skip non-printable characters

	for (uint8_t col = 0; col < 5; col++) {
		uint8_t line = pgm_read_byte(&font[c * 5 + col]);                                                                               // get the column bitmap from font array
		for (uint8_t row = 0; row < 8; row++) {
			uint16_t px = (line & (1 << row)) ? fg_color : bg_color;                                                                    // check if pixel is set, choose color
			display_fill_rect(x + col * text_size, y + row * text_size, text_size, text_size, px);                                      // draw scaled pixel
		}
	}

	display_fill_rect(x + 5 * text_size, y, text_size, 8 * text_size, bg_color);                                                        // draw spacing column
}


void display_print(const char* str, uint16_t x, uint16_t y) {
	while (*str) {
		display_draw_char(*str++, x, y);                                                                                                // draw the current character and move to the next
		x += 6 * text_size;                                                                                                             // move x position forward by character width
	}
}

void display_fill_color(uint16_t color) {
	uint8_t hi = color >> 8;                                                                                                            // get high byte of the color
	uint8_t lo = color & 0xFF;                                                                                                          // get low byte of the color

	display_set_window(0, 0, display_width, display_height);                                                                            // set full screen as active window
	display_send_command(ILI9341_RAMWR);                                                                                                // tell the display we want to write to memory
	display_data_start();                                                                                                               // enter data mode

	for (uint32_t i = 0; i < (uint32_t)display_width * display_height; i++) {
		SPI_transfer(hi);                                                                                                               // send high byte of color
		SPI_transfer(lo);                                                                                                               // send low byte of color
	}

	display_data_end();                                                                                                                 // exit data mode
}


void display_draw_pixel(uint16_t x, uint16_t y, uint16_t color) {
	display_fill_rect(x, y, 1, 1, color);                                                                                               // draw a single pixel using a 1x1 filled rectangle
}

void display_draw_line(int x0, int y0, int x1, int y1, uint16_t color) {                                                                // bresenham line algorithm
	int dx = abs(x1 - x0);                                                                                                              // calculate delta x
	int dy = -abs(y1 - y0);                                                                                                             // calculate delta y (negative for error calc)
	int sx = (x0 < x1) ? 1 : -1;                                                                                                        // step direction for x
	int sy = (y0 < y1) ? 1 : -1;                                                                                                        // step direction for y
	int err = dx + dy;                                                                                                                  // initial error term

	while (1) {
		display_draw_pixel(x0, y0, color);                                                                                              // draw pixel at current position
		if (x0 == x1 && y0 == y1) break;                                                                                                // stop if we've reached the end point
		int e2 = 2 * err;                                                                                                               // double the error for decision
		if (e2 >= dy) { err += dy; x0 += sx; }                                                                                          // move in x direction if needed
		if (e2 <= dx) { err += dx; y0 += sy; }                                                                                          // move in y direction if needed
	}
}


uint16_t scale_voltage_to_y(float value, float v_min, float v_max) {                                                                    // scale the waveform viewer's range automatically to better see the wave variations when differences are small
	if (value < v_min) value = v_min;                                                                                                   // clamp value to minimum
	if (value > v_max) value = v_max;                                                                                                   // clamp value to maximum

	float scale = (value - v_min) / (v_max - v_min);                                                                                    // normalize value between 0 and 1
	return PLOT_Y_BOTTOM - (uint16_t)(scale * (PLOT_Y_BOTTOM - PLOT_Y_TOP));                                                            // map to screen y range 
}

void store_points(float points[], int count, float v_min, float v_max) {
	if (count > MAX_POINTS)
	count = MAX_POINTS;

	for (int i = 0; i < count; i++) {
		uint16_t x = PLOT_X_START + i * PLOT_X_SPACING;
		uint16_t y = scale_voltage_to_y(points[i], v_min, v_max);

		plot_coords[i].x = x;
		plot_coords[i].y = y;
		plot_coords[i].value = points[i];
	}

	plot_count = count;
}



void plot_points_digital(float points[], int count, float v_max, float v_min) {
	if (count < 1)
		return;                                                                                                                         // skip if there are no points

	if(count > MAX_POINTS)                                                                                                              // limit the number of points to max allowed
		plot_count = MAX_POINTS;                                                                                                        // *should* always be ignored
	else
		plot_count = count;

	clear_plot_line(plot_count);                                                                                                        // clear previous frame using stored coords
	store_points(points, plot_count, v_min, v_max);                                                                                     // store the scaled points into the plot_coords buffer

	for (int i = 0; i < plot_count; i++) {                                                                                              // go through every point
		uint16_t x = plot_coords[i].x;                                                                                                  // get x coordinate from buffer
		uint16_t y = plot_coords[i].y;                                                                                                  // get y coordinate from buffer

		display_draw_line(x, PLOT_Y_BOTTOM, x, y, COLOR_GREEN);                                                                         // draw vertical line from bottom to voltage level
		display_draw_line(x - 2, y - 2, x + 2, y + 2, COLOR_BLUE);                                                                      // draw X marker on point position
		display_draw_line(x - 2, y + 2, x + 2, y - 2, COLOR_BLUE);
	}

	if(is_cursor_on)                                                                                                                   // if cursor is enabled, draw it
		draw_cursor();
	else
		erase_cursor();
}




void plot_points_line(float points[], int count, float v_max, float v_min) {
    if (count < 2) 
		return;

    clear_plot_line(plot_count);                      // clear previous frame using stored coords
    store_points(points, count, v_min, v_max);        // update buffer with new scaled points

    for (int i = 0; i < plot_count; i++) {
        uint16_t x0 = plot_coords[i].x;
        uint16_t y0 = plot_coords[i].y;

        display_draw_line(x0 - 2, y0 - 2, x0 + 2, y0 + 2, COLOR_BLUE);  // draw X marker
        display_draw_line(x0 - 2, y0 + 2, x0 + 2, y0 - 2, COLOR_BLUE);

        if (i < plot_count - 1) {
            uint16_t x1 = plot_coords[i + 1].x;
            uint16_t y1 = plot_coords[i + 1].y;
            display_draw_line(x0, y0, x1, y1, COLOR_GREEN);             // draw connecting line
        }
    }

    if (is_cursor_on)
        draw_cursor();
	else
		erase_cursor();
}


void restore_point(){
	if(is_digital_line){
		display_draw_line(plot_coords[cursor_position].x, 207, plot_coords[cursor_position].x, plot_coords[cursor_position].y, COLOR_GREEN);
	}
	
	display_draw_line(plot_coords[cursor_position].x - 2, plot_coords[cursor_position].y - 2, plot_coords[cursor_position].x + 2, plot_coords[cursor_position].y + 2, COLOR_BLUE);  // redraw blue X
	display_draw_line(plot_coords[cursor_position].x - 2, plot_coords[cursor_position].y + 2, plot_coords[cursor_position].x + 2, plot_coords[cursor_position].y - 2, COLOR_BLUE);
}

void clear_plot() { 
	display_fill_rect(PLOT_X_START - 2, PLOT_Y_TOP - 20, PLOT_X_END - PLOT_X_START + 20, PLOT_Y_BOTTOM - PLOT_Y_TOP + 43, COLOR_BLACK);  // clear the waveform viewer area
}

void toggle_plot() {
	display_fill_rect(PLOT_X_START - 2, PLOT_Y_TOP - 2, PLOT_X_END - PLOT_X_START + 5, PLOT_Y_BOTTOM - PLOT_Y_TOP + 5, COLOR_BLACK);  // clear the waveform viewer area
}

void clear_plot_line(int count){
	if (!is_digital_line) {
		display_draw_line(plot_coords[0].x - 2, plot_coords[0].y - 2, plot_coords[0].x + 2, plot_coords[0].y + 2, COLOR_BLACK);
		display_draw_line(plot_coords[0].x - 2, plot_coords[0].y + 2, plot_coords[0].x + 2, plot_coords[0].y - 2, COLOR_BLACK);
		for (int i = 1; i < count; i++) {
			display_draw_line(plot_coords[i].x - 2, plot_coords[i].y - 2, plot_coords[i].x + 2, plot_coords[i].y + 2, COLOR_BLACK);
			display_draw_line(plot_coords[i].x - 2, plot_coords[i].y + 2, plot_coords[i].x + 2, plot_coords[i].y - 2, COLOR_BLACK);
			display_draw_line(plot_coords[i - 1].x, plot_coords[i - 1].y, plot_coords[i].x, plot_coords[i].y, COLOR_BLACK);
		}
	}else {
		for (int i = 0; i < count; i++) {
			display_draw_line(plot_coords[i].x, PLOT_Y_BOTTOM, plot_coords[i].x, plot_coords[i].y, COLOR_BLACK);                          // clear vertical sample line
			display_draw_line(plot_coords[i].x - 2, plot_coords[i].y - 2, plot_coords[i].x + 2, plot_coords[i].y + 2, COLOR_BLACK);       // clear X marker
			display_draw_line(plot_coords[i].x - 2, plot_coords[i].y + 2, plot_coords[i].x + 2, plot_coords[i].y - 2, COLOR_BLACK);
		}
	}
}

void draw_cursor() {
	uint16_t x = plot_coords[cursor_position].x;                                                                                        // get x position from current cursor index
	uint16_t y = plot_coords[cursor_position].y;                                                                                        // get y position from current cursor index

	display_draw_line(x, PLOT_Y_TOP, x, PLOT_Y_BOTTOM, COLOR_RED);                                                                      // draw vertical red line across plot

	display_draw_line(x - 2, y - 2, x + 2, y + 2, COLOR_RED);                                                                           // draw red X marker
	display_draw_line(x - 2, y + 2, x + 2, y - 2, COLOR_RED);
}

void erase_cursor() {
	display_draw_line(plot_coords[cursor_position].x, PLOT_Y_TOP, plot_coords[cursor_position].x, PLOT_Y_BOTTOM, COLOR_BLACK);          // erase vertical line with black

	// redraw the blue X marker at the cursor point
	display_draw_line(plot_coords[cursor_position].x - 2, plot_coords[cursor_position].y - 2, plot_coords[cursor_position].x + 2, plot_coords[cursor_position].y + 2, COLOR_BLUE);
	display_draw_line(plot_coords[cursor_position].x - 2, plot_coords[cursor_position].y + 2, plot_coords[cursor_position].x + 2, plot_coords[cursor_position].y - 2, COLOR_BLUE);
}


void move_cursor(int direction) {
	if (!is_cursor_on || plot_count == 0)                                                                                               // skip if cursor is inactive or no points to move through
		return;                                                                                                                                  

	erase_cursor();                                                                                                                     // remove current cursor visuals

	restore_point();
	                                                                                                                                    // update cursor position
	cursor_position += direction;                                                                                                       // move cursor left or right
	if (cursor_position < 0) cursor_position = 0;                                                                                       // clamp to first point
	if (cursor_position >= plot_count) cursor_position = plot_count - 1;                                                                // clamp to last point

	uint16_t new_x = plot_coords[cursor_position].x;                                                                                    // get new x for cursor position
	uint16_t new_y = plot_coords[cursor_position].y;                                                                                    // get new y for cursor position
	display_draw_line(new_x, PLOT_Y_TOP, new_x, PLOT_Y_BOTTOM, COLOR_RED);                                                              // draw vertical red line

	display_draw_line(new_x - 2, new_y - 2, new_x + 2, new_y + 2, COLOR_RED);                                                           // draw red X marker
	display_draw_line(new_x - 2, new_y + 2, new_x + 2, new_y - 2, COLOR_RED);
}

float get_cursor_voltage() {
	return plot_coords[cursor_position].value;                                                                                          // return voltage value at current cursor position
}

void draw_indicator_leds(float voltage, bool is_high_voltage) {
#ifdef DEBUG_MODE
#else
	set_text_size(2);                                                                                                                   // set text size for LED indicators
	int led_count;
	if(is_high_voltage){
		led_count = (int)(voltage / 2.4f);                                                                                              // one led every 2.4V
		if (led_count > 10)
			led_count = 10;                                                                                                                     // clamp to max 10 leds
	}else{
		led_count = (int)(voltage / 0.5f);                                                                                              // one led every 2.4V
		if (led_count > 10)
		led_count = 10;
	}
	
	int current_x = 200;                                                                                                                // starting x position for leds
	for (int i = 0; i < 10; i++) {
		if (i < 6)
			display_set_color(COLOR_GREEN);                                                                                             // green for low range
		else if (i < 9)
			display_set_color(COLOR_ORANGE);                                                                                            // orange for mid to high range
		else
			display_set_color(COLOR_RED);                                                                                               // red for end range

		if (i < led_count) {
			display_print("*", current_x + i * 12, 5);                                                                                  // draw top led
			display_print("*", current_x + i * 12, 15);                                                                                 // draw bottom led
			} else {
			display_print(" ", current_x + i * 12, 5);                                                                                  // clear top led
			display_print(" ", current_x + i * 12, 15);                                                                                 // clear bottom led
		}
	}
#endif
}

void erase_voltage_zone() {
#ifdef DEBUG_MODE
#else
	display_fill_rect(VOLTAGE_X_BEGIN, VOLTAGE_Y_BEGIN, VOLTAGE_X_END - VOLTAGE_X_BEGIN, VOLTAGE_Y_END - VOLTAGE_Y_BEGIN, COLOR_BLACK); // clear voltage display area
#endif
}

void draw_power_on_screen(){
	set_text_size(3);
	display_set_color(COLOR_GREEN);
	display_print("Power      ON", 50, 60);
	display_print("Display    ON", 50, 90);
	display_print("System     OFF", 50, 120);
	display_print("Waiting...", 50, 180);
}

void draw_ui(){
#ifdef DEBUG_MODE
	set_text_size(3);
	display_set_color(COLOR_GREEN);
	if(is_plot_on){
		display_print("DEBUG MODE", 70, 5);
		
	}else
		display_print("DEBUG MODE", 70, 100);
#else
	display_draw_line(0, 35, display_width, 35, COLOR_WHITE);                                    // horizontal voltage line
	display_draw_line(184, 35, 184, 0, COLOR_WHITE);                                             // vertical voltage line
	
	display_draw_line(280, 57, 280, 217, COLOR_WHITE);                                           // top horizontal button menu line
	display_draw_line(280, 57, 340, 57, COLOR_WHITE);                                            // bottom horizontal button menu line
	display_draw_line(280, 97, display_width, 97, COLOR_WHITE);                                  // 1st button horizontal line
	display_draw_line(280, 137, display_width, 137, COLOR_WHITE);                                // 2nd button horizontal line
	display_draw_line(280, 177, display_width, 177, COLOR_WHITE);                                // 3rd button horizontal line
	display_draw_line(280, 217, display_width, 217, COLOR_WHITE);                                // 4th button horizontal line
	
	
	set_text_size(1);                                                                            // touchscreen button text (right side of the screen)
	display_set_color(COLOR_GREEN);
	display_print("Cursor", 285, 72);
	display_print("Hold", 290, 112);
	display_print("Wave", 290, 152);
	display_print("Toggle", 283, 192);
#endif
}

void draw_calibration_ui(){
	display_draw_line(75, 155, 245, 155, COLOR_WHITE);
	display_draw_line(75, 155, 75, 185, COLOR_WHITE);
	display_draw_line(245, 155, 245, 185, COLOR_WHITE);
	display_draw_line(75, 185, 245, 185, COLOR_WHITE);
	
	display_set_color(COLOR_GREEN);
	set_text_size(2);
	
	display_print("Please calibrate device!", 20, 20);
	display_print("Turn the wheel to max V", 20, 60);
	display_print("(to the right) and press", 20, 80);
	display_print("the button!", 20, 100);
	
	set_text_size(3);
	display_print("Calibrate", 80, 160);
}

void draw_voltmeter(bool is_cursor_on){
#ifdef DEBUG_MODE
#else	
	set_text_size(2);                                                                            // set text size 2
	display_set_color(COLOR_BLUE);                                                               // set text colour blue
	if(!is_cursor_on)
	display_print("Voltage:", 2, 10);                                                        // top left corner of the screen, Voltmeter text
	else{
		display_print("Voltage:", 2, 0);
		display_print("Cursor:", 2, 15);
	}
#endif
}

void draw_voltage_type(bool is_high_voltage){
#ifdef DEBUG_MODE
#else	
	display_set_color(COLOR_GREEN);
	if(is_high_voltage)
	display_print("H", 188, 10);
	else
	display_print("L", 188, 10);
#endif
}

void draw_cursor_warning(){
#ifdef DEBUG_MODE
#else
	set_text_size(2);
	display_set_color(COLOR_RED);
	display_print("Plot must be enabled", 25, 60);                               // warn the user about the problem
	display_print("for cursor usage!   ", 22, 80);	
#endif
}

void draw_toggle_warning(){
#ifdef DEBUG_MODE
#else
	set_text_size(2);
	display_set_color(COLOR_RED);
	display_print("Plot must be enabled", 25, 60);                               // warn the user about the problem
	display_print("to toggle plot type!", 22, 80);
#endif
}

void print_voltage(bool is_cursor_on, float voltage_value){
	char voltage_value_string[10];
	uint16_t voltage_mV = (uint16_t)(voltage_value * 1000);                                      // convert to mV for float -> int conversion
	if(voltage_value > 10)                                                                       // if voltage < 10 add a space before the value, to clean up previous measurements > 10
		sprintf(voltage_value_string, "%u.%03uV", voltage_mV / 1000, (voltage_mV % 1000));       // convert mV to V in string (sprintf doesn't seem to work with float to string directly...)
	else
		sprintf(voltage_value_string, " %u.%03uV", voltage_mV / 1000, (voltage_mV % 1000));
#ifdef DEBUG_MODE
	char message[64] = "";
	if(is_plot_on){
		strcat(message, "PLOT ");
		if(is_digital_line)
			strcat(message, "DIGITAL ");
		else
			strcat(message, "LINE ");
	}
	if(is_cursor_on)
		strcat(message, "CURSOR ");
	if(is_holding)
		strcat(message, "HOLD ");
	
	strcat(message, "Voltage: ");
	strcat(message, voltage_value_string);
	if(is_cursor_on || !is_plot_on)
		strcat(message, " ");
	else
		strcat(message, "\r\n");
	USART_Send_Msg(message);
#else
	set_text_size(2);
	display_set_color(COLOR_BLUE);
	if(!is_cursor_on)                                                                            // if the cursor was enabled, move the voltage to the top of the voltmeter zone (instead of in the middle of the zone)
		display_print(voltage_value_string, 100, 9);                                             // to make space for the cursor voltage
	else
		display_print(voltage_value_string, 100, 0);
#endif
}

void print_min_max_voltage(bool is_plot_on, float min_value, float max_value){
	char max_value_string[32];
	uint16_t voltage_mV = (uint16_t)(max_value * 1000);                                          // same as above, convert to mV
	sprintf(max_value_string, "Max Voltage: %u.%03uV", voltage_mV / 1000, (voltage_mV % 1000));  // convert mV to V in string
	
	char min_value_string[32];
	voltage_mV = (uint16_t)(min_value * 1000);                                                   // same as above, conver to mV
	sprintf(min_value_string, "Min Voltage: %u.%03uV", voltage_mV / 1000, (voltage_mV % 1000));  // convert mV to V in string

#ifdef DEBUG_MODE
	if(!is_plot_on){
		char min_max_string[64];
		strcpy(min_max_string, min_value_string);
		strcat(min_max_string, " ");
		strcat(min_max_string, max_value_string);
		strcat(min_max_string, "\r\n");
		USART_Send_Msg(min_max_string);
	}else{	
		set_text_size(1);
		display_draw_line (20, 57, 260, 57, COLOR_WHITE);                                        // draw the upper line for the plot
		display_draw_line (20, 217, 260, 217, COLOR_WHITE);                                      // draw the lower line for the plot
		display_print(max_value_string, 90, 47);                                                 // place max_value on top of the top line
		display_print(min_value_string, 90, 222);                                                // place min_valu below the bottom line
	}
#else
	if(is_plot_on){                                                                              // if the plot is enable (waveform viewer)
		set_text_size(1);
		display_draw_line (20, 57, 260, 57, COLOR_WHITE);                                        // draw the upper line for the plot
		display_draw_line (20, 217, 260, 217, COLOR_WHITE);                                      // draw the lower line for the plot
		display_print(max_value_string, 90, 47);                                                 // place max_value on top of the top line
		display_print(min_value_string, 90, 222);                                                // place min_valu below the bottom line
	}else{
		display_print(max_value_string, 25, 120);                                                // otherwise, keep them both in the middle of the waveform viewer area
		display_print(min_value_string, 25, 140);
	}
#endif
}

void print_cursor_voltage(float voltage_value){
	char cursor_voltage_string[10];
	uint16_t voltage_mV = (uint16_t)(voltage_value * 1000);                                          // same as above, convert to mV
	if(voltage_value > 10)
		sprintf(cursor_voltage_string, "%u.%03uV", voltage_mV / 1000, (voltage_mV % 1000));  // convert mV to V in string
	else
		sprintf(cursor_voltage_string, " %u.%03uV", voltage_mV / 1000, (voltage_mV % 1000)); // add a space before the value, if voltage < 10, to clean up previous >10 measurements

#ifdef DEBUG_MODE
	char message[32];
	strcpy(message, "Cursor: ");
	strcat(message, cursor_voltage_string);
	strcat(message, "\r\n");
	USART_Send_Msg(message);
#else
	display_set_color(COLOR_BLUE);
	set_text_size(2);
	display_print(cursor_voltage_string, 100, 15);                                       // print the cursor voltage value (in the Voltmeter area)	
#endif
}

void insert_plot_points(float *plot_points, float point){
	for(int i = 1; i < MAX_POINTS; i++)
	plot_points[i - 1] = plot_points[i];
	plot_points[MAX_POINTS - 1] = point;
}