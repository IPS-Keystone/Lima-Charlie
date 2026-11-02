//------------------------------------------------------------------------------------------------
//! One held handset as seen on this machine: the handset in the holder's hand and a thin dark cable from it
//! to a point on the radio or vehicle, sagging with slack up to the cable's length. Purely local and
//! visual: every machine builds its own from the replicated lease (LC_HandsetCables), nothing of it is
//! replicated, and it has no physics. Generic: it knows nothing of phones or manpacks, only its two ends.
//!
//! The cable is a tube mesh rebuilt with MeshObject.Create whenever an end moves (vanilla rebuilds its
//! area meshes the same way), on a local entity placed at the hand end so vertex coordinates stay small.
class LC_HandsetCable
{
	//! Plain black, fully rough: the mesh has no normals of its own, so a textured or glossy material lights white
	protected static const ResourceName MATERIAL = "{9C3E5A1F7B2D4068}Assets/LC/LC_HandsetCable.emat";
	protected static const float RADIUS = 0.006;
	//! The drawn cable is the straight line plus this share of it plus SLACK_M, up to the cable's length
	protected static const float SLACK_SHARE = 0.15;
	protected static const float SLACK_M = 0.5;
	//! Lowest the cable hangs above the terrain
	protected static const float GROUND_CLEARANCE = 0.02;
	//! Rebuild only when an end moved further than this (m)
	protected static const float REBUILD_MOVE = 0.01;
	//! Tube resolution: SEGMENTS along, SIDES around. The arrays in Rebuild are sized from these.
	protected static const int SEGMENTS = 16;
	protected static const int SIDES = 4;
	//! The handset prop goes on the handset item in the left hand (the radio inspect pose), or on this bone
	//! until the item is there. Its model is centred on GRIP_OFFSET (the item's / bone's space) at
	//! HANDSET_ANGLES; the cord leaves the bottom of the model (lowest Y, centred), where the cable starts.
	protected static const string HAND_BONE = "LeftHandProp";
	protected static const vector GRIP_OFFSET = "0 0.06 0";
	protected static const vector HANDSET_ANGLES = "0 0 0";

	protected RplId m_RadioId;
	protected int m_iPlayerId;
	protected LC_ERadioLeaseKind m_eKind;
	protected ResourceName m_sHandsetPrefab;

	//! Weak: the holder's character, and what the handset prop is attached to (the held item or the character)
	protected IEntity m_Holder;
	protected IEntity m_Parent;
	//! The cord's exit in the handset prop's own space
	protected vector m_vCordExit;
	//! Owned local entities, deleted by Destroy (the handset also goes with its holder)
	protected IEntity m_Handset;
	protected IEntity m_Cable;
	//! Kept, or the engine frees the mesh while the entity still uses it
	protected ref Resource m_Mesh;
	protected vector m_vLastStart;
	protected vector m_vLastEnd;
	protected float m_fLastLength;
	protected bool m_bVisible;

	//------------------------------------------------------------------------------------------------
	void LC_HandsetCable(RplId radioId, int playerId, LC_ERadioLeaseKind kind, ResourceName handsetPrefab)
	{
		m_RadioId = radioId;
		m_iPlayerId = playerId;
		m_eKind = kind;
		m_sHandsetPrefab = handsetPrefab;
	}

	//------------------------------------------------------------------------------------------------
	void ~LC_HandsetCable()
	{
		Destroy();
	}

	//------------------------------------------------------------------------------------------------
	bool Is(RplId radioId, int playerId, LC_ERadioLeaseKind kind)
	{
		return m_RadioId == radioId && m_iPlayerId == playerId && m_eKind == kind;
	}

	//------------------------------------------------------------------------------------------------
	RplId GetRadioId()
	{
		return m_RadioId;
	}

	//------------------------------------------------------------------------------------------------
	int GetPlayerId()
	{
		return m_iPlayerId;
	}

	//------------------------------------------------------------------------------------------------
	LC_ERadioLeaseKind GetKind()
	{
		return m_eKind;
	}

