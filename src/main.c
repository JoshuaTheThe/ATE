#include <ate.h>
#include <cmd.h>

int main(int c, char **v)
{
        ATE_BufferManager Man = ATE_NewManager();
        for (size_t i = 1; i < c; ++i)
        {
                ATE_Text Text = ATE_CreateText(v[i]);
                ATE_OpenPath(&Man, &Text);
                ATE_FreeText(&Text);
        }
        
        ATE_Text Text = ATE_CreateText(".");
        ATE_OpenPath(&Man, &Text);
        ATE_FreeText(&Text);

        ATE_Render(&Man, stdout);
        while (!Man.ShouldClose)
        {
                int key = getch();
                if (key != EOF)
                {
                        ATE_EnterCommand(&Man, key);
                        ATE_Render(&Man, stdout);
                }

                usleep(10000);
        }

        ATE_CloseManager(&Man);
        printf("\033[2J\033[H");  // Clear screen on exit
        return 0;
}

