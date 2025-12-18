/******************************************************************************
 * MIT License
 * 
 * Copyright (c) 2025 Kevin Witteveen (MartiniMarter)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *****************************************************************************/

#ifndef _CONSOLE_H
#define _CONSOLE_H

/******************************************************************************
 * Includes
 *****************************************************************************/

#include <stdio.h>
#include "pax_fonts.h"
#include "pax_gfx.h"
#include "pax_text.h"
#include "freertos/idf_additions.h"
#include "esp_log.h"

/******************************************************************************
 * Preprocessors
 *****************************************************************************/

/* Enable ANSII escape codes */

#define CONSOLE_ANSII_ESCAPE_CODES  1

/* Default color settings */

#define CONSOLE_DEFAULT_FG          0xFFFFFFFF
#define CONSOLE_DEFAULT_BG          0xFF000000
#define CONSOLE_DEFAULT_ALPHA       0xFF000000

/* Enable character structure packing. This uses less memory,
 * but might give worse performance
 */

#define CONSOLE_PACK_CHARACTERS 1

/* Buffer settings */

#define CONSOLE_PRINTF_MAX_LEN      256
#define CONSOLE_ASCII_ESC_SEQ_LEN   32

/* The tab size in characters */

#define CONSOLE_TABS                4

/******************************************************************************
 * Datatypes
 *****************************************************************************/

/* ANSII colors */

enum console_fg_color_vga_e
{
  CONS_COL_VGA_BLACK     = 0x00000000,
  CONS_COL_VGA_RED       = 0x00AA0000,
  CONS_COL_VGA_GREEN     = 0x0000AA00,
  CONS_COL_VGA_YELLOW    = 0x00AA5000,
  CONS_COL_VGA_BLUE      = 0x000000AA,
  CONS_COL_VGA_MAGENTA   = 0x00AA00AA,
  CONS_COL_VGA_CYAN      = 0x0000AAAA,
  CONS_COL_VGA_WHITE     = 0x00AAAAAA,
  CONS_COL_VGA_GRAY      = 0x00505050,

  CONS_COL_VGA_B_RED     = 0x00FF5050,
  CONS_COL_VGA_B_GREEN   = 0x0050FF50,
  CONS_COL_VGA_B_YELLOW  = 0x00FFFF50,
  CONS_COL_VGA_B_BLUE    = 0x005050FF,
  CONS_COL_VGA_B_MAGNENTA = 0x00FF50FF,
  CONS_COL_VGA_B_CYAN    = 0x0050FFFF,
  CONS_COL_VGA_B_WHITE   = 0x00FFFFFF,
  CONS_COL_VGA_B_ERR     = 0x00FF0000
};

/* ANSII escape code characters */

enum console_control_codes_e
{
  CONS_CTRL_CSI = '['
};

/* All the supported CSI commands */

enum console_csi_terminators_e
{
  CONS_CSI_CUP_TERM = 'H', /* Cursor Position n;m */
  CONS_CSI_SGR_TERM = 'm', /* Select Graphic Rendition */
  CONS_CSI_ED_TERM  = 'J', /* Erase in Display n */
  CONS_CSI_EL_TERM  = 'K', /* Erase in Line n */
  CONS_CSI_DSR_TERM = 'n', /* Device Status Raport */
  CONS_CSI_HVP_TERM = 'f', /* Horizontal Vertical Pos */
  CONS_CSI_CHA_TERM = 'G', /* Cursor Horizontal Absolute */
  CONS_CSI_CUU_TERM = 'A', /* Cursor Up */
  CONS_CSI_CUD_TERM = 'B', /* Cursor Down */
  CONS_CSI_CUF_TERM = 'C', /* Cursor Forward */
  CONS_CSI_CUB_TERM = 'D', /* Cursor Down */
  CONS_CSI_CNL_TERM = 'E', /* Cursor Next Line */
  CONS_CSI_CPL_TERM = 'F', /* Cursor Previous Line */
  CONS_CSI_PBE_TERM = 'h', /* Enable Paste Bracket mode */
  CONS_CSI_PBD_TERM = 'l', /* Disable Paste Bracket mode */
};