	//------------------------------------------------------------------------------------------------
	//! Draws the cable from the holder's hand to anchor (world), at most length long. Call every frame while shown.
	void Update(notnull IEntity holder, vector anchor, float length)
	{
		IEntity parent = LC_HandsetGadgetComponent.GetHeld(holder);
		if (!parent)
			parent = holder;

		if (holder != m_Holder || parent != m_Parent)
			Attach(holder, parent);

		vector start = GetHandPosition();
		if (!m_Cable)
		{
			m_Cable = GetGame().SpawnEntity(GenericEntity, holder.GetWorld());
			if (!m_Cable)
				return;

			m_Mesh = null;
		}

		SetVisible(true);
		if (m_Mesh && length == m_fLastLength && vector.DistanceSq(start, m_vLastStart) < REBUILD_MOVE * REBUILD_MOVE
			&& vector.DistanceSq(anchor, m_vLastEnd) < REBUILD_MOVE * REBUILD_MOVE)
			return;

		m_vLastStart = start;
		m_vLastEnd = anchor;
		m_fLastLength = length;
		Rebuild(start, anchor, length);
	}

	//------------------------------------------------------------------------------------------------
	//! Out of view or streamed out: keep the entities, draw nothing
	void SetVisible(bool visible)
	{
		if (visible == m_bVisible)
			return;

		m_bVisible = visible;
		SetEntityVisible(m_Cable, visible);
		SetEntityVisible(m_Handset, visible);
	}

	//------------------------------------------------------------------------------------------------
	protected static void SetEntityVisible(IEntity entity, bool visible)
	{
		if (!entity)
			return;

		if (visible)
			entity.SetFlags(EntityFlags.VISIBLE);
		else
			entity.ClearFlags(EntityFlags.VISIBLE);
	}

	//------------------------------------------------------------------------------------------------
	void Destroy()
	{
		if (m_Cable)
			SCR_EntityHelper.DeleteEntityAndChildren(m_Cable);

		if (m_Handset)
			SCR_EntityHelper.DeleteEntityAndChildren(m_Handset);

		m_Cable = null;
		m_Handset = null;
		m_Mesh = null;
		m_Holder = null;
		m_Parent = null;
	}

	//------------------------------------------------------------------------------------------------
	//! Puts the handset prop on the held item, or on the hand bone (a local prop, like vanilla's loiter
	//! animation props), centred on its own bounds so an off-centre model origin doesn't matter
	protected void Attach(IEntity holder, IEntity parent)
	{
		if (m_Handset)
			SCR_EntityHelper.DeleteEntityAndChildren(m_Handset);

		m_Handset = null;
		m_Holder = holder;
		m_Parent = parent;
		m_bVisible = false;
		if (m_sHandsetPrefab.IsEmpty())
			return;

		TNodeId bone = -1;
		if (parent == holder)
		{
			if (!holder.GetAnimation())
				return;

			bone = holder.GetAnimation().GetBoneIndex(HAND_BONE);
			if (bone < 0)
				return;
		}

		Resource resource = Resource.Load(m_sHandsetPrefab);
		if (!resource.IsValid())
			return;

		EntitySpawnParams params = new EntitySpawnParams();
		parent.GetTransform(params.Transform);
		m_Handset = GetGame().SpawnEntityPrefabLocal(resource, holder.GetWorld(), params);
		if (!m_Handset)
			return;

		vector mins;
		vector maxs;
		m_Handset.GetBounds(mins, maxs);
		vector centre = (mins + maxs) * 0.5;
		m_vCordExit = Vector(centre[0], mins[1], centre[2]);

		parent.AddChild(m_Handset, bone);
		vector transform[4];
		Math3D.AnglesToMatrix(HANDSET_ANGLES, transform);
		transform[3] = GRIP_OFFSET - centre.Multiply3(transform);
		m_Handset.SetLocalTransform(transform);
	}

	//------------------------------------------------------------------------------------------------
	//! Where the cord leaves the handset, or the hand bone when there is no handset
	protected vector GetHandPosition()
	{
		vector transform[4];
		if (m_Handset)
		{
			m_Handset.GetWorldTransform(transform);
			return m_vCordExit.Multiply4(transform);
		}

		Animation animation = m_Holder.GetAnimation();
		if (animation)
		{
			TNodeId bone = animation.GetBoneIndex(HAND_BONE);
			vector boneTransform[4];
			if (bone >= 0 && animation.GetBoneMatrix(bone, boneTransform))
			{
				m_Holder.GetWorldTransform(transform);
				return boneTransform[3].Multiply4(transform);
			}
		}

		return m_Holder.GetOrigin() + vector.Up;
	}

