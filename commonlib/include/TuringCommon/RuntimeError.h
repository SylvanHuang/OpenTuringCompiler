#ifndef _TURING_RUNTIME_ERROR_H_
#define _TURING_RUNTIME_ERROR_H_

namespace TuringCommon {
    //! raise a runtime error and halt the execution of the program. Does not return.
    //! \param isWarning set to true for a non-fatal error. Function returns when this is true.
	void runtimeError(const char *errMsg, bool isWarning = false);
}

#endif