/*
*
*       Copyright (C) 1995 by the Regents of the University of California
*       Copyright (C) 1995 by George Mills
*
*      This program is free software; you can redistribute it and/or modify
*      it under the terms of the GNU General Public License as published by
*      the Free Software Foundation; either version 2 of the License, or
*      (at your option) any later version.
*
*      This program is distributed in the hope that it will be useful,
*      but WITHOUT ANY WARRANTY; without even the implied warranty of
*      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*      GNU General Public License for more details.
*
*      You should have received a copy of the GNU General Public License
*      along with this program; if not, write to the Free Software
*      Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*
*/

#include "pch.h"
#ifndef USE_PRECOMPILED_HEADER
   #include "fileswnd.h"
   #include "wrksp.h"
   #include "files.h"
   #include "init.h"
   #include "eval.h"
   #include "parse.h"
   #include "screenwindow.h"
   #include "cursor.h"
   #include "debugheap.h"

   #include "localizedstrings.h"
#endif

void filesave(const char *FileName)
{
    if (IsEditorOpen())
    {
        // Notify the user that they have an editor open
        // and that changes in the editor will not be saved.
#ifndef WX_PURE
        MessageBox(
            GetCommanderWindow(),
            LOCALIZED_EDITORISOPEN,
            LOCALIZED_INFORMATION,
            MB_OK | MB_ICONQUESTION);
#endif
    }

    PrintWorkspaceToFileStream(fopen(FileName, "w+"));
}


// Loads and evaluates the contents of a file.
//
// Returns true if the file could be opened for reading,
// even if there was error while evaluating the contents.
// Returns false if the file couldn't be opened for reading.
bool fileload(const char *Filename)
{
    bool isOk;

    FILE * filestream = fopen(Filename, "r");
    if (filestream != NULL)
    {
        // save all global state that may be modified
        NODE *previous_startup = Startup.GetValue();

        FIXNUM savedValueStatus = g_ValueStatus;
        bool   savedIsDirty     = IsDirty;
        bool   savedYieldFlag   = yield_flag;
        FILE * savedLoadStream  = loadstream;
        NODE * savedCurrentLine = vref(current_line);

        loadstream = filestream;

        yield_flag = false;
        lsetcursorwait(NIL);

        start_execution();

        while (!feof(loadstream) && NOT_THROWING)
        {
            assign(current_line, reader(loadstream, ""));
            NODE * exec_list = parser(current_line, true);
            g_ValueStatus = VALUE_STATUS_NotOk;
            eval_driver(exec_list);
        }
        fclose(loadstream);

        // Restore some of the global state before running the startup
        // instruction list.
        lsetcursorarrow(NIL);
        yield_flag = savedYieldFlag;

        // Restore loadstream so that we don't confuse to_helper
        // into reading more data from the current (closed) file stream.
        loadstream = savedLoadStream;

        // Run the any startup instruction list that may have been defined
        // when the file was loaded.  (The parameter "previous_startup" is
        // not what is run, but rather is used to detect if anything new
        // has been defined.
        runstartup(previous_startup);

        // Restore the rest of the global state.
        g_ValueStatus = savedValueStatus;
        IsDirty       = savedIsDirty;
        deref(current_line);
        current_line = savedCurrentLine;

        stop_execution();

        isOk = true;
    }
    else
    {
        isOk = false;
    }

    return isOk;
}
