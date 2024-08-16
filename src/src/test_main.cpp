#ifndef UNIT_TEST
#include "CRSFHandset.h"
#include "devHandset.h"
#include "logging.h"

Stream *TxBackpack;

void setup()
{
	SEGGER_RTT_Init();
	DBGLN("PA9: %d, PA_9: %d, PA8: %d, PA_8: %d", PA9, PA_9, PA8, PA_8);
	__enable_irq();
	//TxBackpack = new NullStream();
	Handset_device.initialize();
	Handset_device.start();
	CRSFHandset::Port.listen();
	return;
}

void loop()
{
	return;
}
#endif
