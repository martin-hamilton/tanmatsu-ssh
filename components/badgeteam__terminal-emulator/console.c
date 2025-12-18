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

/******************************************************************************
 * Includes
 *****************************************************************************/

#include "console.h"
#include "freertos/portable.h"
#include "pax_fonts.h"
#include "pax_gfx.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

/******************************************************************************
 * Preprocessors
 *****************************************************************************/

#define CONS_ESC_TERM_RANGE_START 0x40
#define CONS_ESC_TERM_RANGE_END   0x7E

#define CONS_ESC_PARAM_RANGE_START  0x30
#define CONS_ESC_PARAM_RANGE_END    0x3F

/* Normal colors */

#define CONS_ESC_SGR_FG_START       30
#define CONS_ESC_SGR_FG_END         38

#define CONS_ESC_SGR_BG_START       40
#define CONS_ESC_SGR_BG_END         47

/* Bright colors */

#define CONS_ESC_SGR_FG_B_START       90
#define CONS_ESC_SGR_FG_B_END         97

#define CONS_ESC_SGR_BG_B_START       100
#define CONS_ESC_SGR_BG_B_END         107

/* Special chars */

#define CONS_SPEC_CHARS_END           31

/******************************************************************************
 * Datatypes
 *****************************************************************************/

/******************************************************************************
 * Globals
 *****************************************************************************/

const char CONS_TAG[] = "CONS";
const enum console_fg_color_vga_e g_console_vga_palette[] =
{
  CONS_COL_VGA_BLACK,
  CONS_COL_VGA_RED,
  CONS_COL_VGA_GREEN,
  CONS_COL_VGA_YELLOW,
  CONS_COL_VGA_BLUE,
  CONS_COL_VGA_MAGENTA,
  CONS_COL_VGA_CYAN,
  CONS_COL_VGA_WHITE,
  CONS_COL_VGA_GRAY,

  CONS_COL_VGA_B_RED,
  CONS_COL_VGA_B_GREEN,
  CONS_COL_VGA_B_YELLOW,
  CONS_COL_VGA_B_BLUE,
  CONS_COL_VGA_B_MAGNENTA,
  CONS_COL_VGA_B_CYAN,
  CONS_COL_VGA_B_WHITE,
  CONS_COL_VGA_B_ERR
};

/******************************************************************************
 * Prototypes
 *****************************************************************************/

/******************************************************************************
 * Private Functions
 *****************************************************************************/

void console_output(struct cons_insts_s *inst, char *str, size_t len)
{
  if (inst->output_cb == NULL)
  {
    return;
  }

  inst->output_cb(str, len);
}

/* ANSII escape code parsing *************************************************/

/* The entire parsing happens in single characters. cons_insts_s holds the state.
 * [put] char> [esc_put] char> [escparse_x] params> [escparse_x_y]
 *
 * The following comments assumes the user already knows about ANSII escape codes.
 *
 * Typically, the "escparse_x_y" is called based on what character terminates
 * the escape sequence. For example the H at the end of a CSI sequence would call
 * "escparse_csi_cup", where y in escparse_x_y is cup.
 * Anything that is not a valid terminator is stored as parameters for parsing in
 * "escparse_x_y".
 *
 * CSI means Control Sequence Introducer.
 * This is the x in "escparse_x" and "escparse_x_y".
 * The CSI is decided by the character after the escape character \e. (ex: "\e[...")
 * Characters that come after are routed to "escparse_x".
 */

/* Reset is used to end a sequence */

void console_esc_reset(struct cons_insts_s *inst)
{
  inst->_esc_code_parsing.control_code = 0;
  inst->_esc_code_parsing.esc_mode = false;
  inst->_esc_code_parsing.seq_buf_pos = 0;
  memset(inst->_esc_code_parsing.seq_buf, 0,
         sizeof(inst->_esc_code_parsing.seq_buf));
}

/* All the SGR Select Graphic Rendition parameters and commands */

