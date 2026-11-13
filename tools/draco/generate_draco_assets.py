#!/usr/bin/env python3
"""
Generate Draco-compressed glTF (GLB) test assets for the raylib glTF loader.

Requires: pip install DracoPy numpy

Generates (into examples/models/resources/models/gltf/draco/):
  - draco_cube_multiattr.glb : indexed cube, POSITION/NORMAL/TANGENT/TEXCOORD_0/COLOR_0,
                               material with base color factor, transformed node
  - draco_skinned.glb        : skinned mesh, POSITION/NORMAL/JOINTS_0/WEIGHTS_0 + skin
  - draco_corrupted.glb      : valid glTF but truncated Draco bitstream (must fail gracefully)
  - draco_undeclared.glb     : valid Draco mesh but extension not declared in extensionsUsed
  - draco_mixed.glb          : one Draco-compressed mesh + one uncompressed mesh in the same file
"""

import json
import os
import struct
import sys

import numpy as np

try:
    import DracoPy
except ImportError:
    print("DracoPy is required: pip install DracoPy", file=sys.stderr)
    sys.exit(1)

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
OUTPUT_DIR = os.path.normpath(os.path.join(
    SCRIPT_DIR, "..", "..", "examples", "models", "resources", "models", "gltf", "draco"))

# Draco geometry attribute types (draco::GeometryAttribute::Type)
DRACO_ATTR_TYPE = {0: "POSITION", 1: "NORMAL", 2: "COLOR_0", 3: "TEXCOORD_0"}

# Unique ids used for generic attributes (kept high to avoid collisions with
# the ids DracoPy assigns to the dedicated attributes)
GENERIC_IDS = {"TANGENT": 100, "JOINTS_0": 101, "WEIGHTS_0": 102}


def encode_mesh(points, faces, normals=None, texcoords=None, colors=None, generic=None):
    """Encode a mesh with DracoPy and return (payload, {gltf_name: unique_id})."""
    payload = DracoPy.encode(
        points, faces,
        normals=normals, tex_coord=texcoords, colors=colors,
        generic_attributes=generic,
        quantization_bits=14, compression_level=7,
    )
    # Recover the unique ids assigned by the encoder
    decoded = DracoPy.decode(payload)
    attributes = {}
    for attr in decoded.attributes:
        if attr["attribute_type"] in DRACO_ATTR_TYPE:
            attributes[DRACO_ATTR_TYPE[attr["attribute_type"]]] = attr["unique_id"]
        else:
            for name, uid in (generic or {}).items():
                pass
            # Generic attributes keep the unique id we assigned
            for name, uid in GENERIC_IDS.items():
                if attr["unique_id"] == uid:
                    attributes[name] = uid
    return payload, attributes


def build_glb(json_dict, bin_chunks):
    """Assemble a GLB container from a JSON dict and a list of binary chunks."""
    bin_data = b""
    for chunk in bin_chunks:
        bin_data += chunk
        while len(bin_data) % 4 != 0:
            bin_data += b"\x00"

    json_data = json.dumps(json_dict, separators=(",", ":")).encode("utf-8")
    while len(json_data) % 4 != 0:
        json_data += b" "

    total_length = 12 + 8 + len(json_data) + 8 + len(bin_data)
    glb = struct.pack("<III", 0x46546C67, 2, total_length)
    glb += struct.pack("<II", len(json_data), 0x4E4F534A) + json_data
    glb += struct.pack("<II", len(bin_data), 0x004E4942) + bin_data
    return glb


