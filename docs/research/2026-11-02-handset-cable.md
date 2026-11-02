# Handset cable and handset prop: what exists, what LC does (2026-11-02)

Question (Duke's fourth round): a visible black cable with slack from the holder's hand to the infantry phone
(later the manpack radio) while the phone is held, generic for the manpack handset (A). Duke added: the
vanilla wall phone's handset (`PhoneWall_01_dst_02_blue`) in the holder's hand.

## Vanilla 1.8 (game 1.8.0.13)

- **No rope, cable, winch, tow or sling simulation.** No script class, component or prefab for one; no
  physics rope API (`Physics*.c` has joints and surfaces only). "Rope"/"cable" assets are static meshes
  (cable reels, crane cables, fishing rope props).
- **Script-built meshes exist and are cheap:** `MeshObject.Create(numMeshes, numVerts[], numIndices[],
  materials[], flags)` + `UpdateVerts(mesh, verts[], uvs[])` + `UpdateIndices` + `entity.SetObject(mesh, "")`.
  Vanilla uses it in `SCR_GenericBoxEntity`, `SCR_TerrainMeshEntity` and `SCR_Shape.CreateAreaMesh`
  (editor area borders); `SCR_BaseAreaMeshComponent.EOnFrame` **rebuilds the whole mesh** (Create +
  SetObject) whenever its owner moves, so per-move rebuilds are a vanilla pattern. `MeshObjectFlags` has a
  `DYNAMIC_VERTEXES` hint and `UPDATE_BOUNDBOX`, but no script passes anything but 0 and the enum values are
  undocumented, so LC doesn't rely on updating a live mesh in place.
- **Debug shapes** (`Shape.CreateLines`) are one-pixel unlit lines: not a cable.
- **Material:** `{1C8CA76ED832109C}Assets/Structures/Industrial/Cranes/GantryCrane_01/Data/GantryCrane_cables_EXT.emat`
  is a near-black (0.08 0.09 0.10) rope-normal material. Reused as is (vanilla asset, no new resource).
- **Props in hands:** vanilla's loiter animations put local props in the hand
  (`SCR_LoiterItemData.SpawnPrefabs`): `GetGame().SpawnEntityPrefabLocal(prefab)`, then
  `owner.AddChild(item, owner.GetAnimation().GetBoneIndex("RightHandProp"))` and `SetLocalTransform(offsets)`.
  The prop prefab (`AnimationProp_base.et`) is a bare `GenericEntity` with a `MeshObject`, no physics.
  `LeftHandProp` is a character bone too (`Driving_Instructor_Copilot.et` slots a book on it).
- **Passenger doors** (for the phone's new place): BTR-70 `door_l01` (DoorInfoList 0) and `door_r01` (1),
  both used by every cargo seat (`DoorInfoList {1 0}`, right first) and the driver; contexts at
  ±1.16 m side, 0.9 m up, -0.1 m back. LAV-25 cargo seats use `door_back_left` (2) or `door_back_right`
  (3), the two rear ramp doors (contexts ±0.375 m, 1.26 m up, -2.78 m); `door_side_left` (1) is the
  codriver/turret hatch-side door, not cargo.

## Mods (approach only)

- **RHS Status Quo** (`References/ARMods/from-WeaponModels/RHS-StatusQuo_595F2BF2F44836FB`,
  `RHS_TowCableComponent`): tow cable = 6-DOF spring joint for physics plus a **visual cylinder built with
  `MeshObject.Create`** on a `GetGame().SpawnEntity(GenericEntity)`, vertices in world space, rebuilt on a
  100 ms repeating CallLater when an end moves > 10 cm, material = the gantry crane cable above. Notes worth
  keeping: hold the `Resource` in a field or the engine frees the mesh still in use ("Usage of released
  BaseResourceObject" crash); `EntityEvent.FIXEDFRAME` only ticks reliably on the authority, so visuals ride
  a frame/call-queue tick instead. Not copied (GROUND-RULES 7).
- No fast-rope or other rope mod is installed or in `References/ARMods` (installed Workshop addons checked by
  name: nothing rope/sling/winch).

## What LC does

- `LC_HandsetCable` (`Radio/LC_HandsetCable.c`): a 4-sided tube, 16 segments, 6 mm radius, on a local
  `GenericEntity` placed at the hand end (small vertex coordinates; world-space vertices would be ~1 mm
  precision at Everon's 10 km). The curve is a parabola sagging with the slack (drawn length = straight
  line × 1.15 + 0.5 m, capped at the lease's cable length; sag from the parabolic arc-length formula) and
  clamped 2 cm above the terrain. Rebuilt (Create + SetObject) only when an end moves > 1 cm.
- The handset: own prefab `Prefabs/LC/Props/LC_InfantryPhoneHandset.et` from the vanilla model
  `PhoneWall_01_dst_02.xob` with the blue materials, **no RigidBody** (the vanilla debris prefab carries a
  static model-geometry body, which must not ride on a character), spawned locally and attached to
  `LeftHandProp` the vanilla loiter way. The cable starts at it.
- `LC_HandsetCables`: on every client, every frame, one cable per replicated lease of a cable kind (PHONE
  now; the manpack handset adds BORROWED in `HasCable`), hidden and not updated beyond 100 m of the camera
  or when the holder/vehicle is streamed out. No new replication, no physics.

## Open (needs eyes in play mode)

Tube winding (inside-out or not), the handset's offset/rotation on `LeftHandProp` (all zero for now), how it
looks against a held rifle (no hold animation: the hand keeps its weapon pose), and whether the material
reads black enough.