void console_escparse_csi_sgr(struct cons_insts_s *inst)
{
  /* Extract parameters */

  int code   = 0;
  int param1 = 0;
  int param2 = 0;
  int param3 = 0;
  int param4 = 0;

  int params = sscanf(inst->_esc_code_parsing.seq_buf, "%d;%d;%d;%d;%d",
                      &code, &param1, &param2, &param3, &param4);

  /* We need atleast a code */

  if (params == 0)
  {
    console_esc_reset(inst);
    return;
  }

  /* Get SGR code */

  switch (code)
  {
    case CONS_CSI_SGR_RESET:
    {
      inst->fg = CONSOLE_DEFAULT_FG;
      inst->bg = CONSOLE_DEFAULT_BG;
      break;
    }

    case CONS_CSI_SGR_SET_FG:
    {
      /* RGB params */

      if (param1 == 2)
      {
        uint32_t r = (uint32_t)param4 << 16;
        uint32_t g = (uint32_t)param3 << 8;
        uint32_t b = (uint32_t)param2;

        uint32_t rgb = r | g | b;
        uint32_t rgba = rgb | CONSOLE_DEFAULT_ALPHA;

        inst->fg = rgba;
      }

      break;
    }

    case CONS_CSI_SGR_SET_BG:
    {
      /* RGB params */

      if (param1 == 2)
      {
        uint32_t r = (uint32_t)param4 << 16;
        uint32_t g = (uint32_t)param3 << 8;
        uint32_t b = (uint32_t)param2;

        uint32_t rgb = r | g | b;
        uint32_t rgba = rgb | CONSOLE_DEFAULT_ALPHA;

        inst->bg = rgba;
      }

      break;      
    }
    
    /* Other codes. Such as ranges or unknown.
     * NOTE: it is probably not a bad idea to turn some of these in functions.
     * Might make this function less big.
     */

    default:
    {
      /* Set FG */

      if (code >= CONS_ESC_SGR_FG_START && code <= CONS_ESC_SGR_FG_END)
      {
        code = code - CONS_ESC_SGR_FG_START;
        /* Colors cant exceed palette. Limit to last */

        if (code >= (sizeof(g_console_vga_palette) / sizeof(g_console_vga_palette[0])) )
        {
          code = (sizeof(g_console_vga_palette) / sizeof(g_console_vga_palette[0])) - 1;
        }

        /* Set FG */

        uint32_t fg = g_console_vga_palette[code];
        inst->fg = fg | CONSOLE_DEFAULT_ALPHA;
        break;
      }

      /* Set BG */

      if (code >= CONS_ESC_SGR_BG_START && code <= CONS_ESC_SGR_BG_END)
      {
        code = code - CONS_ESC_SGR_BG_START;

        /* Colors cant exceed palette. Limit to last */

        if (code >= (sizeof(g_console_vga_palette) / sizeof(g_console_vga_palette[0])) )
        {
          code = (sizeof(g_console_vga_palette) / sizeof(g_console_vga_palette[0])) - 1;
        }

        /* Set BG */

        uint32_t bg = g_console_vga_palette[code];
        inst->bg = bg | CONSOLE_DEFAULT_ALPHA;
        break;
      }

      /* Set Bright FG */

      if (code >= CONS_ESC_SGR_FG_B_START && code <= CONS_ESC_SGR_FG_B_END)
      {
        code = code - CONS_ESC_SGR_FG_B_START;
        code += 8;

        /* Colors cant exceed palette. Limit to last */

        if (code >= (sizeof(g_console_vga_palette) / sizeof(g_console_vga_palette[0])) )
        {
          code = (sizeof(g_console_vga_palette) / sizeof(g_console_vga_palette[0])) - 1;
        }

        /* Set FG */

        uint32_t fg = g_console_vga_palette[code];
        inst->fg = fg | CONSOLE_DEFAULT_ALPHA;
        break;
      }

      /* Set Bright BG */

      if (code >= CONS_ESC_SGR_BG_B_START && code <= CONS_ESC_SGR_BG_B_END)
      {
        code = code - CONS_ESC_SGR_BG_B_START;
        code += 8;

        /* Colors cant exceed palette. Limit to last */

        if (code >= (sizeof(g_console_vga_palette) / sizeof(g_console_vga_palette[0])) )
        {
          code = (sizeof(g_console_vga_palette) / sizeof(g_console_vga_palette[0])) - 1;
        }

        /* Set BG */

        uint32_t bg = g_console_vga_palette[code];
        inst->bg = bg | CONSOLE_DEFAULT_ALPHA;
        break;
      }

      /* Unknown */

      break;
    }
  }

  /* End sequence */

  console_esc_reset(inst);
}