def cube_geometry():
    """Unit cube: 24 vertices (per-face normals/tangents/uvs), 36 indices."""
    faces_def = [
        # normal, tangent, 4 corners
        ((0, 0, 1), (1, 0, 0), [(-1, -1, 1), (1, -1, 1), (1, 1, 1), (-1, 1, 1)]),
        ((0, 0, -1), (-1, 0, 0), [(1, -1, -1), (-1, -1, -1), (-1, 1, -1), (1, 1, -1)]),
        ((1, 0, 0), (0, 0, -1), [(1, -1, 1), (1, -1, -1), (1, 1, -1), (1, 1, 1)]),
        ((-1, 0, 0), (0, 0, 1), [(-1, -1, -1), (-1, -1, 1), (-1, 1, 1), (-1, 1, -1)]),
        ((0, 1, 0), (1, 0, 0), [(-1, 1, 1), (1, 1, 1), (1, 1, -1), (-1, 1, -1)]),
        ((0, -1, 0), (1, 0, 0), [(-1, -1, -1), (1, -1, -1), (1, -1, 1), (-1, -1, 1)]),
    ]
    points, normals, tangents, uvs, colors, indices = [], [], [], [], [], []
    face_colors = [(255, 0, 0, 255), (0, 255, 0, 255), (0, 0, 255, 255),
                   (255, 255, 0, 255), (255, 0, 255, 255), (0, 255, 255, 255)]
    for fi, (normal, tangent, corners) in enumerate(faces_def):
        base = len(points)
        for ci, corner in enumerate(corners):
            points.append(corner)
            normals.append(normal)
            tangents.append((*tangent, 1.0))
            uvs.append(((ci & 1) * 1.0, ((ci >> 1) & 1) * 1.0))
            colors.append(face_colors[fi])
        indices += [base, base + 1, base + 2, base, base + 2, base + 3]
    return (
        np.array(points, dtype=np.float64),
        np.array(indices, dtype=np.uint32).reshape(-1, 3),
        np.array(normals, dtype=np.float64),
        np.array(tangents, dtype=np.float64),
        np.array(uvs, dtype=np.float64),
        np.array(colors, dtype=np.uint8),
    )


def base_document(name, payload, attributes, declare_extension=True):
    doc = {
        "asset": {"version": "2.0", "generator": "raylib tools/draco/generate_draco_assets.py"},
        "scene": 0,
        "scenes": [{"nodes": [0]}],
        "buffers": [{"byteLength": 0}],  # patched by caller
        "bufferViews": [{"buffer": 0, "byteOffset": 0, "byteLength": len(payload)}],
        "meshes": [{
            "name": name,
            "primitives": [{
                "mode": 4,
                "extensions": {
                    "KHR_draco_mesh_compression": {
                        "bufferView": 0,
                        "attributes": attributes,
                    }
                },
            }],
        }],
    }
    if declare_extension:
        doc["extensionsUsed"] = ["KHR_draco_mesh_compression"]
        doc["extensionsRequired"] = ["KHR_draco_mesh_compression"]
    return doc


def write_asset(filename, doc, bin_chunks):
    path = os.path.join(OUTPUT_DIR, filename)
    with open(path, "wb") as f:
        f.write(build_glb(doc, bin_chunks))
    print("generated:", path)


def generate_cube_multiattr():
    points, faces, normals, tangents, uvs, colors = cube_geometry()
    payload, attributes = encode_mesh(
        points, faces, normals=normals, texcoords=uvs, colors=colors,
        generic={GENERIC_IDS["TANGENT"]: tangents},
    )
    doc = base_document("DracoCubeMultiAttr", payload, attributes)
    doc["buffers"][0]["byteLength"] = len(payload)
    # Node with a non-identity transform to verify transforms are still applied
    doc["nodes"] = [{
        "name": "CubeNode",
        "mesh": 0,
        "translation": [1.0, 2.0, 3.0],
        "rotation": [0.0, 0.0, 0.0, 1.0],
        "scale": [0.5, 0.5, 0.5],
    }]
    doc["materials"] = [{
        "name": "CubeMaterial",
        "pbrMetallicRoughness": {
            "baseColorFactor": [0.8, 0.6, 0.4, 1.0],
            "metallicFactor": 0.2,
            "roughnessFactor": 0.7,
        },
    }]
    doc["meshes"][0]["primitives"][0]["material"] = 0
    write_asset("draco_cube_multiattr.glb", doc, [payload])


