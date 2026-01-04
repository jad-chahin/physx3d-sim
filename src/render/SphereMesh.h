//
// Created by jchah on 2026-01-04.
//

#ifndef PHYSICS3D_SPHEREMESH_H
#define PHYSICS3D_SPHEREMESH_H

#include <cstdint>
#include <vector>


// Builds a unit sphere mesh.
// outVerts: [px,py,pz,nx,ny,nz] per vertex
// outIdx: triangle indices
void buildSphereMesh(
    int stacks,
    int slices,
    std::vector<float>& outVerts,
    std::vector<std::uint32_t>& outIdx);

#endif //PHYSICS3D_SPHEREMESH_H