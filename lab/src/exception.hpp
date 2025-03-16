//
// exception.hpp
// 
// Custom exception class, used by nice functions to throw domain exceptions.
// 
// (c) 2021 Tomaz Stih
// This code is licensed under MIT license (see LICENSE.txt for details).
// 
// 16.01.2021   tstih
// 
#ifndef _EXCEPTION_HPP
#define _EXCEPTION_HPP

#include "includes.hpp"

namespace nice {

//{{BEGIN.DEC}}
#define throw_ex(ex, what) \
        throw ex(what, __FILE__,__FUNCTION__,__LINE__);

    class nice_exception : public std::exception {
    public:
        nice_exception(
            std::string what,
            std::string file = nullptr,
            std::string func = nullptr,
            int line = 0) : what_(what), file_(file), func_(func), line_(line) {};
        std::string what() { return what_; }
    protected:
        std::string what_;
        std::string file_; // __FILE__
        std::string func_; // __FUNCTION__
        int line_; // __LINE__
    };
//{{END.DEC}}

}

#endif // _EXCEPTION_HPP