////////////////////////////////////////////////////////////////////////////
//	Module 		: alife_simulator.h
//	Created 	: 25.12.2002
//  Modified 	: 13.05.2004
//	Author		: Dmitriy Iassenev
//	Description : ALife Simulator
////////////////////////////////////////////////////////////////////////////

#pragma once
#include "alife_simulator_base.h"
#include "alife_update_manager.h"
#include "../xrScripts/export/script_export_space.h"

#pragma warning(push)
#pragma warning(disable:4005)

class CALifeSimulator : public CALifeUpdateManager, public virtual CALifeSimulatorBase
{
protected:
	virtual void	setup_simulator		(CSE_ALifeObject *object);
	virtual void	reload				(LPCSTR section);

public:
					CALifeSimulator		(xrServer *server, shared_str* command_line);
	virtual			~CALifeSimulator	();
	virtual	void	destroy				();
	IReader const* get_config			( shared_str config ) const;
	void			kill_entity			(CSE_ALifeMonsterAbstract *l_tpALifeMonsterAbstract, const u16 &l_tGraphID, CSE_ALifeSchedulable *schedulable);

private:
	typedef xr_list< std::pair<shared_str,IReader*> >	configs_type;
	mutable configs_type	m_configs_lru;

	DECLARE_SCRIPT_REGISTER_FUNCTION
};
add_to_type_list(CALifeSimulator)
#define script_type_list save_type_list(CALifeSimulator)

#pragma warning(pop)


namespace AlifeUtils
{
	bool object_exists_in_alife_registry(u32 id);
}