	//------------------------------------------------------------------------------------------------
	//! A parabola from start to end that sags with the slack and stays above the terrain, as a SIDES-sided
	//! tube. Vertices are relative to start, where the cable entity is put.
	protected void Rebuild(vector start, vector end, float length)
	{
		vector span = end - start;
		float distance = span.Length();
		float drawn = Math.Min(length, distance * (1 + SLACK_SHARE) + SLACK_M);
		// Parabolic arc length: drawn = distance + 8 sag^2 / (3 distance)
		float sag = 0;
		if (drawn > distance)
			sag = Math.Sqrt(0.375 * distance * (drawn - distance));

		BaseWorld world = m_Cable.GetWorld();
		vector points[17];
		for (int i = 0; i <= SEGMENTS; i++)
		{
			float t = i;
			t /= SEGMENTS;
			vector point = start + span * t;
			point[1] = point[1] - 4 * sag * t * (1 - t);
			float ground = world.GetSurfaceY(point[0], point[2]) + GROUND_CLEARANCE;
			if (point[1] < ground)
				point[1] = ground;

			points[i] = point;
		}

		// (SEGMENTS + 1) * SIDES vertices, SEGMENTS * SIDES * 6 indices
		vector verts[68];
		float uvs[136];
		int indices[384];
		int vertex;
		for (int i = 0; i <= SEGMENTS; i++)
		{
			int previous = i - 1;
			if (previous < 0)
				previous = 0;

			int next = i + 1;
			if (next > SEGMENTS)
				next = SEGMENTS;

			vector tangent = (points[next] - points[previous]).Normalized();
			vector side = tangent * vector.Up;
			if (side.LengthSq() < 0.0001)
				side = tangent * vector.Forward;

			side.Normalize();
			vector up = side * tangent;
			for (int k = 0; k < SIDES; k++)
			{
				float around = k;
				around /= SIDES;
				float angle = Math.PI2 * around;
				verts[vertex] = points[i] - start + (side * Math.Cos(angle) + up * Math.Sin(angle)) * RADIUS;
				uvs[vertex * 2] = around;
				uvs[vertex * 2 + 1] = i * drawn / SEGMENTS;
				vertex++;
			}
		}

		int index;
		for (int i = 0; i < SEGMENTS; i++)
		{
			for (int k = 0; k < SIDES; k++)
			{
				int a = i * SIDES + k;
				int b = i * SIDES + (k + 1) % SIDES;
				int c = a + SIDES;
				int d = b + SIDES;
				indices[index] = a; index++;
				indices[index] = b; index++;
				indices[index] = c; index++;
				indices[index] = b; index++;
				indices[index] = d; index++;
				indices[index] = c; index++;
			}
		}

		int numVertices[] = {vertex};
		int numIndices[] = {index};
		string materials[] = {MATERIAL};
		Resource resource = MeshObject.Create(1, numVertices, numIndices, materials, 0);
		if (!resource || !resource.IsValid())
			return;

		MeshObject mesh = resource.GetResource().ToMeshObject();
		if (!mesh)
			return;

		mesh.UpdateVerts(0, verts, uvs);
		mesh.UpdateIndices(0, indices);
		m_Mesh = resource;

		vector transform[4];
		Math3D.MatrixIdentity4(transform);
		transform[3] = start;
		m_Cable.SetWorldTransform(transform);
		m_Cable.SetObject(mesh, "");
		m_Cable.Update();
	}
}

//------------------------------------------------------------------------------------------------
//! Draws a cable for every held handset this machine can see, from the replicated leases. Owned and ticked
//! by LC_Client. Which lease kinds have a cable, where it ends, how long it is and which handset is in the
//! hand are the override points (GetAnchor and friends): the manpack handset (3.0 plan, A) adds BORROWED.
class LC_HandsetCables
{
	//! Farther than this from the camera, a cable is hidden and not updated
	protected static const float VIEW_DISTANCE_M = 100;

