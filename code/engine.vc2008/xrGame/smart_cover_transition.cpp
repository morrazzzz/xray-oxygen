////////////////////////////////////////////////////////////////////////////
//	Module 		: smart_cover_transition.cpp
//	Created 	: 20.12.2007
//	Author		: Alexander Dudin
//	Description : Transition class for smart_cover
////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "smart_cover_transition.hpp"
#include "smart_cover_transition_animation.hpp"
#include "smart_cover_detail.h"
#include "ai_space.h"
#include "script_engine.h"
#include "../xrServerEntities/object_broker.h"
#include <luabind/luabind.hpp>

namespace smart_cover
{
	namespace transitions
	{


action::action					(luabind::object const &table)
{
	VERIFY						(table.type() == LUA_TTABLE);

	m_precondition_functor		= smart_cover::detail::parse_string(table, "precondition_functor");
	m_precondition_params		= smart_cover::detail::parse_string(table, "precondition_params");

	luabind::object				anim_table;
	smart_cover::detail::parse_table (table, "actions", anim_table);
	load_animations				(anim_table);
}

action::~action()
{
	delete_data(m_animations);
}

bool action::applicable			() const
{
	luabind::functor<bool>		functor;

	R_ASSERT_FORMAT(ai().script_engine().functor(m_precondition_functor.c_str(),functor), 
		"failed to get [%s]", m_precondition_functor.c_str());

	return						(functor(m_precondition_params.c_str()));
}

void action::load_animations	(luabind::object const &table)
{
	luabind::object::iterator	I = table.begin();
	luabind::object::iterator	E = table.end();
	for ( ; I != E; ++I) {
		luabind::object			tmp = *I;
		Fvector	const			&pos = smart_cover::detail::parse_fvector(tmp, "position");
		shared_str				anim_id = smart_cover::detail::parse_string(tmp, "animation");
		MonsterSpace::EBodyState body_state = (MonsterSpace::EBodyState)smart_cover::detail::parse_int(tmp, "body_state");
		MonsterSpace::EMovementType movement_type = (MonsterSpace::EMovementType)smart_cover::detail::parse_int(tmp, "movement_type");
		animation_action		*animation = xr_new<animation_action>(pos, anim_id, body_state, movement_type);
		m_animations.push_back	(animation);
	}
}

class body_state_predicate {
	MonsterSpace::EBodyState	m_body_state;

public:
	IC	body_state_predicate	(MonsterSpace::EBodyState const &body_state) :
		m_body_state			(body_state)
	{
	}

	IC	bool	operator()		(animation_action *animation_action) const
	{
		VERIFY					(animation_action);

		//if (!animation_action->has_animation())
		//	return				(false);

		return					(m_body_state == animation_action->body_state());
	}
};

animation_action const &action::animation	(MonsterSpace::EBodyState const &target_body_state) const
{
	Animations::const_iterator found = 
		std::find_if			(m_animations.begin(), m_animations.end(), body_state_predicate(target_body_state));
	
	if (found == m_animations.end()) {
#ifndef MASTER_GOLD
		Msg						("! There is no animation which can transfer bot to body_state [%i], selecting random transition", target_body_state);
#endif // #ifndef MASTER_GOLD
		return					(animation());
	}

	return						(**found);
}

animation_action const	&action::animation	() const
{
	return						(*m_animations[Random.randI((u32)m_animations.size())]);
}


	}
}
