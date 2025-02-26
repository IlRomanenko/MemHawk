#ifndef _MEM_HAWK_
#define _MEM_HAWK_

extern "C" {

#define MemHawkLogLevelDebug 0
#define MemHawkLogLevelInfo 1
#define MemHawkLogLevelWarning 2
#define MemHawkLogLevelError 3
#define MemHawkLogLevelOff 4

void SetUpSigHandler(int signal);

// todo: add more methods
}


#endif // _MEM_HAWK_
