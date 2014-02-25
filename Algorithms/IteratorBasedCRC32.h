/*

Copyright (c) 2013, Project OSRM, Dennis Luxen, others
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

Redistributions of source code must retain the above copyright notice, this list
of conditions and the following disclaimer.
Redistributions in binary form must reproduce the above copyright notice, this
list of conditions and the following disclaimer in the documentation and/or
other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

#ifndef ITERATORBASEDCRC32_H_
#define ITERATORBASEDCRC32_H_

#include "../Util/SimpleLogger.h"

#include <boost/crc.hpp>  // for boost::crc_32_type
#include <iostream>

template<class ContainerT>
class IteratorbasedCRC32 {
private:
    typedef typename ContainerT::iterator IteratorType;
    unsigned crc;

    typedef boost::crc_optimal<32, 0x1EDC6F41, 0x0, 0x0, true, true> my_crc_32_type;
    typedef unsigned (IteratorbasedCRC32::*CRC32CFunctionPtr)(char *str, unsigned len, unsigned crc);

    unsigned SoftwareBasedCRC32(char *str, unsigned len, unsigned ){
        boost::crc_optimal<32, 0x1EDC6F41, 0x0, 0x0, true, true> CRC32_processor;
        CRC32_processor.process_bytes( str, len);
        return CRC32_processor.checksum();
    }

    unsigned SSE42BasedCRC32( char *str, unsigned len, unsigned crc){
#if defined(__x86_64__)
        unsigned q=len/sizeof(unsigned),
                r=len%sizeof(unsigned),
                *p=(unsigned*)str/*, crc*/;

        //crc=0;
        while (q--) {
            __asm__ __volatile__(
                    ".byte 0xf2, 0xf, 0x38, 0xf1, 0xf1;"
                    :"=S"(crc)
                     :"0"(crc), "c"(*p)
            );
            ++p;
        }

        str=(char*)p;
        while (r--) {
            __asm__ __volatile__(
                    ".byte 0xf2, 0xf, 0x38, 0xf1, 0xf1;"
                    :"=S"(crc)
                     :"0"(crc), "c"(*str)
            );
            ++str;
        }
#endif
        return crc;
    }

#if defined(__x86_64__)
    unsigned cpuid(unsigned functionInput){
        unsigned eax;
        unsigned ebx;
        unsigned ecx;
        unsigned edx;
        asm("cpuid" : "=a" (eax), "=b" (ebx), "=c" (ecx), "=d" (edx) : "a" (functionInput));
        return ecx;
    }
#endif

    CRC32CFunctionPtr DetectNativeCRC32Support(){
#if defined(__x86_64__)
        static const int SSE42_BIT = 20;
        unsigned ecx = cpuid(1);
        bool has_SSE42 = ecx & (1 << SSE42_BIT);
#else
        bool has_SSE42 = false;
#endif
        if (has_SSE42) {
            SimpleLogger().Write() << "using hardware based CRC32 computation";
            return &IteratorbasedCRC32::SSE42BasedCRC32; //crc32 hardware accelarated;
        } else {
            SimpleLogger().Write() << "using software based CRC32 computation";
            return &IteratorbasedCRC32::SoftwareBasedCRC32; //crc32cSlicingBy8;
        }
    }
    CRC32CFunctionPtr crc_function;
public:
    IteratorbasedCRC32(): crc(0) {
        crc_function = DetectNativeCRC32Support();
    }

    // virtual ~IteratorbasedCRC32() { }

    unsigned operator()( IteratorType iter, const IteratorType end) {
        unsigned crc = 0;
        while(iter != end) {
            char * data = reinterpret_cast<char*>(&(*iter) );
            crc =((*this).*(crc_function))(data, sizeof(typename ContainerT::value_type*), crc);
            ++iter;
        }
        return crc;
    }
};

#endif /* ITERATORBASEDCRC32_H_ */