/* Cursor Position command parsing */

void console_escparse_csi_cup(struct cons_insts_s *inst)
{
  int row;
  int col;

  /* Extract parameters n;m or only n */

  int params = sscanf(inst->_esc_code_parsing.seq_buf,
                      "%d;%d", &col, &row);

  /* Change cursor position */

  if (params == 2)
  {
    console_set_cursor(inst, row-1, col-1);
  }

  /* End sequence */

  console_esc_reset(inst);

  return;
}

void console_escparse_csi_cha(struct cons_insts_s *inst)
{
  int col;

  /* Extract parameters n;m or only n */

  int params = sscanf(inst->_esc_code_parsing.seq_buf, "%d", &col);

  /* Change cursor position */

  if (params == 1)
  {
    console_set_cursor(inst, inst->cursor_x, col-1);
  }else{
    console_set_cursor(inst, inst->cursor_x, 0);
  }

  /* End sequence */

  console_esc_reset(inst);

  return;
}

/* Erase in Display */

void console_escparse_csi_ed(struct cons_insts_s *inst)
{
  int param;

  /* Extract parameters */

  int params = sscanf(inst->_esc_code_parsing.seq_buf, "%d", &param);

  /* Change cursor position */

  if (params == 1)
  {
    if (param == 0)
    {
      /* Clear from cursor to end of screen */

      console_clear_at(inst, inst->cursor_x, inst->cursor_y,
                       inst->chars_x, inst->chars_y);      
    }
    else if (param == 1)
    {
      /* Clear from cursor to beginning of screen */

      console_clear_at(inst, inst->chars_x, inst->chars_y,
                       inst->cursor_x, inst->cursor_y );            
    }
    else if (param == 2)
    {
      /* Clear entire screen and moves cursor to upper left */

      console_clear(inst);
      console_set_cursor(inst, 0, 0);
    }
    else if (param == 3)
    {
      /* clear entire screen and delete all lines saved in the scrollback buffer
       * (this feature was added for xterm and is supported by other terminal applications).
       */

      /* No support yet for scrollback */
    }
  }
  else /* Param is missing */
  {
    /* Clear from cursor to end of screen */

    console_clear_at(inst, inst->cursor_x, inst->cursor_y,
                     inst->chars_x, inst->chars_y);
  }

  /* End sequence */

  console_esc_reset(inst);

  return;
}

/* Erase in Line */

void console_escparse_csi_el(struct cons_insts_s *inst)
{
  int param;

  /* Extract parameters */

  int params = sscanf(inst->_esc_code_parsing.seq_buf, "%d", &param);

  /* Change cursor position */

  if (params == 1)
  {
    if (param == 0)
    {
      /* Clear from cursor to end of line */

      console_clear_at(inst, inst->cursor_x, inst->cursor_y,
                       inst->chars_x, inst->cursor_y);  
    }
    else if (param == 1)
    {
      /* Clear from cursor to beginning of line */

      console_clear_at(inst, 0, inst->cursor_y,
                       inst->cursor_x, inst->cursor_y );            
    }
    else if (param == 2)
    {
      /* Clear entire line */

      console_clear_at(inst, 0, inst->cursor_y,
                       inst->chars_x, inst->cursor_y);
    }
  }
  else /* Param is missing */
  {
    /* Clear from cursor to end of line */

    console_clear_at(inst, inst->cursor_x, inst->cursor_y,
                     inst->chars_x, inst->cursor_y);
  }

  /* End sequence */

  console_esc_reset(inst);

  return;
}

/* Device Status Return */

void console_escparse_csi_dsr(struct cons_insts_s *inst)
{
  /* Return cursor position */

  char str[32];
  snprintf(str, sizeof(str), "\e[%d;%dR", inst->cursor_y, inst->cursor_x);
  console_output(inst, str, strlen(str));

  /* End of sequence */

  console_esc_reset(inst);

  return;
}

/* Basic cursor movements */

