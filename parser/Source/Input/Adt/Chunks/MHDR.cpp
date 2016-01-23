#include "utility/Include/Exception.hpp"
#include "Input/ADT/Chunks/MHDR.hpp"

namespace parser
{
namespace input
{
MHDR::MHDR(long position, utility::BinaryStream *reader) : AdtChunk(position, reader)
{
#ifdef DEBUG
    if (Type != AdtChunkType::MHDR)
        THROW("Expected (but did not find) MHDR type");

    if (Size != sizeof(MHDRInfo))
        THROW("MHDR chunk of incorrect size");
#endif

    reader->ReadBytes(&Offsets, sizeof(Offsets));
}
}
}