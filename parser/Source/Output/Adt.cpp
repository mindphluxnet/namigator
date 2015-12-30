#include <fstream>
#include <sstream>
#include <string>
#include <iomanip>
#include <set>
#include <memory>
#include <cassert>

#include "Input/Adt/AdtFile.hpp"
#include "Output/Adt.hpp"
#include "Output/Continent.hpp"
#include "LinearAlgebra.hpp"
#include "parser.hpp"
#include "Directory.hpp"

namespace parser
{
    inline bool Adt::IsRendered(unsigned char mask[], int x, int y)
    {
        return (mask[y] >> x & 1) != 0;
    }

    Adt::Adt(Continent *continent, int x, int y)
        : X(x), Y(y),
          MaxX((32.f-(float)y)*(533.f+(1.f/3.f))), MinX(MaxX - (533.f + (1.f/3.f))),
          MaxY((32.f-(float)x)*(533.f+(1.f/3.f))), MinY(MaxY - (533.f + (1.f/3.f))), m_continent(continent)
    {
        std::stringstream ss;

        ss << "World\\Maps\\" << continent->Name << "\\" << continent->Name << "_" << x << "_" << y << ".adt";

        // parsing
        parser_input::AdtFile adt(ss.str());

        // Process all data into triangles/indices
        // Terrain

        for (int chunkY = 0; chunkY < 16; ++chunkY)
        {
            for (int chunkX = 0; chunkX < 16; ++chunkX)
            {
                auto const mapChunk = adt.m_chunks[chunkY][chunkX].get();

                AdtChunk *const chunk = new AdtChunk();

                int vertCount = 0;

                chunk->m_terrainVertices = std::move(mapChunk->Positions);
                chunk->m_surfaceNormals = std::move(mapChunk->Normals);

                memcpy(chunk->m_holeMap, mapChunk->HoleMap, sizeof(bool)*8*8);

                // build index list to exclude holes (8 * 8 quads, 4 triangles per quad, 3 indices per triangle)
                chunk->m_terrainIndices.reserve(8 * 8 * 4 * 3);

                for (int y = 0; y < 8; ++y)
                {
                    for (int x = 0; x < 8; ++x)
                    {
                        const int currIndex = y * 17 + x;

                        // if this chunk has holes and this quad is one of them, skip it
                        if (mapChunk->HasHoles && mapChunk->HoleMap[y][x])
                            continue;

                        // Upper triangle
                        chunk->m_terrainIndices.push_back(currIndex);
                        chunk->m_terrainIndices.push_back(currIndex + 1);
                        chunk->m_terrainIndices.push_back(currIndex + 9);

                        // Left triangle
                        chunk->m_terrainIndices.push_back(currIndex);
                        chunk->m_terrainIndices.push_back(currIndex + 9);
                        chunk->m_terrainIndices.push_back(currIndex + 17);

                        // Lower triangle
                        chunk->m_terrainIndices.push_back(currIndex + 9);
                        chunk->m_terrainIndices.push_back(currIndex + 18);
                        chunk->m_terrainIndices.push_back(currIndex + 17);

                        // Right triangle
                        chunk->m_terrainIndices.push_back(currIndex + 1);
                        chunk->m_terrainIndices.push_back(currIndex + 18);
                        chunk->m_terrainIndices.push_back(currIndex + 9);
                    }
                }

                m_chunks[chunkY][chunkX].reset(chunk);
            }
        }

        // Water

        if (adt.m_hasMH2O || adt.m_hasMCLQ)
        {
            for (int chunkY = 0; chunkY < 16; ++chunkY)
                for (int chunkX = 0; chunkX < 16; ++chunkX)
                {
                    auto const chunk = m_chunks[chunkY][chunkX].get();
                    auto const mh2oBlock = adt.m_liquidChunk ? adt.m_liquidChunk->Blocks[chunkY][chunkX].get() : nullptr;
                    auto const mclqBlock = adt.m_chunks[chunkY][chunkX]->LiquidChunk.get();

                    // i dont think this is possible.  its only here because im curious if it happens anywhere.
                    assert(!(mh2oBlock && mclqBlock));

                    if (mh2oBlock)
                    {
                        // expand LiquidVertices.  4 verts per square.
                        chunk->m_liquidVertices.reserve(chunk->m_liquidVertices.size() + 4 * mh2oBlock->Data.Width * mh2oBlock->Data.Height);

                        // expand LiquidIndices.  6 indices per square (two triangles, three indices per triangle)
                        chunk->m_liquidIndices.reserve(chunk->m_liquidIndices.size() + 6 * mh2oBlock->Data.Width * mh2oBlock->Data.Height);

                        for (int y = mh2oBlock->Data.YOffset; y < mh2oBlock->Data.YOffset + mh2oBlock->Data.Height; ++y)
                             for (int x = mh2oBlock->Data.XOffset; x < mh2oBlock->Data.XOffset + mh2oBlock->Data.Width; ++x)
                             {
                                 if (!IsRendered(mh2oBlock->RenderMask, x, y))
                                     continue;

                                 int terrainVert = y * 17 + x;

                                 chunk->m_liquidVertices.push_back(Vertex(chunk->m_terrainVertices[terrainVert].X,
                                                                          chunk->m_terrainVertices[terrainVert].Y,
                                                                          mh2oBlock->Heights[y][x]));

                                 terrainVert = y * 17 + (x + 1);

                                 chunk->m_liquidVertices.push_back(Vertex(chunk->m_terrainVertices[terrainVert].X,
                                                                          chunk->m_terrainVertices[terrainVert].Y,
                                                                          mh2oBlock->Heights[y][x]));

                                 terrainVert = (y + 1) * 17 + x;

                                 chunk->m_liquidVertices.push_back(Vertex(chunk->m_terrainVertices[terrainVert].X,
                                                                          chunk->m_terrainVertices[terrainVert].Y,
                                                                          mh2oBlock->Heights[y][x]));

                                 terrainVert = (y + 1) * 17 + (x + 1);

                                 chunk->m_liquidVertices.push_back(Vertex(chunk->m_terrainVertices[terrainVert].X,
                                                                          chunk->m_terrainVertices[terrainVert].Y,
                                                                          mh2oBlock->Heights[y][x]));

                                 chunk->m_liquidIndices.push_back(chunk->m_liquidVertices.size() - 4);
                                 chunk->m_liquidIndices.push_back(chunk->m_liquidVertices.size() - 2);
                                 chunk->m_liquidIndices.push_back(chunk->m_liquidVertices.size() - 3);

                                 chunk->m_liquidIndices.push_back(chunk->m_liquidVertices.size() - 2);
                                 chunk->m_liquidIndices.push_back(chunk->m_liquidVertices.size() - 1);
                                 chunk->m_liquidIndices.push_back(chunk->m_liquidVertices.size() - 3);
                             }
                    }

                    if (mclqBlock)
                    {
                        // four vertices per square, 8x8 squares (max)
                        chunk->m_liquidVertices.reserve(chunk->m_liquidVertices.size() + 4 * 8 * 8);

                        // six indices (two triangles) per square
                        chunk->m_liquidIndices.reserve(chunk->m_liquidIndices.size() + 6 * 8 * 8);

                        for (int y = 0; y < 8; ++y)
                            for (int x = 0; x < 8; ++x)
                            {
                                if (mclqBlock->RenderMap[y][x] == 0xF)
                                    continue;

                                // XXX FIXME - this is here because it may be that this bit is what actually controls rendering.  trap with assert to debug
                                assert(mclqBlock->RenderMap[y][x] != 8);

                                int terrainVert = y * 17 + x;
                                chunk->m_liquidVertices.push_back({ chunk->m_terrainVertices[terrainVert].X, chunk->m_terrainVertices[terrainVert].Y, mclqBlock->Heights[y][x] });

                                terrainVert = y * 17 + (x + 1);
                                chunk->m_liquidVertices.push_back({ chunk->m_terrainVertices[terrainVert].X, chunk->m_terrainVertices[terrainVert].Y, mclqBlock->Heights[y][x] });

                                terrainVert = (y + 1) * 17 + x;
                                chunk->m_liquidVertices.push_back({ chunk->m_terrainVertices[terrainVert].X, chunk->m_terrainVertices[terrainVert].Y, mclqBlock->Heights[y][x] });

                                terrainVert = (y + 1) * 17 + (x + 1);
                                chunk->m_liquidVertices.push_back({ chunk->m_terrainVertices[terrainVert].X, chunk->m_terrainVertices[terrainVert].Y, mclqBlock->Heights[y][x] });

                                chunk->m_liquidIndices.push_back(chunk->m_liquidVertices.size() - 4);
                                chunk->m_liquidIndices.push_back(chunk->m_liquidVertices.size() - 2);
                                chunk->m_liquidIndices.push_back(chunk->m_liquidVertices.size() - 3);

                                chunk->m_liquidIndices.push_back(chunk->m_liquidVertices.size() - 2);
                                chunk->m_liquidIndices.push_back(chunk->m_liquidVertices.size() - 1);
                                chunk->m_liquidIndices.push_back(chunk->m_liquidVertices.size() - 3);
                            }
                    }
                }
        }

        // WMOs

        if (adt.m_wmoChunk)
        {
            std::vector<std::unique_ptr<parser_input::WmoRootFile>> wmoFiles;
            wmoFiles.reserve(adt.m_wmoChunk->Wmos.size());

            for (unsigned int wmoFile = 0; wmoFile < adt.m_wmoChunk->Wmos.size(); ++wmoFile)
            {
                std::string wmoName = adt.m_wmoNames[adt.m_wmoChunk->Wmos[wmoFile].NameId];
                wmoFiles.push_back(std::unique_ptr<parser_input::WmoRootFile>(new parser_input::WmoRootFile(wmoName, &adt.m_wmoChunk->Wmos[wmoFile])));
            }

            for (int chunkY = 0; chunkY < 16; ++chunkY)
                for (int chunkX = 0; chunkX < 16; ++chunkX)
                {
                    AdtChunk *chunk = m_chunks[chunkY][chunkX].get();

                    BoundingBox chunkBox(chunk->m_terrainVertices[144].X, chunk->m_terrainVertices[144].Y,
                        chunk->m_terrainVertices[0].X, chunk->m_terrainVertices[0].Y);

                    // iterate over each wmo referenced in the adt.  see if it belongs in this chunk and has not been placed yet
                    for (unsigned int w = 0; w < adt.m_wmoChunk->Wmos.size(); ++w)
                    {
                        const unsigned int uniqueId = adt.m_wmoChunk->Wmos[w].UniqueId;
                        bool landsOnChunk = false;

                        // save time by checking overlap with the boundary of the wmo
                        if (!chunkBox.Intersects(wmoFiles[w]->Bounds))
                        {
#ifdef DEBUG
                            if (landsOnChunk)
                                THROW("ADT object file says WMO lands on this chunk, but bounding box disagrees");
#endif
                            continue;
                        }

                        // XXX - this can be made faster by flagging which chunks are hit as the vertices are transformed
                        for (unsigned int i = 0; i < wmoFiles[w]->Vertices.size(); ++i)
                        {
                            if (chunkBox.Contains(wmoFiles[w]->Vertices[i]))
                            {
                                landsOnChunk = true;
                                break;
                            }
                        }

                        if (!landsOnChunk)
                            continue;

                        // this wmo lands on the current chunk.  add it to the chunk's list of unique wmo ids.
                        chunk->m_wmos.insert(uniqueId);

                        // if this is the first time loading this wmo instance for this continent, update the continent
                        if (continent->HasWmo(uniqueId))
                            continue;

                        continent->InsertWmo(uniqueId, new Wmo(wmoFiles[w]->Vertices, wmoFiles[w]->Indices,
                            wmoFiles[w]->LiquidVertices, wmoFiles[w]->LiquidIndices,
                            wmoFiles[w]->DoodadVertices, wmoFiles[w]->DoodadIndices,
                            wmoFiles[w]->Bounds.MinCorner.Z, wmoFiles[w]->Bounds.MaxCorner.Z));
                    }
                }
        }

        // Doodads

        if (adt.m_doodadChunk)
        {
            std::vector<std::unique_ptr<parser_input::Doodad>> doodads;
            doodads.reserve(adt.m_doodadChunk->Doodads.size());

            for (unsigned int doodadFile = 0; doodadFile < adt.m_doodadChunk->Doodads.size(); ++doodadFile)
            {
                std::string modelName = adt.m_doodadNames[adt.m_doodadChunk->Doodads[doodadFile].NameId];
                doodads.push_back(std::unique_ptr<parser_input::Doodad>(new parser_input::Doodad(modelName, adt.m_doodadChunk->Doodads[doodadFile])));
            }

            for (int chunkY = 0; chunkY < 16; ++chunkY)
                for (int chunkX = 0; chunkX < 16; ++chunkX)
                {
                    //MCNK *mapChunk = adt.m_chunks[chunkY][chunkX].get();
                    auto chunk = m_chunks[chunkY][chunkX].get();

                    const Vector2 upperLeftCorner(chunk->m_terrainVertices[0].X, chunk->m_terrainVertices[0].Y),
                                  lowerRightCorner(chunk->m_terrainVertices[144].X, chunk->m_terrainVertices[144].Y);

                    for (unsigned int d = 0; d < adt.m_doodadChunk->Doodads.size(); ++d)
                    {
                        const unsigned int uniqueId = adt.m_doodadChunk->Doodads[d].UniqueId;

                        bool landsOnChunk = false;

                        for (unsigned int i = 0; i < doodads[d]->Vertices.size(); ++i)
                        {
                            if (doodads[d]->Vertices[i].X > upperLeftCorner.X ||
                                doodads[d]->Vertices[i].X < lowerRightCorner.X ||
                                doodads[d]->Vertices[i].Y > upperLeftCorner.Y ||
                                doodads[d]->Vertices[i].Y < lowerRightCorner.Y)
                                continue;

                            landsOnChunk = true;
                            break;
                        }

                        if (!landsOnChunk)
                            continue;

                        chunk->m_doodads.insert(uniqueId);

                        if (continent->HasDoodad(uniqueId))
                            continue;

                        continent->InsertDoodad(uniqueId, new Doodad(doodads[d]->Vertices, doodads[d]->Indices, doodads[d]->MinZ, doodads[d]->MaxZ));
                    }
                }
        }
    }