void console_escparse_csi_cuu(struct cons_insts_s *inst)
{
  int param;
  int x;
  int y;

  /* Extract parameters */

  int params = sscanf(inst->_esc_code_parsing.seq_buf, "%d", &param);
  param = param - 1;
  if (params > 0)
  {
    console_get_cursor(inst, &x, &y);
    console_set_cursor(inst, x, y - param);
  }
  else
  {
    console_get_cursor(inst, &x, &y);
    console_set_cursor(inst, x, y - 1);   
  }

  /* End of sequence */

  console_esc_reset(inst);

  return;   
}

void console_escparse_csi_cud(struct cons_insts_s *inst)
{
  int param;
  int x;
  int y;

  /* Extract parameters */

  int params = sscanf(inst->_esc_code_parsing.seq_buf, "%d", &param);
  param = param - 1;
  if (params > 0)
  {
    console_get_cursor(inst, &x, &y);
    console_set_cursor(inst, x , y + param);
  }
  else
  {
    console_get_cursor(inst, &x, &y);
    console_set_cursor(inst, x, y + 1);   
  }

  /* End of sequence */

  console_esc_reset(inst);

  return;   
}

void console_escparse_csi_cuf(struct cons_insts_s *inst)
{
  int param;
  int x;
  int y;

  /* Extract parameters */

  int params = sscanf(inst->_esc_code_parsing.seq_buf, "%d", &param);
  param = param - 1;
  if (params > 0)
  {
    console_get_cursor(inst, &x, &y);
    console_set_cursor(inst, x + param, y);
  }
  else
  {
    console_get_cursor(inst, &x, &y);
    console_set_cursor(inst, x + 1, y);   
  }

  /* End of sequence */

  console_esc_reset(inst);

  return;   
}

void console_escparse_csi_cub(struct cons_insts_s *inst)
{
  int param;
  int x;
  int y;

  /* Extract parameters */

  int params = sscanf(inst->_esc_code_parsing.seq_buf, "%d", &param);
  param = param - 1;
  if (params > 0)
  {
    console_get_cursor(inst, &x, &y);
    console_set_cursor(inst, x - param, y);
  }
  else
  {
    console_get_cursor(inst, &x, &y);
    console_set_cursor(inst, x - 1, y);   
  }

  /* End of sequence */

  console_esc_reset(inst);

  return;   
}

/* Cursor Next Line */

void console_escparse_csi_cnl(struct cons_insts_s *inst)
{
  int param;
  int x;
  int y;

  /* Extract parameters */

  int params = sscanf(inst->_esc_code_parsing.seq_buf, "%d", &param);
  param = param - 1;
  if (params > 0)
  {
    console_get_cursor(inst, &x, &y);
    console_set_cursor(inst, 0, y + param);
  }
  else
  {
    console_get_cursor(inst, &x, &y);
    console_set_cursor(inst, 0, y + 1);   
  }

  /* End of sequence */

  console_esc_reset(inst);

  return;   
}

/* Cursor Previous Line */

void console_escparse_csi_cpl(struct cons_insts_s *inst)
{
  int param;
  int x;
  int y;

  /* Extract parameters */

  int params = sscanf(inst->_esc_code_parsing.seq_buf, "%d", &param);
  param = param - 1;
  if (params > 0)
  {
    console_get_cursor(inst, &x, &y);
    console_set_cursor(inst, 0, y - param);
  }
  else
  {
    console_get_cursor(inst, &x, &y);
    console_set_cursor(inst, 0, y - 1);   
  }

  /* End of sequence */

  console_esc_reset(inst);

  return;    
}

/* Paste Bracket mode enable */

void console_escparse_csi_pbe(struct cons_insts_s *inst)
{
  int param;

  int params = sscanf(inst->_esc_code_parsing.seq_buf, "%d", &param);
  param = param - 1;
  fprintf(stderr, "console_escparse_csi_pbe() - %d params\n", params);

  /* End of sequence */

  console_esc_reset(inst);

  return;   
}

/* Paste Bracket mode disable */

void console_escparse_csi_pbd(struct cons_insts_s *inst)
{
  int param;

  int params = sscanf(inst->_esc_code_parsing.seq_buf, "%d", &param);
  param = param - 1;
  fprintf(stderr, "console_escparse_csi_pbd() - %d params\n", params);

  /* End of sequence */

  console_esc_reset(inst);

  return;   
}

