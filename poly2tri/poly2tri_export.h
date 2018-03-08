#pragma once

#ifdef POLY2TRI_STATIC_LIB
    #define POLY2TRI_API 
#else
    #ifdef _WIN32
        #ifdef POLY2TRI_EXPORTS_API
            #define POLY2TRI_API __declspec(dllexport)
        #else
            #define POLY2TRI_API __declspec(dllimport)
        #endif
    #else
        #ifdef POLY2TRI_EXPORTS_API
            #define POLY2TRI_API __attribute__ ((visibility ("default")))
        #else
            #define POLY2TRI_API
        #endif
    #endif
#endif
