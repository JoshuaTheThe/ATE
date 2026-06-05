
#include <ate.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <math.h>
#include <limits.h>

ATE_BufferManager ATE_NewManager(void)
{
        ATE_BufferManager man = {0};
        return man;
}

void ATE_CloseManager(ATE_BufferManager *man)
{
        while (man->Buffers.Count)
                ATE_CloseBufferIdx(man, 0);
        if (man->Buffers.Data)
                free(man->Buffers.Data);
        man->Focused = 0;
}

size_t ATE_IndexOf(ATE_BufferManager *Manager, ATE_Buffer *Buffer)
{
        return Buffer - Manager->Buffers.Data;
}

ATE_Buffer *ATE_GetFocused(ATE_BufferManager *Manager)
{
        return &Manager->Buffers.Data[Manager->Focused];
}

ATE_Buffer *ATE_OpenBuffer(ATE_BufferManager *Manager)
{
        ATE_Buffer new_buffer = {0};
        new_buffer.WindowSize.X = 80;
        new_buffer.WindowSize.Y = 25;
        DA_APPEND(&Manager->Buffers, new_buffer);
        Manager->Focused = Manager->Buffers.Count - 1;
        return &Manager->Buffers.Data[Manager->Buffers.Count - 1];
}

void ATE_CloseBufferIdx(ATE_BufferManager *Manager, size_t Index)
{
        ATE_FreeText(&Manager->Buffers.Data[Index].Data);
        ATE_FreeText(&Manager->Buffers.Data[Index].Path);
        DA_REMOVE(&Manager->Buffers, Index);
}

void ATE_CloseBuffer(ATE_BufferManager *Manager, ATE_Buffer *Buffer)
{
        ATE_CloseBufferIdx(Manager, ATE_IndexOf(Manager, Buffer));
}

void ATE_CloseFocusedBuffer(ATE_BufferManager *Manager)
{
        ATE_CloseBufferIdx(Manager, Manager->Focused);
        Manager->Focused = 0;
}

void ATE_ComputeLines(ATE_Text *Text)
{
        if (Text->LineOffsets)
                free(Text->LineOffsets);
        long lines = 1;
        for (size_t i = 0; i < Text->Count; ++i)
                lines += Text->Data[i] == '\n';
        Text->LineOffsets = calloc(lines, sizeof(lines));
        Text->LineOffsets[0] = 0;
        for (size_t i = 0, l = 1; i < Text->Count; ++i)
                if (Text->Data[i] == '\n')
                        Text->LineOffsets[l++] = i+1;
        Text->Lines = lines;
}

void ATE_AdvanceTo(ATE_Buffer *Buffer,
                   ATE_Position NewPos)
{
        Buffer->CursorPos = NewPos;
}

void ATE_AdvanceBy(ATE_Buffer *Buffer,
                   ATE_Position RelPos)
{
        Buffer->CursorPos.X += RelPos.X;
        Buffer->CursorPos.Y += RelPos.Y;
}

void ATE_ReadFile(ATE_Buffer *Buffer, FILE *fp)
{
        fseek(fp, 0, SEEK_END);
        long size = ftell(fp);
        rewind(fp);
        
        ATE_FreeText(&Buffer->Data);
        
        Buffer->Data.Count = 0;
        Buffer->Data.Capacity = size > 0 ? size : 256;
        Buffer->Data.Data = malloc(Buffer->Data.Capacity);
        Buffer->Data.LineOffsets = NULL;
        
        if (size > 0)
        {
                size_t bytes_read = fread(Buffer->Data.Data, 1, size, fp);
                Buffer->Data.Count = bytes_read;
        }
        
        if (Buffer->Data.Count > 0 && Buffer->Data.Data[Buffer->Data.Count - 1] != '\n')
        {
                DA_APPEND(&Buffer->Data, '\n');
        }
        
        ATE_ComputeLines(&Buffer->Data);
}

void ATE_WriteFile(ATE_Buffer *Buffer, FILE *fp)
{
        fwrite(Buffer->Data.Data, 1, Buffer->Data.Count, fp);
}

void ATE_InsertText(ATE_Buffer *Buffer,
                    ATE_Text *Data,
                    ATE_Position Pos)
{
        ATE_ComputeLines(&Buffer->Data);
        size_t insert_idx = 0;
        for (size_t i = 0; i < Pos.Y && i < Buffer->Data.Lines; i++)
        {
                insert_idx += Buffer->Data.LineOffsets[i + 1] - Buffer->Data.LineOffsets[i];
        }
        
        insert_idx += Pos.X;
        
        for (size_t i = 0; i < Data->Count; i++)
        {
                DA_INSERT(&Buffer->Data, insert_idx + i, Data->Data[i]);
        }
        
        ATE_ComputeLines(&Buffer->Data);
}

