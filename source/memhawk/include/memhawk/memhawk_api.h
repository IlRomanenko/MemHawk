#ifndef MEM_HAWK_
#define MEM_HAWK_

extern "C" {

#define MemHawkLogLevelDebug 0
#define MemHawkLogLevelInfo 1
#define MemHawkLogLevelWarning 2
#define MemHawkLogLevelError 3
#define MemHawkLogLevelOff 4

void SetLogLevel(unsigned int level);

// todo: add more methods
}


#endif // MEM_HAWK_