/* All the supported SGR codes */

enum console_csi_sgr_codes_e
{
  CONS_CSI_SGR_RESET         = 0,
  CONS_CSI_SGR_SET_FG        = 38,
  CONS_CSI_SGR_SET_BG        = 48,

  /* Includes FG/BG color palette codes */
};

/* Contains initialization info */

struct cons_config_s
{
  /* The font size multiplier will also influence the row/column allocation.
   * A small font will result more characters to fit in the
   * *paxbuf, therefore more space will be allocated for it.
   */

  float font_size_mult;

  /* This is where pixel data is stored */

  pax_buf_t *paxbuf;

  const struct pax_font *font;

  /* Console output. Sends a string back. Used by Device Status Raport */

  void (*output_cb)(char *str, size_t len);
};

#if CONSOLE_PACK_CHARACTERS == 1
#pragma pack(1)
#endif
struct cons_char_s
{
  char character;
  pax_col_t fg;
  pax_col_t bg;
};

/* Contains internal console instance data */

struct cons_insts_s
{
  /* Pax buffer. Can be used to draw the console */

  pax_buf_t *paxbuf;

  /* Character info */

  float font_size_mult; /* Font size multiplier */
  size_t char_width; /* Size width*/
  size_t char_height; /* Size height */
  size_t chars_y; /* N chars y */
  size_t chars_x; /* N chars x */
  float font_size; /* Pax font size */

  /* Character allocation */

  struct cons_char_s *char_alloc;
  
  /* Console instance info */

  const struct pax_font *font;
  size_t cursor_x;
  size_t cursor_y;
  pax_col_t fg;
  pax_col_t bg;

  /* ANSII escape code parsing */

  struct 
  {
    bool esc_mode;
    enum console_control_codes_e control_code;
    char seq_buf[CONSOLE_ASCII_ESC_SEQ_LEN];
    size_t seq_buf_pos;

  } _esc_code_parsing;

  /* Console output for responses, such as DSR response */

  void (*output_cb)(char *str, size_t len);
};

/******************************************************************************
 * Public Functions
 *****************************************************************************/

/* Behaves like printf with full formatting and
 * ANSII escape codes if enabled
 */
 
void console_printf(struct cons_insts_s *inst, char *format, ...);

/* Put a single character or puts a string.
 * Both supports ANSII escape code sequences
 * if enabled.
 */

void console_puts(struct cons_insts_s *inst, char *str);
void console_put(struct cons_insts_s *inst, char c);
void console_puts_at(struct cons_insts_s *inst, size_t x, size_t y, char *str);
void console_put_at(struct cons_insts_s *inst, size_t x, size_t y, char c);

/* Advances Y position and shifts console when needed.
 * Does not reset X position.
 * Printing a \n will do both.
 */

void console_newline(struct cons_insts_s *inst);

/* Selects a color to be used with new characters */

void console_set_colors(struct cons_insts_s *inst, pax_col_t fg, pax_col_t bg);

/* Clear a part or entire screen with a BG color */

void console_clear_at(struct cons_insts_s *inst, size_t x1, size_t y1, size_t x2, size_t y2);
void console_clear(struct cons_insts_s *inst); 

/* Returns the console size in characters */

void console_get_size(struct cons_insts_s *inst, size_t *x, size_t *y);

/* Absolute cursor positioning */

void console_get_cursor(struct cons_insts_s *inst, int *x, int *y); 
void console_set_cursor(struct cons_insts_s *inst, int x, int y);

/* Takes a console config and initializes an instance */

int console_init(struct cons_insts_s *instance, const struct cons_config_s *config);

#endif /* _CONSOLE_H */
