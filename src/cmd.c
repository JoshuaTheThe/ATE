
// cmd.c
// Command Manager

#include <cmd.h>

size_t next_word_start(ATE_Text *Data, size_t line, size_t col)
{
        const char *text = &Data->Data[Data->LineOffsets[line]];
        size_t len = ATE_SizeOfLine(Data, line);
        while (col < len && (text[col] == ' ' || text[col] == '\t'))
                col++;
        while (col < len && text[col] != ' ' && text[col] != '\t')
                col++;
        while (col < len && (text[col] == ' ' || text[col] == '\t'))
                col++;
        return col;
}

size_t prev_word_start(ATE_Text *Data, size_t line, size_t col)
{
        const char *text = &Data->Data[Data->LineOffsets[line]];
        if (col == 0)
                return 0;
        col--;
        while (col > 0 && (text[col] == ' ' || text[col] == '\t'))
                col--;
        while (col > 0 && text[col-1] != ' ' && text[col-1] != '\t')
                col--;
        return col;
}

void ATE_EnterLongCommand(ATE_BufferManager *Manager, char NewChar)
{
        ATE_AppendCharToText(&Manager->Command, NewChar);
        if (CTRL(NewChar) != NewChar)
        {
                ATE_Buffer *Buffer = ATE_GetFocused(Manager);
                // evaluate then free
                if (Manager->Command.Data[0] == CTRL('x') && Manager->Command.Data[1] == 'q')
                {
                        Manager->ShouldClose = true;
                }
                else if (Manager->Command.Data[0] == CTRL('x') && Manager->Command.Data[1] == 'c')
                {
                        ATE_CloseFocusedBuffer(Manager);
                        if (Manager->Buffers.Count == 0)
                                Manager->ShouldClose = true;
                }
                else if (Manager->Command.Data[0] == CTRL('x') && Manager->Command.Data[1] == 'n')
                {
                        Manager->Focused++;
                        if (Manager->Focused >= Manager->Buffers.Count)
                                Manager->Focused = 0;
                }
                
                else if (Manager->Command.Data[0] == CTRL('x') && Manager->Command.Data[1] == 'p')
                {
                        if (Manager->Focused == 0)
                                Manager->Focused = Manager->Buffers.Count - 1;
                        else
                                Manager->Focused--;
                }
                else if (Manager->Command.Data[0] == CTRL('x') && Manager->Command.Data[1] == 'w')
                {
                        if (Buffer->Path.Data[Buffer->Path.Count-1])
                        {
                                ATE_AppendCharToText(&Buffer->Path, 0);
                        }
                        FILE *fp = fopen(Buffer->Path.Data, "w");
                        if (fp)
                        {
                                ATE_WriteFile(Buffer, fp);
                                fclose(fp);
                        }
                }
                else if (Manager->Command.Data[0] == CTRL('x') && Manager->Command.Data[1] == 'd')
                {
                        ATE_Text Text = ATE_CreateText(".");
                        ATE_OpenPath(Manager, &Text);
                        ATE_FreeText(&Text);
                }
                else if (Manager->Command.Data[0] == CTRL('x') && (Manager->Command.Data[1] == 'o' || Manager->Command.Data[1] == 'r'))
                {
                        bool replace = Manager->Command.Data[1] == 'r';
                        ATE_FreeText(&Manager->Command);
                        size_t saved_line = Buffer->CursorPos.Y;
                        size_t saved_col = Buffer->CursorPos.X;
                        while (Buffer->CursorPos.X > 0)
                        {
                                const char *text = &Buffer->Data.Data[Buffer->Data.LineOffsets[Buffer->CursorPos.Y]];
                                char prev_char = text[Buffer->CursorPos.X - 1];
                                if (prev_char == ' ' || prev_char == '\t' || prev_char == '\n')
                                        break;
                                ATE_EnterCommand(Manager, CTRL('h'));
                        }
                        
                        ATE_EnterCommand(Manager, CTRL('v'));
                        size_t line_len = ATE_SizeOfLine(&Buffer->Data, Buffer->CursorPos.Y);
                        while (Buffer->CursorPos.X < line_len)
                        {
                                const char *text = &Buffer->Data.Data[Buffer->Data.LineOffsets[Buffer->CursorPos.Y]];
                                char next_char = text[Buffer->CursorPos.X];
                                if (next_char == ' ' || next_char == '\t' || next_char == '\n')
                                        break;
                                ATE_EnterCommand(Manager, CTRL('l'));
                        }
                        
                        ATE_EnterCommand(Manager, CTRL('v'));
                        ATE_EnterCommand(Manager, CTRL('y'));
                        if (Manager->Clipboard.Data && Manager->Clipboard.Count > 0)
                        {
                                ATE_Text RelPath = ATE_CopyText(&Buffer->Path);
                                if (RelPath.Count > 0 && RelPath.Data[RelPath.Count-1] != '/')
                                        ATE_AppendCharToText(&RelPath, '/');
                                for (size_t i = 0; i < Manager->Clipboard.Count; i++)
                                {
                                        char c = Manager->Clipboard.Data[i];
                                        if (c != '\n' && c != '\r')  // Skip newlines
                                                ATE_AppendCharToText(&RelPath, c);
                                }
                                
                                ATE_OpenPath(Manager, &RelPath);
                                ATE_FreeText(&RelPath);
                                ATE_FreeText(&Manager->Clipboard);
                        }
                        
                        Buffer->CursorPos.Y = saved_line;
                        Buffer->CursorPos.X = saved_col;
                        if (replace)
                                ATE_CloseBuffer(Manager, Buffer);
                }

                ATE_FreeText(&Manager->Command);
        }
}

