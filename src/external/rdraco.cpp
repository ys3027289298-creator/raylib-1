/**********************************************************************************************
*
*   rdraco - Thin C wrapper around the Google Draco mesh decoder (C++ API)
*
*   Implementation of the C API declared in rdraco.h, bridging raylib (C99) to the
*   Google Draco decoder library (C++): https://github.com/google/draco
*
*   This file is only compiled when SUPPORT_DRACO is defined.
*
*   LICENSE: zlib/libpng (same as raylib)
*
**********************************************************************************************/

#include "rdraco.h"

#include "draco/compression/decode.h"
#include "draco/mesh/mesh.h"
#include "draco/point_cloud/point_cloud.h"
#include "draco/attributes/point_attribute.h"

#include <cstdio>
#include <cstring>
#include <memory>

struct rdracoMesh {
    std::unique_ptr<draco::Mesh> mesh;
};

// Find a point attribute by its unique id (as declared in the glTF extension attributes map)
static const draco::PointAttribute *FindAttributeByUniqueId(const draco::PointCloud &pointCloud, int uniqueId)
{
    for (int i = 0; i < pointCloud.num_attributes(); i++)
    {
        const draco::PointAttribute *attribute = pointCloud.attribute(i);
        if ((attribute != nullptr) && (attribute->unique_id() == uniqueId)) return attribute;
    }

    return nullptr;
}

rdracoMesh *rdracoDecodeMesh(const void *data, size_t dataSize, char *error, int errorSize)
{
    if (error && (errorSize > 0)) error[0] = '\0';

    if ((data == nullptr) || (dataSize == 0))
    {
        if (error && (errorSize > 0)) snprintf(error, (size_t)errorSize, "empty Draco data buffer");
        return nullptr;
    }

    draco::Decoder decoder;
    draco::DecoderBuffer buffer;
    buffer.Init(reinterpret_cast<const char *>(data), dataSize);

    // Detect the encoded geometry type first to give a diagnosable error on non-mesh data
    draco::StatusOr<draco::EncodedGeometryType> typeOr = draco::Decoder::GetEncodedGeometryType(&buffer);
    if (!typeOr.ok())
    {
        if (error && (errorSize > 0)) snprintf(error, (size_t)errorSize, "invalid Draco bitstream: %s", typeOr.status().error_msg());
        return nullptr;
    }

    if (typeOr.value() != draco::TRIANGULAR_MESH)
    {
        if (error && (errorSize > 0)) snprintf(error, (size_t)errorSize, "Draco bitstream does not contain a triangular mesh");
        return nullptr;
    }

    draco::StatusOr<std::unique_ptr<draco::Mesh>> meshOr = decoder.DecodeMeshFromBuffer(&buffer);
    if (!meshOr.ok() || (meshOr.value() == nullptr))
    {
        if (error && (errorSize > 0)) snprintf(error, (size_t)errorSize, "failed to decode Draco mesh: %s", meshOr.ok()? "empty mesh" : meshOr.status().error_msg());
        return nullptr;
    }

    rdracoMesh *decoded = new (std::nothrow) rdracoMesh();
    if (decoded == nullptr)
    {
        if (error && (errorSize > 0)) snprintf(error, (size_t)errorSize, "out of memory allocating Draco mesh");
        return nullptr;
    }

    decoded->mesh = std::move(meshOr).value();
    return decoded;
}

void rdracoReleaseMesh(rdracoMesh *mesh)
{
    delete mesh;
}

int rdracoGetPointCount(const rdracoMesh *mesh)
{
    if ((mesh == nullptr) || (mesh->mesh == nullptr)) return 0;
    return (int)mesh->mesh->num_points();
}

int rdracoGetIndexCount(const rdracoMesh *mesh)
{
    if ((mesh == nullptr) || (mesh->mesh == nullptr)) return 0;
    return (int)mesh->mesh->num_faces()*3;
}

int rdracoGetAttributeInfo(const rdracoMesh *mesh, int uniqueId, int *numComponents, int *dataType)
{
    if ((mesh == nullptr) || (mesh->mesh == nullptr)) return 0;

    const draco::PointAttribute *attribute = FindAttributeByUniqueId(*mesh->mesh, uniqueId);
    if (attribute == nullptr) return 0;

    if (numComponents != nullptr) *numComponents = (int)attribute->num_components();
    if (dataType != nullptr) *dataType = (int)attribute->data_type();

    return 1;
}

int rdracoReadAttribute(const rdracoMesh *mesh, int uniqueId, void *dst, size_t dstSize)
{
    if ((mesh == nullptr) || (mesh->mesh == nullptr) || (dst == nullptr)) return 0;

    const draco::PointAttribute *attribute = FindAttributeByUniqueId(*mesh->mesh, uniqueId);
    if (attribute == nullptr) return 0;

    const int numComponents = (int)attribute->num_components();
    const int componentSize = (int)draco::DataTypeLength(attribute->data_type());
    const int numPoints = (int)mesh->mesh->num_points();
    const size_t entrySize = (size_t)numComponents*(size_t)componentSize;
    const size_t requiredSize = (size_t)numPoints*entrySize;

    // NOTE: Refuse to write past the destination buffer
    if (dstSize < requiredSize) return 0;

    unsigned char *out = reinterpret_cast<unsigned char *>(dst);
    for (int i = 0; i < numPoints; i++)
    {
        attribute->GetMappedValue(draco::PointIndex(i), out + (size_t)i*entrySize);
    }

    return 1;
}

int rdracoReadIndices(const rdracoMesh *mesh, unsigned int *dst, size_t dstCount)
{
    if ((mesh == nullptr) || (mesh->mesh == nullptr) || (dst == nullptr)) return 0;

    const int numFaces = (int)mesh->mesh->num_faces();

    // NOTE: Refuse to write past the destination buffer
    if (dstCount < (size_t)numFaces*3) return 0;

    for (int i = 0; i < numFaces; i++)
    {
        const draco::Mesh::Face &face = mesh->mesh->face(draco::FaceIndex(i));
        dst[3*i + 0] = (unsigned int)face[0].value();
        dst[3*i + 1] = (unsigned int)face[1].value();
        dst[3*i + 2] = (unsigned int)face[2].value();
    }

    return 1;
}
