"""
Recenter static-mesh pivots to bottom-center for Natural_Pack_01.

Background
----------
The pack's FBX was exported with every model laid out at its own spot in a
shared scene. Imported with combine_meshes=False, each mesh baked its scene
XY layout into the vertices, so every asset's pivot sits at the FBX scene
origin while the geometry floats off at some (X, Y). Z is fine: every model
was built up from ground 0, so the bottom is already ~0.

This script translates each mesh's geometry by (-centerX, -centerY, 0) so the
XY center lands on the origin. Combined with the already-correct Z, the pivot
ends up at the bottom-center of the geometry. All LODs are shifted by the same
LOD0-derived offset so they stay aligned.

Requirements
------------
- Enable the "Geometry Script" plugin (Edit > Plugins > search "Geometry
  Script"), then restart the editor. The script checks for it and aborts
  cleanly if missing.
- Run from the editor: Tools > Execute Python Script... (pick this file),
  or paste into the Output Log's Python console (Cmd line set to "Python").

Notes
-----
- Moving vertices does NOT move any pre-existing *simple* collision shapes.
  Low-poly packs like this usually have none (complex-as-simple), but if a
  mesh has box/convex collision it will be out of place. Set
  REGEN_CONVEX_COLLISION = True below to rebuild a convex hull after moving.
- Idempotent-ish: re-running re-centers to the current center, so running
  twice on an already-centered mesh is a no-op (offset ~0).
"""

import unreal

FOLDER = "/Game/Environments/Naturals/Natural_Pack_01"
REGEN_CONVEX_COLLISION = False  # set True only if meshes have simple collision
EPS = 0.01  # skip translation below this magnitude (already centered)


def list_static_meshes(folder):
    paths = unreal.EditorAssetLibrary.list_assets(folder, recursive=False,
                                                  include_folder=False)
    meshes = []
    for p in paths:
        obj = unreal.EditorAssetLibrary.load_asset(p)
        if isinstance(obj, unreal.StaticMesh):
            meshes.append((p, obj))
    return meshes


def make_dynamic_mesh():
    # Transient DynamicMesh to hold geometry during the edit.
    # UE5.8 Python has no get_transient_package(); construct directly.
    return unreal.DynamicMesh()


def resolve_static_mesh_lib():
    """UE5.8 moved the static-mesh copy functions off a single well-known
    class. Scan every GeometryScript_* class for the two methods we need and
    return the class that has both."""
    for name in dir(unreal):
        if not name.startswith("GeometryScript"):
            continue
        cls = getattr(unreal, name)
        if (hasattr(cls, "copy_mesh_from_static_mesh")
                and hasattr(cls, "copy_mesh_to_static_mesh")):
            unreal.log("[recenter] Using '{}' for static-mesh copy.".format(name))
            return cls
    return None


SM_LIB = None  # set in run()


def copy_from_lod(sm, dyn, lod_index):
    opts = unreal.GeometryScriptCopyMeshFromAssetOptions()
    read_lod = unreal.GeometryScriptMeshReadLOD()
    read_lod.lod_type = unreal.GeometryScriptLODType.SOURCE_MODEL
    read_lod.lod_index = lod_index
    dyn, _outcome = SM_LIB.copy_mesh_from_static_mesh(sm, dyn, opts, read_lod)
    return dyn


def copy_to_lod(sm, dyn, lod_index):
    opts = unreal.GeometryScriptCopyMeshToAssetOptions()
    opts.enable_recompute_normals = False
    opts.enable_recompute_tangents = False
    opts.replace_materials = False  # keep existing GRADIENT material assignments
    opts.emit_transaction = True    # bEmitTransaction is an options field, not a fn arg
    write_lod = unreal.GeometryScriptMeshWriteLOD()
    write_lod.lod_index = lod_index
    SM_LIB.copy_mesh_to_static_mesh(dyn, sm, opts, write_lod)


def bottom_center_offset(dyn):
    box = unreal.GeometryScript_MeshQueries.get_mesh_bounding_box(dyn)
    cx = (box.min.x + box.max.x) * 0.5
    cy = (box.min.y + box.max.y) * 0.5
    return unreal.Vector(-cx, -cy, 0.0)


def run():
    global SM_LIB
    SM_LIB = resolve_static_mesh_lib()
    if SM_LIB is None:
        unreal.log_error(
            "[recenter] Could not find a GeometryScript class exposing "
            "copy_mesh_from_static_mesh/copy_mesh_to_static_mesh. Is the "
            "Geometry Script plugin fully enabled?")
        return

    for req in ["GeometryScript_MeshTransforms", "GeometryScript_MeshQueries",
                "DynamicMesh"]:
        if not hasattr(unreal, req):
            unreal.log_error("[recenter] Missing '{}'.".format(req))
            return

    meshes = list_static_meshes(FOLDER)
    unreal.log("[recenter] Found {} static meshes in {}".format(len(meshes), FOLDER))

    moved, skipped = [], []
    for path, sm in meshes:
        # We read/write SourceModel LODs, so count SourceModels (not render
        # LODs, which GetNumLODs returns and may differ).
        if hasattr(sm, "get_num_source_models"):
            num_lods = max(1, sm.get_num_source_models())
        else:
            num_lods = 1

        # Compute offset from LOD0, apply the SAME offset to every LOD.
        dyn0 = copy_from_lod(sm, make_dynamic_mesh(), 0)
        offset = bottom_center_offset(dyn0)

        if offset.length() < EPS:
            skipped.append(path)
            continue

        for lod in range(num_lods):
            dyn = dyn0 if lod == 0 else copy_from_lod(sm, make_dynamic_mesh(), lod)
            unreal.GeometryScript_MeshTransforms.translate_mesh(dyn, offset)
            copy_to_lod(sm, dyn, lod)

        if REGEN_CONVEX_COLLISION:
            unreal.EditorStaticMeshLibrary.set_convex_decomposition_collisions(
                sm, 4, 16, 100000)

        unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False)
        moved.append((path, round(offset.x, 1), round(offset.y, 1)))

    unreal.log("[recenter] Done. moved={} skipped(already centered)={}".format(
        len(moved), len(skipped)))
    for path, dx, dy in moved:
        unreal.log("  moved {}  by ({}, {}, 0)".format(path.split('/')[-1], dx, dy))
    return {"moved": len(moved), "skipped": len(skipped)}


run()