void ATE_DeleteSelection(ATE_Buffer *Buffer,
                         ATE_Position Start,
                         ATE_Position End)
{
        if (Start.Y > End.Y || (Start.Y == End.Y && Start.X > End.X))
        {
                ATE_Position temp = Start;
                Start = End;
                End = temp;
        }
        
        size_t start_idx = 0, end_idx = 0;
        for (size_t i = 0; i < Start.Y && i < Buffer->Data.Lines; i++)
        {
                start_idx += Buffer->Data.LineOffsets[i + 1] - Buffer->Data.LineOffsets[i];
        }

        start_idx += Start.X;
        for (size_t i = 0; i < End.Y && i < Buffer->Data.Lines; i++)
        {
                end_idx += Buffer->Data.LineOffsets[i + 1] - Buffer->Data.LineOffsets[i];
        }

        end_idx += End.X;
        size_t delete_count = end_idx - start_idx;
        for (size_t i = start_idx; i < Buffer->Data.Count - delete_count; i++)
        {
                Buffer->Data.Data[i] = Buffer->Data.Data[i + delete_count];
        }
        Buffer->Data.Count -= delete_count;
}

void ATE_WordSelect(ATE_Buffer *Buffer,
                    ATE_Position Start,
                    ATE_Position *Out_Start,
                    ATE_Position *Out_End)
{
        size_t idx = 0;
        for (size_t i = 0; i < Start.Y && i < Buffer->Data.Lines; i++)
                idx += Buffer->Data.LineOffsets[i + 1] - Buffer->Data.LineOffsets[i];
        idx += Start.X;
        size_t word_start = idx;
        while (word_start > 0 && isalnum(Buffer->Data.Data[word_start - 1]))
        {
                word_start--;
        }
        
        size_t word_end = idx;
        while (word_end < Buffer->Data.Count && isalnum(Buffer->Data.Data[word_end]))
        {
                word_end++;
        }
        
        Out_Start->Y = 0;
        Out_Start->X = word_start;
        for (size_t i = 0; i < Buffer->Data.Lines; i++)
        {
                size_t line_len = Buffer->Data.LineOffsets[i + 1] - Buffer->Data.LineOffsets[i];
                if (word_start < line_len)
                {
                        Out_Start->Y = i;
                        Out_Start->X = word_start;
                        break;
                }
                word_start -= line_len;
        }
        
        Out_End->Y = 0;
        Out_End->X = word_end;
        for (size_t i = 0; i < Buffer->Data.Lines; i++)
        {
                size_t line_len = Buffer->Data.LineOffsets[i + 1] - Buffer->Data.LineOffsets[i];
                if (word_end <= line_len)
                {
                        Out_End->Y = i;
                        Out_End->X = word_end;
                        break;
                }
                word_end -= line_len;
        }
}

void ATE_LineSelect(ATE_Buffer *Buffer,
                    ATE_Position Start,
                    ATE_Position *Out_Start,
                    ATE_Position *Out_End)
{
        Out_Start->Y = Start.Y;
        Out_Start->X = 0;
        
        Out_End->Y = Start.Y;
        if (Start.Y + 1 < Buffer->Data.Lines)
        {
                size_t line_len = Buffer->Data.LineOffsets[Start.Y + 1] - Buffer->Data.LineOffsets[Start.Y];
                Out_End->X = line_len;
        }
        else
                Out_End->X = Buffer->Data.Count - Buffer->Data.LineOffsets[Start.Y];
}

ATE_Text ATE_YankSelection(ATE_Buffer *Buffer,
                           ATE_Position Start,
                           ATE_Position End)
{
        if (Start.Y > End.Y || (Start.Y == End.Y && Start.X > End.X))
        {
                ATE_Position temp = Start;
                Start = End;
                End = temp;
        }
        
        size_t start_idx = 0, end_idx = 0;
        for (size_t i = 0; i < Start.Y && i < Buffer->Data.Lines; i++)
        {
                start_idx += Buffer->Data.LineOffsets[i + 1] - Buffer->Data.LineOffsets[i];
        }

        start_idx += Start.X;
        for (size_t i = 0; i < End.Y && i < Buffer->Data.Lines; i++)
        {
                end_idx += Buffer->Data.LineOffsets[i + 1] - Buffer->Data.LineOffsets[i];
        }

        end_idx += End.X;
        size_t len = end_idx - start_idx;
        char *data = malloc(len + 1);
        memcpy(data, &Buffer->Data.Data[start_idx], len);
        data[len] = '\0';
        return (ATE_Text){
                .Data = data,
                .Count = len,
                .Capacity = len,
                .LineOffsets = NULL,
                .Lines = 0,
        };
}

