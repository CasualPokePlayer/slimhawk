#ifndef __MDFN_MEDNADISC_H
#define __MDFN_MEDNADISC_H

#include "slim_types.h"

class MednaDisc;

EXPORT void* mednadisc_LoadCD(const char* fname);
EXPORT int32 mednadisc_ReadSector(MednaDisc* disc, int lba, void* buf2448);
EXPORT void mednadisc_CloseCD(MednaDisc* disc);

#endif
