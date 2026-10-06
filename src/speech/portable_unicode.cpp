#include "speech/portable_unicode.hpp"
#include <utf8proc.h>
#include <stdexcept>
#include <cstdlib>
namespace ortho {
std::string speech_unicode(const std::string& text) {
    utf8proc_uint8_t* decomposed=nullptr;
    const auto length=utf8proc_map(reinterpret_cast<const utf8proc_uint8_t*>(text.data()),text.size(),&decomposed,
        static_cast<utf8proc_option_t>(UTF8PROC_STABLE|UTF8PROC_COMPAT|UTF8PROC_DECOMPOSE));
    if(length<0)throw std::runtime_error("Invalid UTF-8 speech text");
    std::string result;
    for(utf8proc_ssize_t i=0;i<length;) {
        utf8proc_int32_t character;
        const auto count=utf8proc_iterate(decomposed+i,length-i,&character);
        if(count<0){std::free(decomposed);throw std::runtime_error("Invalid normalized speech text");}
        i+=count;utf8proc_uint8_t bytes[4];
        const auto size=utf8proc_encode_char(utf8proc_tolower(character),bytes);
        result.append(reinterpret_cast<const char*>(bytes),size);
    }
    std::free(decomposed);return result;
}
}
