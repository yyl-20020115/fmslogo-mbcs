// -*- c++ -*-
// Copyright (C) 1995 by George Mills
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

#ifndef __PRINT_H_
#define __PRINT_H_

#include <stdio.h>

struct NODE;

enum MESSAGETYPE
{
    MESSAGETYPE_Normal, // normal text
    MESSAGETYPE_Trace,  // a TRACE message
    MESSAGETYPE_Error,  // an error message
};

// function declarations
extern void print_char(FILE *strm, MESSAGETYPE type, char ch);
extern void print_space(FILE *strm, MESSAGETYPE type);
extern void ndprintf(FILE *strm, MESSAGETYPE type, const char *fmt, ...);
extern int find_limit(const class CLocalizedNode & Node);
extern void print_helper(FILE *strm, MESSAGETYPE type, NODE *nd);
extern void print_node(FILE *strm, MESSAGETYPE type, NODE *nd);
extern void print_nobrak(FILE *strm, MESSAGETYPE type, NODE *nd);
extern void new_line(FILE *strm, MESSAGETYPE type);
extern NODE *lshow(NODE *args);
extern NODE *ltype(NODE *args);
extern NODE *lprint(NODE *args);

extern
void
InitializeStringPrintInformation(
    char * Buffer,
    size_t BufferLength
    );

extern size_t FinalizeStringPrintInformation();

extern
size_t
PrintNodeToString(
    const NODE * Node,
    char *       Buffer,
    size_t       BufferLength
    );

extern
size_t
PrintNodeToString(
    const NODE * Node,
    char *       Buffer,
    size_t       BufferLength,
    int          PrintDepthLimit,
    int          PrintWidthLimit
    );

// global variables
extern bool print_backslashes;

#endif // __PRINT_H_