void ATE_EnterCommand(ATE_BufferManager *Manager, char NewChar)
{
        ATE_Buffer *Buffer = ATE_GetFocused(Manager);
        if (Buffer->Selecting)
        {
                Buffer->SelectionEnd = Buffer->CursorPos;
        }

        if (Manager->Command.Count > 0) // entering command
        {
                ATE_EnterLongCommand(Manager, NewChar);
        }

        /*
         * Section #0 of short commands:
         * -    movement
         * */

        // Sub Section #0 - standard movement
        else if (NewChar == CTRL('h') && Buffer->CursorPos.X > 0)
        {
                Buffer->CursorPos.X -= 1;
        }
        else if (NewChar == CTRL('j'))
        {
                if (Buffer->CursorPos.Y < Buffer->Data.Lines - 1)
                {
                        Buffer->CursorPos.Y++;
                        const size_t len = ATE_SizeOfLine(&Buffer->Data, Buffer->CursorPos.Y);
                        Buffer->CursorPos.X = MIN(Buffer->CursorPos.X, len - 1);
                        if (Buffer->CursorPos.Y >= Buffer->WindowPos.Y + Buffer->WindowSize.Y)
                                Buffer->WindowPos.Y = Buffer->CursorPos.Y - Buffer->WindowSize.Y + 1;
                }
        }
        else if (NewChar == CTRL('k'))
        {
                if (Buffer->CursorPos.Y > 0)
                {
                        Buffer->CursorPos.Y--;
                        const size_t len = ATE_SizeOfLine(&Buffer->Data, Buffer->CursorPos.Y);
                        Buffer->CursorPos.X = MIN(Buffer->CursorPos.X, len - 1);
                        if (Buffer->CursorPos.Y < Buffer->WindowPos.Y)
                                Buffer->WindowPos.Y = Buffer->CursorPos.Y;
                }
        }
        else if (NewChar == CTRL('l'))
        {
                const size_t len = ATE_SizeOfLine(&Buffer->Data, Buffer->CursorPos.Y);
                Buffer->CursorPos.X = MIN(Buffer->CursorPos.X + 1, len-1);
        }

        // Sub Section #1 - word and line based movement
        else if (NewChar == CTRL('w'))
        {
                const size_t len = ATE_SizeOfLine(&Buffer->Data, Buffer->CursorPos.Y);
                size_t new_x = next_word_start(&Buffer->Data, Buffer->CursorPos.Y, Buffer->CursorPos.X);
                
                if (new_x < len)
                {
                        Buffer->CursorPos.X = new_x;
                }
                else if (Buffer->CursorPos.Y < Buffer->Data.Lines - 1)
                {
                        Buffer->CursorPos.Y++;
                        Buffer->CursorPos.X = 0;
                        const size_t new_len = ATE_SizeOfLine(&Buffer->Data, Buffer->CursorPos.Y);
                        while (Buffer->CursorPos.X < new_len && 
                               (Buffer->Data.Data[Buffer->Data.LineOffsets[Buffer->CursorPos.Y] + Buffer->CursorPos.X] == ' ' ||
                                Buffer->Data.Data[Buffer->Data.LineOffsets[Buffer->CursorPos.Y] + Buffer->CursorPos.X] == '\t'))
                        {
                            Buffer->CursorPos.X++;
                        }
                }
                else
                {
                        Buffer->CursorPos.X = len - 1;
                }
        }
        else if (NewChar == CTRL('b'))
        {
                if (Buffer->CursorPos.X > 0)
                {
                        Buffer->CursorPos.X = prev_word_start(&Buffer->Data,
                                                               Buffer->CursorPos.Y,
                                                               Buffer->CursorPos.X);
                }
                else if (Buffer->CursorPos.Y > 0)
                {
                        Buffer->CursorPos.Y--;
                        size_t prev_len = ATE_SizeOfLine(&Buffer->Data, Buffer->CursorPos.Y);
                        Buffer->CursorPos.X = prev_word_start(&Buffer->Data,
                                                               Buffer->CursorPos.Y,
                                                               prev_len);
                }
        }
        else if (NewChar == CTRL('e'))
        {
                const size_t len = ATE_SizeOfLine(&Buffer->Data, Buffer->CursorPos.Y);
                Buffer->CursorPos.X = len - 1;
        }
        else if (NewChar == CTRL('^'))
        {
                Buffer->CursorPos.X = 0;
        }

        /*
         * Section #2 of short commands
         * -    copy+paste, select, undo+redo (in future)
         */

        // Sub Section #0 - copy+paste
        else if (NewChar == CTRL('y'))
        {
                ATE_FreeText(&Manager->Clipboard);
                Manager->Clipboard = ATE_YankSelection(
                                        Buffer,
                                        Buffer->SelectionStart,
                                        Buffer->SelectionEnd);
        }
        else if (NewChar == CTRL('p'))
        {
                ATE_InsertText(Buffer,
                               &Manager->Clipboard,
                               Buffer->CursorPos);
        }

        // Sub Section #1 - select
        else if (NewChar == CTRL('v'))
        {
                Buffer->Selecting = !Buffer->Selecting;
                if (Buffer->Selecting)
                        Buffer->SelectionStart = Buffer->CursorPos;
                else
                        Buffer->SelectionEnd = Buffer->CursorPos;
        }

        /*
         * Section #1 of short commands
         * -    insertion
         */
        else if (NewChar == '\b' || NewChar == 127)
        {
                if (Buffer->CursorPos.X > 0)
                {
                        const size_t offset = Buffer->CursorPos.X + Buffer->Data.LineOffsets[Buffer->CursorPos.Y] - 1;
                        if (offset < Buffer->Data.Count)
                        DA_REMOVE(&Buffer->Data, offset);
                        ATE_ComputeLines(&Buffer->Data);
                        Buffer->CursorPos.X -= 1;
                }
                else if (Buffer->CursorPos.Y > 0)
                {
                        size_t prev_line_len = ATE_SizeOfLine(&Buffer->Data, Buffer->CursorPos.Y - 1);
                        size_t merge_pos = Buffer->Data.LineOffsets[Buffer->CursorPos.Y];
                        if (merge_pos > 0 && Buffer->Data.Data[merge_pos - 1] == '\n')
                                DA_REMOVE(&Buffer->Data, merge_pos - 1);
                        ATE_ComputeLines(&Buffer->Data);
                        Buffer->CursorPos.Y -= 1;
                        Buffer->CursorPos.X = prev_line_len - 1;
                }
        }
        else if (NewChar == CTRL('n'))
        {
                const size_t offset = Buffer->CursorPos.X + Buffer->Data.LineOffsets[Buffer->CursorPos.Y];
                DA_INSERT(&Buffer->Data, offset, '\n');
                ATE_ComputeLines(&Buffer->Data);
                Buffer->CursorPos.Y += 1;
                Buffer->CursorPos.X  = 0;
        }
        else if (NewChar >= ' ' && NewChar <= '~')
        {
                const size_t offset = Buffer->CursorPos.X + Buffer->Data.LineOffsets[Buffer->CursorPos.Y];
                DA_INSERT(&Buffer->Data, offset, NewChar); 
                ATE_ComputeLines(&Buffer->Data);
                Buffer->CursorPos.X += 1;
        }

        /*
         * Section #2 of short commands
         * -    start long command with prefix
         */
        else if (NewChar == CTRL('x') ||
                 NewChar == CTRL('c'))
        {
                ATE_AppendCharToText(&Manager->Command, NewChar);
        }
}
