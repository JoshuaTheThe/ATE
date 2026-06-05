
#ifndef BUFFER_H
#define BUFFER_H

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define MIN(a,b) \
        (((a) < (b)) ? (a) : (b))
#define MAX(a,b) \
        (((a) > (b)) ? (a) : (b))

#define CTRL(CHR) (toupper(CHR) - 'A' + 1)

// t s o d i n g
#define DA_APPEND(xs, x)                                                             \
    do {                                                                             \
        if ((xs)->Count >= (xs)->Capacity) {                                         \
            if ((xs)->Capacity == 0) (xs)->Capacity = 256;                           \
            else (xs)->Capacity *= 2;                                                \
            (xs)->Data = realloc((xs)->Data, (xs)->Capacity*sizeof(*(xs)->Data)); \
        }                                                                            \
                                                                                     \
        (xs)->Data[(xs)->Count++] = (x);                                            \
    } while (0)

// ai
#define DA_INSERT(xs, i, x)                                                          \
    do {                                                                             \
        /* Ensure capacity for one more element */                                   \
        if ((xs)->Count + 1 > (xs)->Capacity) {                                      \
            if ((xs)->Capacity == 0) (xs)->Capacity = 256;                           \
            else (xs)->Capacity *= 2;                                                \
            (xs)->Data = realloc((xs)->Data, (xs)->Capacity * sizeof(*(xs)->Data)); \
        }                                                                            \
                                                                                     \
        /* Shift elements to the right to make space */                              \
        for (size_t _idx = (xs)->Count; _idx > (i); _idx--) {                        \
            (xs)->Data[_idx] = (xs)->Data[_idx - 1];                                 \
        }                                                                            \
                                                                                     \
        /* Insert the new element */                                                 \
        (xs)->Data[i] = (x);                                                         \
        (xs)->Count++;                                                               \
    } while (0)

// ai
#define DA_POP(xs)                                                                   \
    do {                                                                             \
        if ((xs)->Count > 0) {                                                       \
            (xs)->Count--;                                                           \
        }                                                                            \
    } while (0)

// ai
#define DA_REMOVE_FAST(xs, i)                                                   \
    do {                                                                             \
        assert((i) < (xs)->Count);                                                   \
        /* Replace with last element */                                              \
        (xs)->Data[i] = (xs)->Data[(xs)->Count - 1];                                 \
        (xs)->Count--;                                                               \
    } while (0)

// ai
#define DA_REMOVE(xs, i)                                                             \
    do {                                                                             \
        assert((i) < (xs)->Count);                                                   \
        /* Shift elements left to fill the gap */                                    \
        for (size_t _idx = (i); _idx < (xs)->Count - 1; _idx++) {                    \
            (xs)->Data[_idx] = (xs)->Data[_idx + 1];                                 \
        }                                                                            \
        (xs)->Count--;                                                               \
    } while (0)

typedef long ATE_Offset;

typedef struct
{
        size_t    X,Y;
} ATE_Position;

typedef struct ATE_Text
{
        char     *Data;
        size_t    Count;
        size_t    Capacity;
        
        ATE_Offset *LineOffsets; // LUT, compute when needed
        size_t      Lines;
} ATE_Text;

typedef struct
{
        ATE_Text     Data;
        ATE_Text     Path;
        ATE_Position CursorPos;
        ATE_Position WindowPos; // scroll offset
        ATE_Position WindowSize;// dimensions of tty
        ATE_Position SelectionStart;
        ATE_Position SelectionEnd;
        bool         Selecting;
} ATE_Buffer;

typedef struct
{
        ATE_Position *Data[2]; // Start/End
        size_t        Count;
        size_t        Capacity;
} ATE_FindResult;

typedef struct
{
        struct
        {
                ATE_Buffer *Data;
                size_t      Count;
                size_t      Capacity;
        } Buffers;

        ATE_Text Clipboard;
        ATE_Text Command;
        size_t   Focused;
        bool     ShouldClose;
} ATE_BufferManager;

/*
 * Rendering
 * */

void              ATE_Render(ATE_BufferManager *Manager, FILE *fp);

/*
 * Buffer Management
 * */

ATE_BufferManager ATE_NewManager(void);
void              ATE_CloseManager(ATE_BufferManager *Manager);
size_t            ATE_IndexOf(ATE_BufferManager *Manager, ATE_Buffer *Buffer);
ATE_Buffer       *ATE_GetFocused(ATE_BufferManager *Manager);

/*
 * Buffer Creation / Manual Destruction
 */

ATE_Buffer       *ATE_OpenBuffer(ATE_BufferManager *Manager);
void              ATE_CloseBufferIdx(ATE_BufferManager *Manager, size_t Index);
void              ATE_CloseBuffer(ATE_BufferManager *Manager, ATE_Buffer *Buffer);
void              ATE_CloseFocusedBuffer(ATE_BufferManager *Manager);

/*
 * Buffer Modification
 * */

void              ATE_ComputeLines(ATE_Text *Text); // automatically called by internal functions
void              ATE_ReadFile(ATE_Buffer *Buffer, FILE *fp);
void              ATE_WriteFile(ATE_Buffer *Buffer, FILE *fp);
void              ATE_InsertText(ATE_Buffer *Buffer,
                                 ATE_Text *Data,
                                 ATE_Position Pos);
void              ATE_DeleteSelection(ATE_Buffer *Buffer,
                                      ATE_Position Start,
                                      ATE_Position End);
void              ATE_AdvanceTo(ATE_Buffer *Buffer,
                                ATE_Position NewPos);
void              ATE_AdvanceBy(ATE_Buffer *Buffer,
                                ATE_Position RelPos);
void              ATE_WordSelect(ATE_Buffer *Buffer,
                                 ATE_Position Start,
                                 ATE_Position *Out_Start,
                                 ATE_Position *Out_End);
void              ATE_LineSelect(ATE_Buffer *Buffer,
                                 ATE_Position Start,
                                 ATE_Position *Out_Start,
                                 ATE_Position *Out_End);
ATE_Text          ATE_YankSelection(ATE_Buffer *Buffer,
                                    ATE_Position Start,
                                    ATE_Position End);
ATE_FindResult    ATE_Find(ATE_Buffer *Buffer,
                           ATE_Text *Text);
void              ATE_CleanFind(ATE_FindResult Found);

// e.g. copy+paste is its own buffer
// that is done via ATE_YankSelection/ATE_InsertText

/*
 * Text
 * */

ATE_Text         ATE_CreateText(char Data[]);
void             ATE_AppendCharToText(ATE_Text *Text, char chr);
void             ATE_FreeText(ATE_Text *Text);
size_t           ATE_SizeOfLine(ATE_Text *Text, size_t Line);

#endif
