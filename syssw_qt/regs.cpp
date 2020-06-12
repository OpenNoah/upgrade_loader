#include <stdio.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdint.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include "regs.h"

Regs *regs = 0;

Regs::Regs(QObject *parent): QObject(parent)
{
	fd  = -1;
	mem = 0;
}

Regs::~Regs()
{
	regs = 0;
}

bool Regs::init()
{
        // Open /dev/mem
        fd = open("/dev/mem", O_RDWR | O_SYNC);
        if (fd == -1) {
		QMessageBox::warning(0, "Registers", "Error mapping /dev/mem");
		return false;
	}

	// Map REGS block
        mem = mmap(0, REGS_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, REGS_BASE);
        if (mem == (void *)-1) {
		QMessageBox::warning(0, "Registers", "Error mapping register block");
		return false;
	}

	regs = this;
	return true;
}
