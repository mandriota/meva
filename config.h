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

//=:config
//                      __ _
//                     / _(_)
//      ___ ___  _ __ | |_ _  __ _
//     / __/ _ \| '_ \|  _| |/ _` |
//    | (_| (_) | | | | | | | (_| |
//     \___\___/|_| |_|_| |_|\__, |
//                            __/ |
//                           |___/

//=:config:debug
// comment to disable debug mode
#define NDEBUG

//=:config:colors
// uncomment to disable colors
#define NCOLORS

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

// info messages color
#define CLR_INF_MSG CLR_BBLU

// primitive values color
#define CLR_PRIM CLR_BCYN

// internal symbols color
#define CLR_INTERNAL CLR_BMAG
#else
#define CLR_RESET ESC "[39;49m"
#define CLR_ERR_MSG CLR_BRED
#define CLR_INF_MSG CLR_BBLU
#define CLR_PRIM CLR_BCYN
#define CLR_INTERNAL CLR_BMAG
#endif

//=:config:output_format
// source tree indentation
#define SOURCE_INDENTATION 0

// source tree max depth
#define SOURCE_MAX_DEPTH 100

// result tree indentation
#define RESULT_INDENTATION 1

// result tree max depth
#define RESULT_MAX_DEPTH 100

//=:config:pipe
#define PIPE_RESULT_PREFIX "= "

#define PIPE_RESULT_SUFFIX ""

//=:config:repl
#define REPL_PROMPT "? "

#define REPL_MULTILINE_PROMPT "... "

#define REPL_RESULT_PREFIX "\n" PIPE_RESULT_PREFIX

#define REPL_RESULT_SUFFIX "\n"

//=:config:internal
// must be at least 1
#define INTERNAL_READING_BUF_SIZE 1