/* Parse CSI */

void console_escparse_csi(struct cons_insts_s *inst, char c)
{
  size_t index = inst->_esc_code_parsing.seq_buf_pos;
  ESP_LOGI(CONS_TAG, "console_escparse_csi");
  ESP_LOGI(CONS_TAG, "  index: %d", (int)index);

  // XXX right here - skip if pattern matches CSI ? \d+ l or h.
  //if (inst->_esc_code_parsing.seq_buf[index] == '?') {
  //  ESP_LOGI(CONS_TAG, "  found private sequence, skipping");
  //  // XXX some private escape sequences are a different number of characters
  //  inst->_esc_code_parsing.seq_buf_pos += 6;
  //  return;
  //}

  /* Check for CSI terminators */

  if (c >= CONS_ESC_TERM_RANGE_START && c <= CONS_ESC_TERM_RANGE_END)
  {
    switch (c)
    {
      case CONS_CSI_CUP_TERM:
      {
        console_escparse_csi_cup(inst);
        return;
       }

      case CONS_CSI_ED_TERM:
      {
        console_escparse_csi_ed(inst);
        return;
      }

      case CONS_CSI_EL_TERM:
      {
        console_escparse_csi_el(inst);
        return;
      }

      case CONS_CSI_DSR_TERM:
      {
        console_escparse_csi_dsr(inst);
        return;
      }

      case CONS_CSI_HVP_TERM:
      {
        console_escparse_csi_cup(inst);
        return;
      }

      case CONS_CSI_CHA_TERM:
      {
        console_escparse_csi_cha(inst);
        return;
      }

      case CONS_CSI_CUU_TERM:
      {
        console_escparse_csi_cuu(inst);
        return;
      }

      case CONS_CSI_CUD_TERM:
      {
        console_escparse_csi_cud(inst);
        return;
      }

      case CONS_CSI_CUF_TERM:
      {
        console_escparse_csi_cuf(inst);
        return;
      }

      case CONS_CSI_CUB_TERM:
      {
        console_escparse_csi_cub(inst);
        return;
      }

      case CONS_CSI_CNL_TERM:
      {
        console_escparse_csi_cnl(inst);
        return;
      }

      case CONS_CSI_CPL_TERM:
      {
        console_escparse_csi_cpl(inst);
        return;
      }

      case CONS_CSI_SGR_TERM:
      {
        console_escparse_csi_sgr(inst);
        return;
      }

      case CONS_CSI_PBE_TERM:
      {
        console_escparse_csi_pbe(inst);
        return;
      }

      case CONS_CSI_PBD_TERM:
      {
        console_escparse_csi_pbd(inst);
        return;
      }

      /* This is not a known or illegal terminator */
  
      default:
      {
        ESP_LOGE(CONS_TAG, "Illegal escape terminator 0x%X", c);

        /* End sequence */

        console_esc_reset(inst);
        return;
      }
    }
  }

  /* Check whether this is a legal parameter byte */

  if (c < CONS_ESC_PARAM_RANGE_START || c > CONS_ESC_PARAM_RANGE_END)
  {
    /* Illegal parameter byte */

    /* End sequence */

    console_esc_reset(inst);
    ESP_LOGE(CONS_TAG, "Illegal escape parameter 0x%X", c);

    return;
  }

  /* Store character as param in sequence buffer */

  index = inst->_esc_code_parsing.seq_buf_pos++;
  ESP_LOGI(CONS_TAG, "  seq_buf index %d", index);
  if (index >= sizeof(inst->_esc_code_parsing.seq_buf))
  {
    /* In case of overflow, reset parser */

    /* End sequence */

    console_esc_reset(inst);
    return;
  }

  inst->_esc_code_parsing.seq_buf[index] = c;
  ESP_LOGI(CONS_TAG, "  seq_buf[%d] = %c", index, c);
}

/* ASII escape code entry ****************************************************/

void console_esc_put(struct cons_insts_s *inst, char c)
{
  /* Get control code if we dont have one and return */

  if (inst->_esc_code_parsing.control_code == 0)
  {
    inst->_esc_code_parsing.control_code = c;
    return;
  }

  /* Give params to control code */

  switch (inst->_esc_code_parsing.control_code)
  {
    case CONS_CTRL_CSI:
    {
      console_escparse_csi(inst, c);
      return;
    }

    /* Unknown control code */

    default:
    {
      ESP_LOGE(CONS_TAG, "Illegal escape code 0x%X", c);
      console_esc_reset(inst);
      return;
    }
  }
}