	protected ref array<ref LC_HandsetCable> m_aCables = {};
	//! Keeps the local player's own handset item in hand and inspected
	protected ref LC_HandsetItem m_OwnHandset = new LC_HandsetItem();

	//------------------------------------------------------------------------------------------------
	//! Call every frame. viewer: the camera position.
	void Update(vector viewer)
	{
		LC_RadioLeases leases = LC_RadioLeases.Get();
		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (!leases || !playerManager)
		{
			Clear();
			return;
		}

		Reconcile(leases);
		int localPlayerId = SCR_PlayerController.GetLocalPlayerId();
		int now = System.GetTickCount();
		foreach (LC_HandsetCable cable : m_aCables)
		{
			IEntity holder = playerManager.GetPlayerControlledEntity(cable.GetPlayerId());
			if (holder && cable.GetPlayerId() == localPlayerId)
				m_OwnHandset.Update(holder, now);

			IEntity radio = LC_RadioLeases.FindRadio(cable.GetRadioId());
			if (!holder || !radio || vector.DistanceSq(holder.GetOrigin(), viewer) > VIEW_DISTANCE_M * VIEW_DISTANCE_M)
			{
				cable.SetVisible(false);
				continue;
			}

			cable.Update(holder, GetAnchor(cable.GetKind(), radio), GetLength(cable.GetKind(), radio));
		}
	}

	//------------------------------------------------------------------------------------------------
	void Clear()
	{
		m_aCables.Clear();
	}

	//------------------------------------------------------------------------------------------------
	//! Leases of these kinds draw a cable
	protected bool HasCable(LC_ERadioLeaseKind kind)
	{
		return kind == LC_ERadioLeaseKind.PHONE;
	}

	//------------------------------------------------------------------------------------------------
	//! Where the cable ends on the leased entity, in world space
	protected vector GetAnchor(LC_ERadioLeaseKind kind, notnull IEntity radio)
	{
		LC_InfantryPhoneComponent phone = LC_InfantryPhoneComponent.Cast(radio.FindComponent(LC_InfantryPhoneComponent));
		if (phone)
			return phone.GetPhonePosition();

		return radio.GetOrigin();
	}

	//------------------------------------------------------------------------------------------------
	//! The cable's length: the lease's own limit
	protected float GetLength(LC_ERadioLeaseKind kind, notnull IEntity radio)
	{
		LC_InfantryPhoneComponent phone = LC_InfantryPhoneComponent.Cast(radio.FindComponent(LC_InfantryPhoneComponent));
		if (phone)
			return phone.GetCableLength();

		return LC_RadioLeases.HANDSET_CABLE_M;
	}

	//------------------------------------------------------------------------------------------------
	//! The prop put in the holder's hand (empty: none)
	protected ResourceName GetHandsetPrefab(LC_ERadioLeaseKind kind, IEntity radio)
	{
		if (!radio)
			return ResourceName.Empty;

		LC_InfantryPhoneComponent phone = LC_InfantryPhoneComponent.Cast(radio.FindComponent(LC_InfantryPhoneComponent));
		if (phone)
			return phone.GetHandsetPrefab();

		return ResourceName.Empty;
	}

	//------------------------------------------------------------------------------------------------
	//! One cable per (radio, player, kind) lease of a kind with a cable; drops the rest
	protected void Reconcile(notnull LC_RadioLeases leases)
	{
		int count = leases.GetLeaseCount();
		array<bool> seen = {};
		seen.Resize(m_aCables.Count());
		for (int i = 0; i < count; i++)
		{
			RplId radioId;
			int playerId;
			LC_ERadioLeaseKind kind;
			leases.GetLeaseAt(i, radioId, playerId, kind);
			if (!HasCable(kind))
				continue;

			bool found;
			foreach (int c, LC_HandsetCable cable : m_aCables)
			{
				if (c < seen.Count() && cable.Is(radioId, playerId, kind))
				{
					seen[c] = true;
					found = true;
					break;
				}
			}

			if (!found)
				m_aCables.Insert(new LC_HandsetCable(radioId, playerId, kind, GetHandsetPrefab(kind, LC_RadioLeases.FindRadio(radioId))));
		}

		for (int c = seen.Count() - 1; c >= 0; c--)
		{
			if (!seen[c])
				m_aCables.RemoveOrdered(c);
		}
	}
}
