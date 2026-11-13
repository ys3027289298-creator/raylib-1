/**********************************************************************************************
*
*   rdraco - Thin C wrapper around the Google Draco mesh decoder (C++ API)
*
*   This header exposes a minimal C API used by rmodels.c to decode glTF primitives
*   compressed with the KHR_draco_mesh_compression extension.
*
*   The implementation (rdraco.cpp) is only compiled when SUPPORT_DRACO is defined
*   and the Draco library (https://github.com/google/draco) is available.
*
*   CONFIGURATION:
*       SUPPORT_DRACO
*           Enables Draco decoding support, requires the Draco library (C++)
*
*   LICENSE: zlib/libpng (same as raylib)
*
**********************************************************************************************/

#ifndef RDRACO_H
#define RDRACO_H

#include <stddef.h>

#if defined(__cplusplus)
extern "C" {
#endif

// Component data types of a decoded Draco attribute
// NOTE: Values mirror draco::DataType, mapped to glTF component types by the caller
typedef enum {
    RDRACO_DT_INVALID = 0,
    RDRACO_DT_INT8,
    RDRACO_DT_UINT8,
    RDRACO_DT_INT16,
    RDRACO_DT_UINT16,
    RDRACO_DT_INT32,
    RDRACO_DT_UINT32,
    RDRACO_DT_INT64,
    RDRACO_DT_UINT64,
    RDRACO_DT_FLOAT32,
    RDRACO_DT_FLOAT64,
    RDRACO_DT_BOOL
} rdracoDataType;

// Opaque decoded Draco mesh
typedef struct rdracoMesh rdracoMesh;

// Decode a KHR_draco_mesh_compression bufferView payload
// Returns a decoded mesh handle or NULL on failure (error buffer is filled with a diagnosable message)
rdracoMesh *rdracoDecodeMesh(const void *data, size_t dataSize, char *error, int errorSize);

// Release a decoded mesh and all its internal data
void rdracoReleaseMesh(rdracoMesh *mesh);

// Get the number of points (vertices) of the decoded mesh
int rdracoGetPointCount(const rdracoMesh *mesh);

// Get the number of indices of the decoded mesh (3 per triangle face)
int rdracoGetIndexCount(const rdracoMesh *mesh);

// Get the number of components and the component data type of an attribute, looked up by
// its unique id (as declared in the glTF extension attributes map)
// Returns 1 if the attribute was found, 0 otherwise
int rdracoGetAttributeInfo(const rdracoMesh *mesh, int uniqueId, int *numComponents, int *dataType);

// Copy an attribute data (in its native component type, tightly packed, one entry per point)
// into the destination buffer, dstSize is the destination buffer size in bytes
// Returns 1 on success, 0 if the attribute is missing or the destination buffer is too small
int rdracoReadAttribute(const rdracoMesh *mesh, int uniqueId, void *dst, size_t dstSize);

// Copy the triangle indices (unsigned int, 3 per face) into the destination buffer,
// dstCount is the number of indices the destination buffer can hold
// Returns 1 on success, 0 if the destination buffer is too small
int rdracoReadIndices(const rdracoMesh *mesh, unsigned int *dst, size_t dstCount);

#if defined(__cplusplus)
}
#endif

#endif // RDRACO_H
