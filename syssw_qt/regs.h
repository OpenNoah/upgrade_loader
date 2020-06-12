#pragma once

#include <stdint.h>
#include <qt.h>

#define REGS_BASE	0x10000000
#define REGS_SIZE	0x04000000

class Regs: public QObject
{
	Q_OBJECT
public:
	Regs(QObject *parent);
	~Regs();
	bool init();
	void *operator()(uint32_t addr) {return (uint8_t *)mem + (addr - REGS_BASE);}

private:
	int fd;
	void *mem;
};

extern Regs *regs;
