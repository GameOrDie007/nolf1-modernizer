// VRCrash.h: a crash report a player can send.
//
// A crash nothing else catches writes crash.txt beside this run's client.log:
// the fault, where it happened as module + offset (offsets resolve against a
// build's map file on any machine running the same binaries), the registers,
// a stack walk and the loaded modules. The first few faults of a run are also
// noted in client.log as they happen, because an engine that catches its own
// crash never reaches an unhandled-exception filter.

#ifndef _VRCRASH_H_
#define _VRCRASH_H_

namespace VRCrash
{
	// Once, after VRLog has made this run's folder.
	void Install(const char* pszRunDir);
}

#endif
