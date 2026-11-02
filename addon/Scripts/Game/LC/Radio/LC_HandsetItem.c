[ComponentEditorProps(category: "Lima Charlie", description: "Lima Charlie handset item: given to whoever holds an infantry phone (later a manpack handset) so the character holds it like a radio. Created and deleted by the server with the lease; never pick one up or keep one.")]
class LC_HandsetGadgetComponentClass : SCR_GadgetComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! Marks the handset item. A gadget so the engine's left-hand gadget pose (the radio's
//! animation, set by m_eAnimVariable and the item's ItemAnimationAttributes) applies and replicates to everyone.
//! It carries no mesh: LC_HandsetCable puts the visible handset on it and draws the cable from there.
class LC_HandsetGadgetComponent : SCR_GadgetComponent
{
	//------------------------------------------------------------------------------------------------
	//! The handset item in a character's left hand, or null (works on every machine: the gadget in hand is replicated)
	static IEntity GetHeld(IEntity character)
	{
		ChimeraCharacter chimera = ChimeraCharacter.Cast(character);
		if (!chimera || !chimera.GetCharacterController())
			return null;

		IEntity held = chimera.GetCharacterController().GetAttachedGadgetAtLeftHandSlot();
		if (held && held.FindComponent(LC_HandsetGadgetComponent))
			return held;

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! The handset item a character has, in hand or inventory, or null
	static IEntity FindOn(IEntity character)
	{
		ChimeraCharacter chimera = ChimeraCharacter.Cast(character);
		if (!chimera)
			return null;

		CharacterControllerComponent controller = chimera.GetCharacterController();
		if (controller)
		{
			IEntity held = controller.GetAttachedGadgetAtLeftHandSlot();
			if (held && held.FindComponent(LC_HandsetGadgetComponent))
				return held;
		}

		SCR_InventoryStorageManagerComponent inventory = SCR_InventoryStorageManagerComponent.Cast(character.FindComponent(SCR_InventoryStorageManagerComponent));
		if (!inventory)
			return null;

		array<IEntity> items = {};
		inventory.FindItemsWithComponents(items, {LC_HandsetGadgetComponent}, EStoragePurpose.PURPOSE_ANY);
		if (items.IsEmpty())
			return null;

		return items[0];
	}
}

//------------------------------------------------------------------------------------------------
//! The handset item's life: the server gives it with the lease and deletes it when the lease ends;
//! the holder's own machine takes it into the left hand once (the radio hold pose). After that the player
//! is free: pulling out something else ends the pose and it is not forced back.
class LC_HandsetItem
{
	//! While the item is not in hand yet, ask again this often, at most MAX_TRIES times
	protected static const int RETRY_MS = 500;
	protected static const int MAX_TRIES = 6;

	//! Weak: the item last taken into hand (or given up on); nothing more is done for it
	protected IEntity m_Done;
	protected IEntity m_Item;
	protected int m_iTries;
	protected int m_iNextTryTick;

	//------------------------------------------------------------------------------------------------
	//! Server: spawns the item into the holder's inventory, or null (no prefab, no room)
	static IEntity ServerGive(notnull IEntity holder, ResourceName prefab)
	{
		if (prefab.IsEmpty())
			return null;

		SCR_InventoryStorageManagerComponent inventory = SCR_InventoryStorageManagerComponent.Cast(holder.FindComponent(SCR_InventoryStorageManagerComponent));
		Resource resource = Resource.Load(prefab);
		if (!inventory || !resource.IsValid())
			return null;

		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		holder.GetTransform(params.Transform);
		IEntity item = GetGame().SpawnEntityPrefab(resource, holder.GetWorld(), params);
		if (!item)
			return null;

		if (!inventory.TryInsertItem(item))
		{
			Print("[LC] Handset item: no room in the holder's inventory", LogLevel.WARNING);
			SCR_EntityHelper.DeleteEntityAndChildren(item);
			return null;
		}

		return item;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: removes it wherever it is (inventory, hand or ground)
	static void ServerTake(IEntity item)
	{
		if (!item)
			return;

		// Through the inventory when it is in one, so the storage lets go of it; otherwise a replicated delete
		IEntity root = item.GetRootParent();
		SCR_InventoryStorageManagerComponent inventory;
		if (root && root != item)
			inventory = SCR_InventoryStorageManagerComponent.Cast(root.FindComponent(SCR_InventoryStorageManagerComponent));

		if (inventory && inventory.TryDeleteItem(item))
			return;

		RplComponent.DeleteRplEntity(item, false);
	}

	//------------------------------------------------------------------------------------------------
	//! Holder's machine, every frame while holding a lease: takes a newly given handset item into the left hand,
	//! once. Not inspected: inspection hides the world actions and forces free look.
	void Update(IEntity character, int now)
	{
		ChimeraCharacter chimera = ChimeraCharacter.Cast(character);
		if (!chimera)
			return;

		CharacterControllerComponent controller = chimera.GetCharacterController();
		IEntity item = LC_HandsetGadgetComponent.FindOn(character);
		if (!controller || !item || item == m_Done)
			return;

		if (item != m_Item)
		{
			m_Item = item;
			m_iTries = 0;
			m_iNextTryTick = now;
		}

		if (controller.GetAttachedGadgetAtLeftHandSlot() == item || m_iTries >= MAX_TRIES)
		{
			m_Done = item;
			return;
		}

		if (now < m_iNextTryTick)
			return;

		SCR_GadgetManagerComponent gadgets = SCR_GadgetManagerComponent.GetGadgetManager(character);
		if (gadgets)
			gadgets.SetGadgetMode(item, EGadgetMode.IN_HAND);

		m_iTries++;
		m_iNextTryTick = now + RETRY_MS;
	}
}
