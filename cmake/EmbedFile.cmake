file(READ "${QZDL_EMBED_INPUT}" bytes HEX)

string(REGEX REPLACE "(..)" "0x\\1," bytes "${bytes}")
string(REGEX REPLACE "(0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,)" "\\1\n    " bytes "${bytes}")

file(WRITE "${QZDL_EMBED_OUTPUT}"
"// Generated from ${QZDL_EMBED_INPUT}. Do not edit.

#include <cstddef>

namespace Embedded {

extern const unsigned char ${QZDL_EMBED_SYMBOL}[];
extern const std::size_t ${QZDL_EMBED_SYMBOL}Size;

const unsigned char ${QZDL_EMBED_SYMBOL}[] = {
    ${bytes}
};

const std::size_t ${QZDL_EMBED_SYMBOL}Size = sizeof(${QZDL_EMBED_SYMBOL});

}
")
