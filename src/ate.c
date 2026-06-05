
#include <ate.h>

static struct termios orig_termios;
static int old_flags;

int getch(void)
{
        int chr = 0;
        int n = read(STDIN_FILENO, &chr, 1);
        if (n == 0)
                return EOF;
        return chr;
}

static void disable_raw_mode(void)
{
        tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios);
        fcntl(STDIN_FILENO, F_SETFL, old_flags);
}

static void enable_raw_mode(void)
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

static void get_terminal_size(int *rows, int *cols)
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

ATE_BufferManager ATE_NewManager(void)
{
        ATE_BufferManager man = {0};
        enable_raw_mode();
        return man;
}

void ATE_CloseManager(ATE_BufferManager *man)
{
        while (man->Buffers.Count)
                ATE_CloseBufferIdx(man, 0);
        if (man->Buffers.Data)
                free(man->Buffers.Data);
        ATE_FreeText(&man->Clipboard);
        man->Focused = 0;
        disable_raw_mode();
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
        int term_rows, term_cols;
        ATE_Buffer new_buffer = {0};
        get_terminal_size(&term_rows, &term_cols);
        new_buffer.WindowSize.X = 80;
        new_buffer.WindowSize.Y = 25;
        new_buffer.WindowSize.Y = term_rows;
        new_buffer.WindowSize.X = term_cols;
        new_buffer.WindowPos.Y = 0;
        new_buffer.WindowPos.X = 0;
        new_buffer.CursorPos.Y = 0;
        new_buffer.CursorPos.X = 0;
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
        Text->Count       = 0;
        Text->Capacity    = 0;
        Text->Lines       = 0;
        Text->Data        = NULL;
        Text->LineOffsets = NULL;
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

void ATE_OpenPath(ATE_BufferManager *Manager, ATE_Text *Path)
{
        ATE_Text _path = ATE_CopyText(Path);
        DA_APPEND(&_path, 0); // null terminate, ATE doesn't use null termination
        struct stat path_stat;
        if (stat(_path.Data, &path_stat) != 0)
        {
                ATE_FreeText(&_path);
                return;
        }

        ATE_Buffer *New = ATE_OpenBuffer(Manager);
        New->Path       = ATE_CopyText(Path);

        if (S_ISDIR(path_stat.st_mode))
        {
                New->Type = ATE_DIR;
                DIR *dir = opendir(_path.Data);
                if (!dir)
                {
                        ATE_CloseBuffer(Manager, New);
                        ATE_FreeText(&_path);
                        return;
                }

                struct dirent *entry;
                char header[256];
                snprintf(header, sizeof(header), "Directory: %s\n\n", _path.Data);
                DA_APPEND_MANY(&New->Data, header, strlen(header));

                // Add column headers
                char *headers = "Permissions  Links Owner    Group    Size     Date Modified        Name\n";
                DA_APPEND_MANY(&New->Data, headers, strlen(headers));

                while ((entry = readdir(dir)) != NULL)
                {
                        char fullpath[PATH_MAX];
                        snprintf(fullpath, sizeof(fullpath), "%s/%s", _path.Data, entry->d_name);
                        
                        struct stat st;
                        if (stat(fullpath, &st) != 0)
                                continue;
                        
                        char line[512];
                        char perms[12];
                        char date[20];
                        char size_str[12];
                        
                        // Format permissions (rwx style)
                        perms[0] = S_ISDIR(st.st_mode) ? 'd' : '-';
                        perms[1] = (st.st_mode & S_IRUSR) ? 'r' : '-';
                        perms[2] = (st.st_mode & S_IWUSR) ? 'w' : '-';
                        perms[3] = (st.st_mode & S_IXUSR) ? 'x' : '-';
                        perms[4] = (st.st_mode & S_IRGRP) ? 'r' : '-';
                        perms[5] = (st.st_mode & S_IWGRP) ? 'w' : '-';
                        perms[6] = (st.st_mode & S_IXGRP) ? 'x' : '-';
                        perms[7] = (st.st_mode & S_IROTH) ? 'r' : '-';
                        perms[8] = (st.st_mode & S_IWOTH) ? 'w' : '-';
                        perms[9] = (st.st_mode & S_IXOTH) ? 'x' : '-';
                        perms[10] = '\0';
                        
                        // Format modification time
                        struct tm *tm = localtime(&st.st_mtime);
                        strftime(date, sizeof(date), "%Y-%m-%d %H:%M", tm);
                        
                        // Format size
                        if (S_ISDIR(st.st_mode))
                                snprintf(size_str, sizeof(size_str), "%-7s ", "<DIR>");
                        else if (st.st_size < 1024)
                                snprintf(size_str, sizeof(size_str), "%-7ld ", st.st_size);
                        else if (st.st_size < 1024*1024)
                                snprintf(size_str, sizeof(size_str), "%-7.1fK", st.st_size/1024.0);
                        else if (st.st_size < 1024*1024*1024)
                                snprintf(size_str, sizeof(size_str), "%-7.1fM", st.st_size/(1024.0*1024.0));
                        else
                                snprintf(size_str, sizeof(size_str), "%-7.1fG", st.st_size/(1024.0*1024.0*1024.0));
                        
                        // Format line
                        snprintf(line, sizeof(line), "%-12s %5ld %-8s %-8s %7s %-20s %s\n",
                                 perms,
                                 (long)st.st_nlink,
                                 getpwuid(st.st_uid) ? getpwuid(st.st_uid)->pw_name : "?",
                                 getgrgid(st.st_gid) ? getgrgid(st.st_gid)->gr_name : "?",
                                 size_str,
                                 date,
                                 entry->d_name);
                        
                        DA_APPEND_MANY(&New->Data, line, strlen(line));
                        
                        // Optional: Add symlink target
                        if (S_ISLNK(st.st_mode))
                        {
                                char link_target[PATH_MAX];
                                ssize_t len = readlink(fullpath, link_target, sizeof(link_target)-1);
                                if (len != -1)
                                {
                                        link_target[len] = '\0';
                                        char link_line[PATH_MAX+20];
                                        snprintf(link_line, sizeof(link_line), "  -> %s\n", link_target);
                                        DA_APPEND_MANY(&New->Data, link_line, strlen(link_line));
                                }
                        }
                }

                closedir(dir);
                ATE_ComputeLines(&New->Data);
        }
        else if (S_ISREG(path_stat.st_mode))
        {
                New->Type = ATE_FIL;
                FILE *fp = fopen(_path.Data, "rb");
                if (fp)
                {
                        ATE_ReadFile(New, fp);
                        fclose(fp);
                }
                else
                {
                        ATE_CloseBuffer(Manager, New);
                }
        }
        ATE_FreeText(&_path);
}

ATE_Text ATE_CopyText(ATE_Text *Text)
{
        ATE_Text New    = *Text;
        New.Data        = calloc(1, Text->Capacity);
        New.LineOffsets = calloc(Text->Lines, sizeof(*Text->LineOffsets));
        memcpy(New.Data, Text->Data, Text->Count);
        memcpy(New.LineOffsets, Text->LineOffsets, Text->Lines*sizeof(*Text->LineOffsets));
        return New;
}