    void Adt::WriteObjFile() const
    {
        std::stringstream ss;

        ss << m_continent->Name << "_" << X  << "_" << Y << ".obj";

        std::ofstream out(ss.str());

        unsigned int indexOffset = 1;
        std::set<unsigned int> dumpedWmos;
        std::set<unsigned int> dumpedDoodads;

        for (int y = 0; y < 16; ++y)
            for (int x = 0; x < 16; ++x)
            {
                auto chunk = m_chunks[y][x].get();

                // terrain vertices
                out << "# terrain vertices (" << chunk->m_terrainVertices.size() << ")" << std::endl;
                for (unsigned int i = 0; i < chunk->m_terrainVertices.size(); ++i)
                    out << "v " << -chunk->m_terrainVertices[i].Y
                        << " "  <<  chunk->m_terrainVertices[i].Z
                        << " "  << -chunk->m_terrainVertices[i].X << std::endl;

                // terrain indices
                out << "# terrain indices (" << chunk->m_terrainIndices.size() << ")" << std::endl;
                for (unsigned int i = 0; i < chunk->m_terrainIndices.size(); i += 3)
                    out << "f " << indexOffset + chunk->m_terrainIndices[i]
                        << " "  << indexOffset + chunk->m_terrainIndices[i+1]
                        << " "  << indexOffset + chunk->m_terrainIndices[i+2] << std::endl;

                indexOffset += chunk->m_terrainVertices.size();

                // water vertices
                out << "# water vertices (" << chunk->m_liquidVertices.size() << ")" << std::endl;
                for (unsigned int i = 0; i < chunk->m_liquidVertices.size(); ++i)
                    out << "v " << -chunk->m_liquidVertices[i].Y
                        << " "  <<  chunk->m_liquidVertices[i].Z
                        << " "  << -chunk->m_liquidVertices[i].X << std::endl;

                // water indices
                out << "# water indices (" << chunk->m_liquidIndices.size() << ")" << std::endl;
                for (unsigned int i = 0; i < chunk->m_liquidIndices.size(); i += 3)
                    out << "f " << indexOffset + chunk->m_liquidIndices[i]   << " "
                                << indexOffset + chunk->m_liquidIndices[i+1] << " "
                                << indexOffset + chunk->m_liquidIndices[i+2] << std::endl;

                indexOffset += chunk->m_liquidVertices.size();

                // wmo vertices and indices
                out << "# wmo vertices / indices (including liquids and doodads)" << std::endl;
                for (auto id = chunk->m_wmos.begin(); id != chunk->m_wmos.end(); ++id)
                {
                    // if this wmo has already been dumped as part of another chunk, skip it
                    if (dumpedWmos.find(*id) != dumpedWmos.end())
                        continue;

                    dumpedWmos.insert(*id);

                    Wmo *wmo = m_continent->GetWmo(*id);

                    for (unsigned int i = 0; i < wmo->Vertices.size(); ++i)
                        out << "v " << -wmo->Vertices[i].Y
                            << " "  <<  wmo->Vertices[i].Z
                            << " "  << -wmo->Vertices[i].X << std::endl;

                    for (unsigned int i = 0; i < wmo->Indices.size(); i+=3)
                        out << "f " << indexOffset + wmo->Indices[i]   << " " 
                                    << indexOffset + wmo->Indices[i+1] << " "
                                    << indexOffset + wmo->Indices[i+2] << std::endl;

                    indexOffset += wmo->Vertices.size();

                    for (unsigned int i = 0; i < wmo->LiquidVertices.size(); ++i)
                        out << "v " << -wmo->LiquidVertices[i].Y
                            << " "  <<  wmo->LiquidVertices[i].Z
                            << " "  << -wmo->LiquidVertices[i].X << std::endl;

                    for (unsigned int i = 0; i < wmo->LiquidIndices.size(); i+=3)
                        out << "f " << indexOffset + wmo->LiquidIndices[i]   << " " 
                                    << indexOffset + wmo->LiquidIndices[i+1] << " "
                                    << indexOffset + wmo->LiquidIndices[i+2] << std::endl;

                    indexOffset += wmo->LiquidVertices.size();

                    for (unsigned int i = 0; i < wmo->DoodadVertices.size(); ++i)
                        out << "v " << -wmo->DoodadVertices[i].Y
                            << " "  <<  wmo->DoodadVertices[i].Z
                            << " "  << -wmo->DoodadVertices[i].X << std::endl;

                    for (unsigned int i = 0; i < wmo->DoodadIndices.size(); i+=3)
                        out << "f " << indexOffset + wmo->DoodadIndices[i]   << " " 
                                    << indexOffset + wmo->DoodadIndices[i+1] << " "
                                    << indexOffset + wmo->DoodadIndices[i+2] << std::endl;

                    indexOffset += wmo->DoodadVertices.size();
                }

                // doodad vertices
                out << "# doodad vertices" << std::endl;
                for (auto id = chunk->m_doodads.begin(); id != chunk->m_doodads.end(); ++id)
                {
                    // if this wmo has already been dumped as part of another chunk, skip it
                    if (dumpedDoodads.find(*id) != dumpedDoodads.end())
                        continue;

                    dumpedDoodads.insert(*id);

                    Doodad *doodad = m_continent->GetDoodad(*id);

                    for (unsigned int i = 0; i < doodad->Vertices.size(); ++i)
                        out << "v " << -doodad->Vertices[i].Y
                            << " "  <<  doodad->Vertices[i].Z
                            << " "  << -doodad->Vertices[i].X << std::endl;

                    for (unsigned int i = 0; i < doodad->Indices.size(); i+=3)
                        out << "f " << indexOffset + doodad->Indices[i]   << " " 
                                    << indexOffset + doodad->Indices[i+1] << " "
                                    << indexOffset + doodad->Indices[i+2] << std::endl;

                    indexOffset += doodad->Vertices.size();
                }
            }

        out.close();
    }

    const AdtChunk *Adt::GetChunk(const int chunkX, const int chunkY) const
    {
        return m_chunks[chunkX][chunkY].get();
    }
}