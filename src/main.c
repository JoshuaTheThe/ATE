#include <ate.h>
#include <cmd.h>

#include <sys/ioctl.h>
#include <unistd.h>
#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <assert.h>

static struct termios orig_termios;
static int old_flags;

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
        while (!Man.ShouldClose)
        {
                int key = getch();
                if (key != EOF)
                {
                        ATE_EnterCommand(&Man, key);
                        ATE_Render(&Man, stdout);
                }
        }

        ATE_CloseManager(&Man);
        disable_raw_mode();
        printf("\033[2J\033[H");  // Clear screen on exit
        return 0;
}

