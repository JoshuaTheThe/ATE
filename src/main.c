#include <ate.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <assert.h>

static struct termios orig_termios;
static int old_flags;

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

void get_terminal_size(int *rows, int *cols)
{
        struct winsize w;
        if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0)
        {
                *rows = w.ws_row;
                *cols = w.ws_col;
        }
        else
        {
                *rows = 24;
                *cols = 80;
        }
}

void disable_raw_mode(void)
{
        tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios);
        fcntl(STDIN_FILENO, F_SETFL, old_flags);
}

void enable_raw_mode(void)
{
        tcgetattr(STDIN_FILENO, &orig_termios);
        old_flags = fcntl(STDIN_FILENO, F_GETFL, 0);
        atexit(disable_raw_mode);
        fcntl(STDIN_FILENO, F_SETFL, old_flags | O_NONBLOCK);
        
        struct termios raw = orig_termios;
        
        raw.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | IXON | IXOFF | IXANY);
        raw.c_oflag |= ONLCR;
        raw.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN | TOSTOP);
        
        raw.c_cc[VMIN] = 0;
        raw.c_cc[VTIME] = 0;
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

int getch(void)
{
        int chr = 0;
        int n = read(STDIN_FILENO, &chr, 1);
        if (n == 0)
                return EOF;
        return chr;
}

int main(int c, char **v)
{
        int term_rows, term_cols;
        get_terminal_size(&term_rows, &term_cols);
        
        if (c < 2)
                return 1;

        enable_raw_mode();
        
        ATE_BufferManager Man = ATE_NewManager();
        
        for (size_t i = 1; i < c; ++i)
        {
                ATE_Buffer *New = ATE_OpenBuffer(&Man);
                New->WindowSize.Y = term_rows;
                New->WindowSize.X = term_cols;
                New->WindowPos.Y = 0;
                New->WindowPos.X = 0;
                New->CursorPos.Y = 0;
                New->CursorPos.X = 0;
                New->Path        = ATE_CreateText(v[i]);
                FILE *fp = fopen(v[i], "rb");
                if (!fp)
                        continue;
                ATE_ReadFile(New, fp);
                ATE_ComputeLines(&New->Data);
                fclose(fp);
        }
        
        ATE_Render(&Man, stdout);
        while (1)
        {
                int key = getch();
                if (key != EOF)
                {
                        ATE_Buffer *Buffer = ATE_GetFocused(&Man);
                        if (key == CTRL('h') && Buffer->CursorPos.X > 0)
                        {
                                Buffer->CursorPos.X -= 1;
                        }
                        else if (key == CTRL('c'))
                        {
                                ATE_FreeText(&Man.Clipboard);
                                Man.Clipboard = ATE_YankSelection(
                                                        Buffer,
                                                        Buffer->SelectionStart,
                                                        Buffer->SelectionEnd);
                        }
                        else if (key == CTRL('v'))
                        {
                                ATE_InsertText(Buffer,
                                               &Man.Clipboard,
                                               Buffer->CursorPos);
                        }
                        else if (key == CTRL('@'))
                        {
                                Buffer->Selecting = !Buffer->Selecting;
                                if (Buffer->Selecting)
                                        Buffer->SelectionStart = Buffer->CursorPos;
                                else
                                        Buffer->SelectionEnd = Buffer->CursorPos;
                        }
                        else if (key == CTRL('v'))
                        {
                                Buffer->SelectionStart = Buffer->CursorPos;
                        }
                        else if (key == CTRL('s'))
                        {
                                if (Buffer->Path.Data[Buffer->Path.Count-1])
                                {
                                        ATE_AppendCharToText(&Buffer->Path, 0);
                                }
                                FILE *fp = fopen(Buffer->Path.Data, "w");
                                if (!fp)
                                        continue;
                                ATE_WriteFile(Buffer, fp);
                                fclose(fp);
                        }
                        else if (key == CTRL('z'))
                        {
                                if (Man.Focused == 0)
                                        Man.Focused = Man.Buffers.Count - 1;
                                else
                                        Man.Focused--;
                                Buffer = ATE_GetFocused(&Man);
                        }
                        else if (key == CTRL('x'))
                        {
                                Man.Focused++;
                                if (Man.Focused >= Man.Buffers.Count)
                                        Man.Focused = 0;
                                Buffer = ATE_GetFocused(&Man);
                        }
                        else if (key == CTRL('l'))
                        {
                                const size_t len = ATE_SizeOfLine(&Buffer->Data, Buffer->CursorPos.Y);
                                Buffer->CursorPos.X = MIN(Buffer->CursorPos.X + 1, len-1);
                        }
                        else if (key == CTRL('j'))  // Down
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
                        else if (key == CTRL('w'))
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
                        else if (key == CTRL('b'))
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
                        else if (key == CTRL('e'))
                        {
                                const size_t len = ATE_SizeOfLine(&Buffer->Data, Buffer->CursorPos.Y);
                                Buffer->CursorPos.X = len - 1;
                        }
                        else if (key == CTRL('^'))
                        {
                                Buffer->CursorPos.X = 0;
                        }
                        else if (key == CTRL('k'))  // Up
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
                        else if (key == CTRL('q'))
                                break;
                        else if (key == '\b' || key == 127)
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
                        else if (key == '\n')
                        {
                                const size_t offset = Buffer->CursorPos.X + Buffer->Data.LineOffsets[Buffer->CursorPos.Y];
                                DA_INSERT(&Buffer->Data, offset, key);
                                ATE_ComputeLines(&Buffer->Data);
                                Buffer->CursorPos.Y += 1;
                                Buffer->CursorPos.X  = 0;
                        }
                        else if (key >= ' ' && key <= '~')
                        {
                                const size_t offset = Buffer->CursorPos.X + Buffer->Data.LineOffsets[Buffer->CursorPos.Y];
                                DA_INSERT(&Buffer->Data, offset, key); 
                                ATE_ComputeLines(&Buffer->Data);
                                Buffer->CursorPos.X += 1;
                        }
                        ATE_Render(&Man, stdout);
                }
        }

        ATE_CloseManager(&Man);
        disable_raw_mode();
        printf("\033[2J\033[H");  // Clear screen on exit
        return 0;
}