/* Drawing ******************************************************************/

void console_clear_at(struct cons_insts_s *inst, size_t x1, size_t y1,
                      size_t x2, size_t y2)
{
  x1 = x1 * inst->char_width;
  x2 = (x2+1) * inst->char_width; /* Add one cell width */

  y1 = y1 * inst->char_height;
  y2 = (y2+1) * inst->char_height; /* Add one cell thickness */

  pax_simple_rect(inst->paxbuf, inst->bg, x1, y1, x2-x1, y2-y1);
}

void console_clear(struct cons_insts_s *inst)
{
  console_clear_at(inst, 0, 0, inst->chars_x-1, inst->chars_y-1);
}

void console_draw_char(struct cons_insts_s *inst, size_t x, size_t y, struct cons_char_s *c)
{
  size_t screen_x = x * inst->char_width;
  size_t screen_y = y * inst->char_height;

  /* Draw background character block */

  pax_simple_rect(inst->paxbuf, c->bg, screen_x, screen_y,
                  inst->char_width, inst->char_height);


  /* Limit the character range */

  if (c->character < ' ' || c->character > '~')
  {
    c->character = '.';
  }

  /* Draw character */

  char single_char[] = {0x00, 0x00};
  single_char[0] = c->character;

  pax_draw_text(inst->paxbuf, c->fg, inst->font,
                inst->font_size, screen_x, screen_y,
                single_char);
}

/* Special chars */

/* Returns non-zero when the char can be printed */

int console_handle_special_char(struct cons_insts_s *inst, char *c)
{
  int x;
  int y;

  switch ((*c))
  {
    case '\n':
    {
      console_newline(inst);
      console_get_cursor(inst, &x, &y);
      console_set_cursor(inst, 0, y);
      return 0;
    }

    case '\b':
    {
      console_get_cursor(inst, &x, &y);
      console_set_cursor(inst, x - 1, y);
      return 0;
    }

    case '\t':
    {
      console_get_cursor(inst, &x, &y);
      x = x + (1 + x % CONSOLE_TABS);
      console_set_cursor(inst, x - 1, y);
      return 0;
    }

    default:
    {
      return 0;
    }
  }

  return 0;
}

/******************************************************************************
 * Public Functions
 *****************************************************************************/

/* Printing and putting ******************************************************/

void console_printf(struct cons_insts_s *inst, char *format, ...)
{
  /* Extract varargs into buffer */

  va_list args;
  va_start(args, format);
  char buffer[CONSOLE_PRINTF_MAX_LEN];
  memset(buffer, 0, sizeof(buffer));
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);

  console_puts(inst, buffer);
}

void console_puts(struct cons_insts_s *inst, char *str)
{
  size_t len = strlen(str);
  for (size_t i = 0; i < len; i++)
  {
    console_put(inst, str[i]);
  }
}

void console_put(struct cons_insts_s *inst, char c)
{

#if CONSOLE_ANSII_ESCAPE_CODES == 1
  /* ANSII escape mode */

  if (inst->_esc_code_parsing.esc_mode)
  {
    console_esc_put(inst, c);
    return;
  }

  /* Escape keys activates the special ANSII escape code parser */

  if (c == '\e')
  {
    inst->_esc_code_parsing.esc_mode = true;
    return;
  }
#endif

  /* Newlines arent printables, so do a shift */

  if (c <= CONS_SPEC_CHARS_END)
  {
    int ret = console_handle_special_char(inst, &c);
    if (ret == 0)
    {
      return;
    }
  }

  /* Put char at new location */

  console_put_at(inst, inst->cursor_x, inst->cursor_y, c);

  /* Increment next X position */

  inst->cursor_x++;

  /* Reset X at X boundry and shift */

  if (inst->cursor_x >= inst->chars_x)
  {
    inst->cursor_x = 0;
    console_newline(inst);
  }
}

