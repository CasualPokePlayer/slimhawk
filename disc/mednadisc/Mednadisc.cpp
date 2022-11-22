#include "Mednadisc.h"

#include "error.h"

#include "cdrom/CDAccess.h"
#include "cdrom/CDUtility.h"
#include "cdrom/cdromif.h"
#include "cdrom/CDAccess_Image.h"


class MednaDisc
{
public:
	~MednaDisc()
	{
		delete disc;
	}
	CDAccess* disc;
	CDUtility::TOC toc;
};

EXPORT void* mednadisc_LoadCD(const char* fname)
{
	CDAccess* disc = NULL;
	try {
		disc = CDAccess_Open(fname,false);
	}
	catch(MDFN_Error &) {
		return NULL;
	}

	MednaDisc* md = new MednaDisc();
	md->disc = disc;
	disc->Read_TOC(&md->toc);
	return md;
}

struct JustTOC
{
  uint8 first_track;
  uint8 last_track;
  uint8 disc_type;
};

EXPORT void mednadisc_ReadTOC(MednaDisc* md, JustTOC* justToc, CDUtility::TOC_Track *tracks101)
{
	CDUtility::TOC &toc = md->toc;
	justToc->first_track = toc.first_track;
	justToc->last_track = toc.last_track;
	justToc->disc_type = toc.disc_type;
	memcpy(tracks101,toc.tracks,sizeof(toc.tracks));
}

//NOTE: the subcode will come out interleaved.
//Don't try changing this unless youre REALLY bored. It's convoluted.
//If you do, make sure you have three states: must_interleave, must_deinterleaved and dontcare
EXPORT int32 mednadisc_ReadSector(MednaDisc* md, int lba, void* buf2448)
{
	CDAccess* disc = md->disc;
	CDUtility::TOC &toc = md->toc;
	try
	{
		disc->Read_Raw_Sector((uint8*)buf2448,lba);
	}	
	catch(MDFN_Error &) {
		return 0;
	}
	return 1;
}

EXPORT void mednadisc_CloseCD(MednaDisc* md)
{
	delete md;
} 