ATE_FindResult ATE_Find(ATE_Buffer *Buffer,
                           ATE_Text *Text)
{
        ATE_FindResult res = {0};
        res.Count = 0;
        res.Capacity = 1024;
        res.Data[0] = malloc(res.Capacity * sizeof(ATE_Position));
        res.Data[1] = malloc(res.Capacity * sizeof(ATE_Position));
        for (size_t i = 0; i <= Buffer->Data.Count - Text->Count; i++)
        {
                if (memcmp(&Buffer->Data.Data[i], Text->Data, Text->Count) == 0)
                {
                        if (res.Count >= res.Capacity)
                        {
                                res.Capacity *= 2;
                                res.Data[0] = realloc(res.Data[0], res.Capacity * sizeof(ATE_Position));
                                res.Data[1] = realloc(res.Data[1], res.Capacity * sizeof(ATE_Position));
                        }
                        
                        size_t pos = i;
                        size_t line = 0;
                        while (line < Buffer->Data.Lines && 
                               pos >= (size_t)(Buffer->Data.LineOffsets[line + 1] - Buffer->Data.LineOffsets[line]))
                        {
                                pos -= (Buffer->Data.LineOffsets[line + 1] - Buffer->Data.LineOffsets[line]);
                                line++;
                        }

                        res.Data[0][res.Count].Y = line;
                        res.Data[0][res.Count].X = pos;
                        pos = i + Text->Count;
                        line = 0;
                        while (line < Buffer->Data.Lines && 
                               pos >= (size_t)(Buffer->Data.LineOffsets[line + 1] - Buffer->Data.LineOffsets[line]))
                        {
                                pos -= (Buffer->Data.LineOffsets[line + 1] - Buffer->Data.LineOffsets[line]);
                                line++;
                        }
                        res.Data[1][res.Count].Y = line;
                        res.Data[1][res.Count].X = pos;
                        
                        res.Count++;
                        i += Text->Count - 1;
                }
        }
        
        return res;
}

void ATE_CleanFind(ATE_FindResult Found)
{
        free(Found.Data[0]);
        free(Found.Data[1]);
}

ATE_Text ATE_CreateText(char Data[])
{
        size_t length = strlen(Data) + 1;
        char *data = calloc(1, length);
        memcpy(data, Data, length);
        return (ATE_Text){.Data=data,
                          .Count = length - 1,
                          .Capacity = length,
                          .LineOffsets = NULL,
                          .Lines = 0};
}

void ATE_AppendCharToText(ATE_Text *Text, char chr)
{
        DA_APPEND(Text, chr);
}

void ATE_FreeText(ATE_Text *Text)
{
        if (Text->Data)        free(Text->Data);
        if (Text->LineOffsets) free(Text->LineOffsets);
}

size_t ATE_SizeOfLine(ATE_Text *Text, size_t Line)
{
        if (Line >= Text->Lines)
                return 0;
        size_t i = Line;
        size_t line_length = (i + 1 < Text->Lines) ?
               Text->LineOffsets[i+1] - Text->LineOffsets[i] :
               Text->Count - Text->LineOffsets[i];
        return line_length;
}

void ATE_Render(ATE_BufferManager *Manager, FILE *fp)
{
        ATE_Buffer *Buffer = ATE_GetFocused(Manager);
        size_t padd = (size_t)(log10((double)Buffer->Data.Lines) + 1);
        int int_padd = (padd > INT_MAX) ? INT_MAX : (int)padd;
        size_t start_line = Buffer->WindowPos.Y;
        size_t end_line = MIN(start_line + Buffer->WindowSize.Y, Buffer->Data.Lines);

        printf("\033[2J\033[H");
        for (size_t i = start_line; i < end_line; ++i)
        {
                size_t line_length = (i + 1 < Buffer->Data.Lines) ?
                                Buffer->Data.LineOffsets[i+1] - Buffer->Data.LineOffsets[i] :
                                Buffer->Data.Count - Buffer->Data.LineOffsets[i];
                fprintf(fp, "%*zu ", int_padd, i + 1);
                if (line_length > 1)
                        fwrite(&Buffer->Data.Data[Buffer->Data.LineOffsets[i]], 1, line_length - 1, fp);
                if (i < end_line - 1)
                        fputc('\n', fp);
        }

        size_t remaining_lines = Buffer->WindowSize.Y - (end_line - start_line);
        for (size_t i = 0; i < remaining_lines; ++i)
        {
                if (i < remaining_lines - 1)
                        fprintf(fp, "~\n");
                else
                        fprintf(fp, "~");
        }

        fprintf(fp, "\033[%zu;%zuH", 
                (Buffer->CursorPos.Y - Buffer->WindowPos.Y) + 1, 
                (Buffer->CursorPos.X - Buffer->WindowPos.X) + 1 + int_padd + 1);
        fflush(fp);
}