def generate_skinned():
    # Two-segment "limb": vertices along Y, skinned to two bones
    points, normals, joints, weights = [], [], [], []
    seg = 8
    for i in range(seg + 1):
        y = 2.0 * i / seg
        for x, z in ((-0.2, 0.0), (0.2, 0.0)):
            points.append((x, y, z))
            normals.append((0.0, 0.0, 1.0))
            w = min(1.0, max(0.0, (y - 0.5)))
            joints.append((0, 1, 0, 0))
            weights.append((1.0 - w, w, 0.0, 0.0))
    faces = []
    for i in range(seg):
        r0, r1 = 2 * i, 2 * (i + 1)
        faces += [[r0, r0 + 1, r1 + 1], [r0, r1 + 1, r1]]
    points = np.array(points, dtype=np.float64)
    faces = np.array(faces, dtype=np.uint32)
    joints = np.array(joints, dtype=np.uint8)
    weights = np.array(weights, dtype=np.float64)
    normals = np.array(normals, dtype=np.float64)

    payload, attributes = encode_mesh(
        points, faces, normals=normals,
        generic={GENERIC_IDS["JOINTS_0"]: joints, GENERIC_IDS["WEIGHTS_0"]: weights},
    )

    # Inverse bind matrices: two identity matrices, stored uncompressed after the Draco payload
    ibm = np.zeros((2, 4, 4), dtype=np.float32)
    ibm[:, 0, 0] = ibm[:, 1, 1] = ibm[:, 2, 2] = ibm[:, 3, 3] = 1.0
    ibm_data = ibm.tobytes()
    ibm_offset = len(payload)
    while ibm_offset % 4 != 0:
        ibm_offset += 1
    padding = b"\x00" * (ibm_offset - len(payload))

    doc = base_document("DracoSkinned", payload, attributes)
    doc["buffers"][0]["byteLength"] = ibm_offset + len(ibm_data)
    doc["bufferViews"].append({"buffer": 0, "byteOffset": ibm_offset, "byteLength": len(ibm_data)})
    doc["accessors"] = [{
        "bufferView": 1, "componentType": 5126, "count": 2, "type": "MAT4",
    }]
    doc["skins"] = [{"joints": [1, 2], "inverseBindMatrices": 0, "skeleton": 1}]
    doc["nodes"] = [
        {"name": "MeshNode", "mesh": 0, "skin": 0},
        {"name": "BoneRoot", "children": [2]},
        {"name": "BoneTip", "translation": [0.0, 1.0, 0.0]},
    ]
    doc["scenes"][0]["nodes"] = [0, 1]
    write_asset("draco_skinned.glb", doc, [payload + padding + ibm_data])


def generate_corrupted():
    points, faces, normals, tangents, uvs, colors = cube_geometry()
    payload, attributes = encode_mesh(points, faces, normals=normals, texcoords=uvs)
    # Truncate the Draco bitstream: the decoder must report an error, not crash
    corrupted = payload[:max(4, len(payload) // 3)]
    doc = base_document("DracoCorrupted", corrupted, attributes)
    doc["buffers"][0]["byteLength"] = len(corrupted)
    doc["nodes"] = [{"name": "CorruptedNode", "mesh": 0}]
    write_asset("draco_corrupted.glb", doc, [corrupted])


def generate_undeclared():
    points, faces, normals, tangents, uvs, colors = cube_geometry()
    payload, attributes = encode_mesh(points, faces, normals=normals, texcoords=uvs)
    # Valid Draco data but the extension is not declared in extensionsUsed:
    # the loader must warn about the missing declaration and still decode
    doc = base_document("DracoUndeclared", payload, attributes, declare_extension=False)
    doc["buffers"][0]["byteLength"] = len(payload)
    doc["nodes"] = [{"name": "UndeclaredNode", "mesh": 0}]
    write_asset("draco_undeclared.glb", doc, [payload])


def generate_mixed():
    points, faces, normals, tangents, uvs, colors = cube_geometry()
    payload, attributes = encode_mesh(points, faces, normals=normals, texcoords=uvs)

    # Uncompressed triangle: positions stored directly after the Draco payload
    tri = np.array([[0, 0, 0], [1, 0, 0], [0, 1, 0]], dtype=np.float32)
    tri_data = tri.tobytes()
    tri_offset = len(payload)
    while tri_offset % 4 != 0:
        tri_offset += 1
    padding = b"\x00" * (tri_offset - len(payload))

    doc = base_document("DracoMixed", payload, attributes)
    doc["buffers"][0]["byteLength"] = tri_offset + len(tri_data)
    doc["bufferViews"].append({"buffer": 0, "byteOffset": tri_offset, "byteLength": len(tri_data)})
    doc["accessors"] = [{
        "bufferView": 1, "componentType": 5126, "count": 3, "type": "VEC3",
        "min": [0.0, 0.0, 0.0], "max": [1.0, 1.0, 0.0],
    }]
    # Second mesh: uncompressed triangle
    doc["meshes"].append({
        "name": "UncompressedTriangle",
        "primitives": [{"mode": 4, "attributes": {"POSITION": 0}}],
    })
    doc["nodes"] = [
        {"name": "DracoNode", "mesh": 0},
        {"name": "PlainNode", "mesh": 1},
    ]
    doc["scenes"][0]["nodes"] = [0, 1]
    write_asset("draco_mixed.glb", doc, [payload + padding + tri_data])


def main():
    os.makedirs(OUTPUT_DIR, exist_ok=True)
    generate_cube_multiattr()
    generate_skinned()
    generate_corrupted()
    generate_undeclared()
    generate_mixed()
    print("done. output dir:", OUTPUT_DIR)


if __name__ == "__main__":
    main()
