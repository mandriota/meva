/******************************************************************************\
*                                                                              *
*    Mewa. Math EWAluator.                                                     *
*    Copyright (C) 2024 Mark Mandriota                                         *
*                                                                              *
*    This program is free software: you can redistribute it and/or modify      *
*    it under the terms of the GNU General Public License as published by      *
*    the Free Software Foundation, either version 3 of the License, or         *
*    (at your option) any later version.                                       *
*                                                                              *
*    This program is distributed in the hope that it will be useful,           *
*    but WITHOUT ANY WARRANTY; without even the implied warranty of            *
*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the             *
*    GNU General Public License for more details.                              *
*                                                                              *
*    You should have received a copy of the GNU General Public License         *
*    along with this program.  If not, see <https://www.gnu.org/licenses/>.    *
*                                                                              *
\******************************************************************************/

//=:config:debug
// comment to disable debug mode
// #define NDEBUG

//=:config:colors
// uncomment to disable colors
// #define NCOLORS

#define ESC "\x1b"

#define CLR_BRED ESC "[1;31m"
#define CLR_BGRN ESC "[1;32m"
#define CLR_BYEL ESC "[1;33m"
#define CLR_BBLU ESC "[1;34m"
#define CLR_BMAG ESC "[1;35m"
#define CLR_BCYN ESC "[1;36m"

#ifndef NCOLORS
#define CLR_RESET ESC "[39;49m"

// error messages color
#define CLR_ERR_MSG CLR_BRED

// warning messages color
#define CLR_WRN_MSG CLR_BYEL

// info messages color
#define CLR_INF_MSG CLR_BBLU

// primitive values color
#define CLR_PRIM CLR_BCYN

// internal symbols color
#define CLR_INTERNAL CLR_BMAG
#else
#define CLR_RESET
#define CLR_ERR_MSG
#define CLR_WRN_MSG
#define CLR_INF_MSG
#define CLR_PRIM
#define CLR_INTERNAL
#endif

//=:config:output_format
// source tree indentation
#define SOURCE_INDENTATION (0)

// source tree max depth
#define SOURCE_MAX_DEPTH (50)

// result tree indentation
#define RESULT_INDENTATION (1)

// result tree max depth
#define RESULT_MAX_DEPTH (50)

//=:config:pipe
#define PIPE_RESULT_PREFIX "= "

#define PIPE_RESULT_SUFFIX " "

//=:config:repl
#define REPL_PROMPT "? "

#define REPL_MULTILINE_PROMPT "... "

#define REPL_RESULT_PREFIX "\n" PIPE_RESULT_PREFIX

#define REPL_RESULT_SUFFIX "\n"

//=:config:math
#define MAX_DIFF_ULPS (4096)

#define MAX_DIFF_ULPS_FROM (1)

#define MAX_DIFF_ABS (0.00000000001)

//=:config:internal
// must be at least 1
#define INTERNAL_READING_BUF_SIZE (512)

#define NODE_BUF_SIZE (1 << 20)

#define GLOBAL_SCOPE_CAPACITY (256)
