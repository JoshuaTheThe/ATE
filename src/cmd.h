
#ifndef CMD_H
#define CMD_H

#include <ate.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <assert.h>

void ATE_EnterCommand(ATE_BufferManager *Manager, char NewChar);

#endif
