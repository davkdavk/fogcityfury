// XSDK's startup reads the console time zone through an XAM export Xenia does not implement.
// Fog City Fury uses monotonic ticks, so keep UTC and let startup reach C++ constructors.
extern "C" void xsdk_system_set_time_zone(void) {}
