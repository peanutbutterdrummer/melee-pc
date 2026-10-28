// libstdcxx13compat.cpp - the prebuilt Dawn archive was compiled with GCC 13+
// libstdc++ headers, which outline the overlapping-replace path of
// std::string::_M_replace into _M_replace_cold. The firmware's GCC 12
// libstdc++ lacks it, so provide it under its mangled name. Logic copied from
// GCC 13 basic_string.tcc (traits move = memmove, copy = memcpy).
#include <cstddef>
#include <cstring>

extern "C" void
_ZNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEE15_M_replace_coldEPcmPKcmm(
    void* /*self*/, char* p, std::size_t len1, const char* s, std::size_t len2, std::size_t how_much) {
    if (len2 && len2 <= len1) std::memmove(p, s, len2);
    if (how_much && len1 != len2) std::memmove(p + len2, p + len1, how_much);
    if (len2 > len1) {
        if (s + len2 <= p + len1) {
            std::memmove(p, s, len2);
        } else if (s >= p + len1) {
            const std::size_t poff = (s - p) + (len2 - len1);
            std::memcpy(p, p + poff, len2);
        } else {
            const std::size_t nleft = (p + len1) - s;
            std::memmove(p, s, nleft);
            std::memcpy(p + nleft, p + len2, len2 - nleft);
        }
    }
}
