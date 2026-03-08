#pragma once

#include "driver.h"


/*
зЂВс WFP Callout
*/
NTSTATUS InitialWfp(
	PDEVICE_OBJECT device
);

/*
зЂЯњ Callout
*/
VOID UnInitialWfp();