void console_set_colors(struct cons_insts_s *inst, uint32_t fg, uint32_t bg)
{
  inst->fg = fg;
  inst->bg = bg;
}

void console_puts_at(struct cons_insts_s *inst, size_t x, size_t y, char *str)
{
  size_t len = strlen(str);
  for (size_t i = 0; i < len; i++)
  {
    console_put_at(inst, x+i, y, str[i]);
  }
}

void console_newline(struct cons_insts_s *inst)
{
  /* TODO: Shift the character buffer as well */
  inst->cursor_y++;
  if (inst->cursor_y >= inst->chars_y)
  {
    inst->cursor_y = inst->chars_y-1;
    pax_buf_scroll(inst->paxbuf, 0xFF000000, 0, -inst->char_height);
  }
}

void console_put_at(struct cons_insts_s *inst, size_t x, size_t y, char c)
{
  /* Limit the characters to boundries */

  if (x >= inst->chars_x || y >= inst->chars_y)
  {
    return;
  }

  /* Store character */

  struct cons_char_s charstruct =
  {
    .character = c,
    .bg = inst->bg,
    .fg = inst->fg
  };

#if 0 /* We dont use the console buffer right now */
  size_t addr = x + (y * inst->chars_y);
  inst->char_alloc[addr] = charstruct;
#endif

  /* Draw character */

  console_draw_char(inst, x, y, &charstruct);
}

void console_get_size(struct cons_insts_s *inst, size_t *x, size_t *y)
{
  (*x) = inst->chars_x;
  (*y) = inst->chars_y;
}

void console_get_cursor(struct cons_insts_s *inst, int *x, int *y)
{
  (*x) = inst->cursor_x;
  (*y) = inst->cursor_y;
}

void console_set_cursor(struct cons_insts_s *inst, int x, int y)
{
  /* Limit cursor to edges */

  if (x < 0)
  {
    x = 0;
  }

  if (y < 0)
  {
    y = 0;
  }

  if (x >= inst->chars_x)
  {
    x = inst->chars_x - 1;
  }

  if (y >= inst->chars_y)
  {
    y = inst->chars_y - 1;
  }

  /* Apply */

  inst->cursor_x = x;
  inst->cursor_y = y;
}

int console_init(struct cons_insts_s *instance, const struct cons_config_s *config)
{
  /* Verify configuration */

  if (config->paxbuf == NULL)
  {
    ESP_LOGE(CONS_TAG, "No pax buffer");
    return -1;
  }

  instance->paxbuf = config->paxbuf;
  instance->font = config->font; // Validate this
  instance->font_size_mult = config->font_size_mult;
  instance->output_cb = config->output_cb;

  /* Character dimensions */

  instance->font_size =   instance->font->default_size
                          * instance->font_size_mult;
  instance->char_width =  instance->font->ranges->bitmap_mono.width
                          * instance->font_size_mult;
  instance->char_height = instance->font->ranges->bitmap_mono.height
                          * instance->font_size_mult;

  /* Calculate how many chars can fit in the buffer
   * NOTE: Orientation is forced with direct assignment of height/width.
   * Depending on actual orientation, we might need to swap
   * the height and width here.
   */

  instance->chars_x = instance->paxbuf->height / instance->char_width;
  instance->chars_y = instance->paxbuf->width / instance->char_height;

  ESP_LOGI(CONS_TAG, "Console size X %zu, Y %zu", instance->paxbuf->width, instance->paxbuf->height);
  ESP_LOGI(CONS_TAG, "Console chars X %zu, Y %zu", instance->chars_x, instance->chars_y);

  /* Allocate as many characters we can to fit in the buffer */

#if 0 /* We dont use the console buffer right now */
  size_t alloc = instance->chars_x * instance->chars_y * sizeof(struct cons_char_s);
  ESP_LOGI(CONS_TAG, "Allocating %zu bytes", alloc);
  instance->char_alloc = (struct cons_char_s *)pvPortMalloc(alloc);
  if (instance->char_alloc == NULL)
  {
    ESP_LOGE(CONS_TAG, "Allocation error");
    return -1;
  }
#endif

  /* Defaults */

  instance->fg = 0xFFFFFFFF;
  instance->bg = 0xFF000000;

  pax_background(instance->paxbuf, instance->bg);


  return 0;
}
