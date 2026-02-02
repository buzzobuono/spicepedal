#ifndef DEBUG_H
#define DEBUG_H

#include <iostream>

#ifdef DEBUG_MODE
    #define DEBUG_LOG(x) std::clog << "[DEBUG] " << x << std::endl
#else
    #define DEBUG_LOG(x) do {} while (0) 
#endif

#endif
