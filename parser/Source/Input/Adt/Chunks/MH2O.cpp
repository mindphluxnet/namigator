#include "BinaryStream.hpp"
#include "Input/ADT/Chunks/MH2O.hpp"

#include <memory>
#include <cassert>
#include <vector>

namespace parser_input
{
    MH2O::MH2O(long position, utility::BinaryStream *reader) : AdtChunk(position, reader)
    {
        const long offsetFrom = Position + 8;

        for (int y = 0; y < 16; ++y)
            for (int x = 0; x < 16; ++x)
            {
                reader->SetPosition(offsetFrom + sizeof(MH2OHeader)*(y * 16 + x));
                
                MH2OHeader header;

                reader->ReadBytes(&header, sizeof(header));

                if (header.LayerCount <= 0)
                    continue;

                // XXX FIXME this isn't actually always true, but i am leaving it here to help us know when we run into a case of it
                assert(header.LayerCount == 1);

                reader->SetPosition(offsetFrom + header.InstancesOffset);

                for (int layer = 0; layer < header.LayerCount; ++layer)
                {
                    LiquidInstance instance;
                    reader->ReadBytes(&instance, sizeof(instance));

                    assert(!!instance.Width && !!instance.Height);

                    auto newLayer = new LiquidLayer();

                    newLayer->X = x;
                    newLayer->Y = y;

                    memset(newLayer->Render, 0, sizeof(newLayer->Render));
                    memset(newLayer->Heights, 0, sizeof(newLayer->Heights));

                    std::vector<unsigned char> exists(instance.OffsetVertexData - instance.OffsetExistsBitmap);

                    if (header.AttributesOffset && instance.OffsetExistsBitmap)
                    {
                        reader->SetPosition(offsetFrom + instance.OffsetExistsBitmap);
                        reader->ReadBytes(&exists[0], exists.size());
                    }
                    else
                        memset(&exists[0], 0xFF, exists.size());

                    std::vector<float> heightMap((instance.Width + 1)*(instance.Height + 1));
                    reader->SetPosition(offsetFrom + instance.OffsetVertexData);
                    reader->ReadBytes(&heightMap[0], sizeof(float)*heightMap.size());

                    // the depth map can be skipped.  it functions only to assist in shading.
                    reader->Slide(heightMap.size());

                    int currentByte = 0;
                    int currentBit = 0;
                    for (int squareY = 0; squareY < instance.Height; ++squareY)
                        for (int squareX = 0; squareX < instance.Width; ++squareX)
                        {
                            if (currentBit == 8)
                            {
                                currentByte++;
                                currentBit = 0;
                            }

                            if (!((exists[currentByte] >> currentBit++) & 0x01))
                                continue;

                            newLayer->Render[squareY + instance.YOffset][squareX + instance.XOffset] = true;

                            newLayer->Heights[squareY + instance.YOffset + 0][squareX + instance.XOffset + 0] = heightMap[(squareY + 0)*(instance.Width + 1) + squareX + 0];
                            newLayer->Heights[squareY + instance.YOffset + 0][squareX + instance.XOffset + 1] = heightMap[(squareY + 0)*(instance.Width + 1) + squareX + 1];
                            newLayer->Heights[squareY + instance.YOffset + 1][squareX + instance.XOffset + 0] = heightMap[(squareY + 1)*(instance.Width + 1) + squareX + 0];
                            newLayer->Heights[squareY + instance.YOffset + 1][squareX + instance.XOffset + 1] = heightMap[(squareY + 1)*(instance.Width + 1) + squareX + 1];
                        }

                    Layers.push_back(std::unique_ptr<LiquidLayer>(newLayer));
                }
            }
    }
}