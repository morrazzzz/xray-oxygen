#include "stdafx.h"
#include "xr_area.h"
#include "ispatial.h"
#include "cl_intersect.h"
#include "../xrengine/xr_collide_form.h"
#include "../xrengine/xr_object.h"
#include "d3d9types.h"
#ifdef	DEBUG
static bool _cdb_bDebug = false;
extern XRCDB_API bool *cdb_bDebug = &_cdb_bDebug;
bool bDebug()
{
	return !!(*cdb_bDebug);
}
#endif
#pragma warning(disable: 4267)

namespace CObjectSpacePrivate
{
	thread_local xrXRC xrc;
	thread_local collide::rq_results r_temp;
	thread_local xr_vector<ISpatial*> r_spatial;
}

using namespace	collide;

//--------------------------------------------------------------------------------
// RayTest - Occluded/No
//--------------------------------------------------------------------------------
bool CObjectSpace::RayTest(const Fvector &start, const Fvector &dir, float range, collide::rq_target tgt, collide::ray_cache* cache, CObject* ignore_object)
{
	bool	_ret = _RayTest(start, dir, range, tgt, cache, ignore_object);
	CObjectSpacePrivate::r_spatial.clear();
	return			_ret;
}
bool CObjectSpace::_RayTest(const Fvector &start, const Fvector &dir, float range, collide::rq_target tgt, collide::ray_cache* cache, CObject* ignore_object)
{
	VERIFY(_abs(dir.magnitude() - 1)<EPS);
	CObjectSpacePrivate::r_temp.r_clear();

	CObjectSpacePrivate::xrc.ray_options(CDB::OPT_ONLYFIRST);
	collide::ray_defs	Q(start, dir, range, CDB::OPT_ONLYFIRST, tgt);

	// dynamic test
	if (tgt&rqtDyn) {
		u32			d_flags = STYPE_COLLIDEABLE | ((tgt&rqtObstacle) ? STYPE_OBSTACLE : 0) | ((tgt&rqtShape) ? STYPE_SHAPE : 0);
		// traverse object database
		g_SpatialSpace->q_ray(CObjectSpacePrivate::r_spatial, 0, d_flags, start, dir, range);
		// Determine visibility for dynamic part of scene
		for (u32 o_it = 0; o_it< CObjectSpacePrivate::r_spatial.size(); o_it++)
		{
			ISpatial*	spatial = CObjectSpacePrivate::r_spatial[o_it];
			CObject*	collidable = spatial->dcast_CObject();
			if (collidable && (collidable != ignore_object)) {
				ECollisionFormType tp = collidable->collidable.model->Type();
				if ((tgt&(rqtObject | rqtObstacle)) && (tp == cftObject) && collidable->collidable.model->_RayQuery(Q, CObjectSpacePrivate::r_temp))	return true;
				if ((tgt&rqtShape) && (tp == cftShape) && collidable->collidable.model->_RayQuery(Q, CObjectSpacePrivate::r_temp))		return true;
			}
		}
	}
	// static test
	if (tgt&rqtStatic) {
		// If we get here - test static model
		if (cache)
		{
			// 0. similar query???
			if (cache->similar(start, dir, range)) {
				return cache->result;
			}

			// 1. Check cached polygon
			float _u, _v, _range;
			if (CDB::TestRayTri(start, dir, cache->verts, _u, _v, _range, false))
				if (_range>0 && _range<range) return true;

			// 2. Polygon doesn't pick - real database query
			CObjectSpacePrivate::xrc.ray_query(&Static, start, dir, range);
			if (CObjectSpacePrivate::xrc.r_empty()) {
				cache->set(start, dir, range, false);
				return false;
			}
			else {
				// cache polygon
				cache->set(start, dir, range, true);
				CDB::RESULT*	R = &(*CObjectSpacePrivate::xrc.r_realBegin());
				CDB::TRI&		T = Static.get_tris()[R->id];
				Fvector*		V = Static.get_verts();
				cache->verts[0].set(V[T.verts[0]]);
				cache->verts[1].set(V[T.verts[1]]);
				cache->verts[2].set(V[T.verts[2]]);
				return true;
			}
		}
		else {
			CObjectSpacePrivate::xrc.ray_query(&Static, start, dir, range);
			return !CObjectSpacePrivate::xrc.r_empty();
		}
	}
	return false;
}

//--------------------------------------------------------------------------------
// RayPick
//--------------------------------------------------------------------------------
bool CObjectSpace::RayPick(const Fvector &start, const Fvector &dir, float range, rq_target tgt, rq_result& R, CObject* ignore_object)
{
	bool	_res = _RayPick(start, dir, range, tgt, R, ignore_object);
	CObjectSpacePrivate::r_spatial.clear();
	return	_res;
}
bool CObjectSpace::_RayPick(const Fvector &start, const Fvector &dir, float range, rq_target tgt, rq_result& R, CObject* ignore_object)
{
	CObjectSpacePrivate::r_temp.r_clear();
	R.O = 0; R.range = range; R.element = -1;
	// static test
	if (tgt&rqtStatic) {
		CObjectSpacePrivate::xrc.ray_options(CDB::OPT_ONLYNEAREST | CDB::OPT_CULL);
		CObjectSpacePrivate::xrc.ray_query(&Static, start, dir, range);
		if (!CObjectSpacePrivate::xrc.r_empty())
		{
			CDB::RESULT* firstVertex = &(*CObjectSpacePrivate::xrc.r_realBegin());
			R.set_if_less(firstVertex);
		}
	}
	// dynamic test
	if (tgt&rqtDyn) {
		collide::ray_defs Q(start, dir, R.range, CDB::OPT_ONLYNEAREST | CDB::OPT_CULL, tgt);
		// traverse object database
		u32			d_flags = STYPE_COLLIDEABLE | ((tgt&rqtObstacle) ? STYPE_OBSTACLE : 0) | ((tgt&rqtShape) ? STYPE_SHAPE : 0);
		g_SpatialSpace->q_ray(CObjectSpacePrivate::r_spatial, 0, d_flags, start, dir, range);
		// Determine visibility for dynamic part of scene
		for (u32 o_it = 0; o_it < CObjectSpacePrivate::r_spatial.size(); o_it++) {
			ISpatial*	spatial = CObjectSpacePrivate::r_spatial[o_it];
			CObject*	collidable = spatial->dcast_CObject();
			if (0 == collidable)				continue;
			if (collidable == ignore_object)	continue;
			ECollisionFormType tp = collidable->collidable.model->Type();
			if (((tgt&(rqtObject | rqtObstacle)) && (tp == cftObject)) || ((tgt&rqtShape) && (tp == cftShape))) {
				u32		C = D3DCOLOR_XRGB(64, 64, 64);
				Q.range = R.range;
				if (collidable->collidable.model->_RayQuery(Q, CObjectSpacePrivate::r_temp)) {
					C = D3DCOLOR_XRGB(128, 128, 196);
					R.set_if_less(CObjectSpacePrivate::r_temp.r_getElement(0));
				}
#ifdef DEBUG
				if (bDebug()) {
					Fsphere	S;		S.P = spatial->spatial.sphere.P; S.R = spatial->spatial.sphere.R;
					(*m_pRender)->dbgAddSphere(S, C);
					//dbg_S.push_back	(std::make_pair(S,C));
				}
#endif
			}
		}
	}
	return (R.element >= 0);
}

//--------------------------------------------------------------------------------
// RayQuery
//--------------------------------------------------------------------------------
bool CObjectSpace::RayQuery(collide::rq_results& dest, const collide::ray_defs& R, collide::rq_callback* CB, LPVOID user_data, collide::test_callback* tb, CObject* ignore_object)
{
	bool						_res = _RayQuery2(dest, R, CB, user_data, tb, ignore_object);
	CObjectSpacePrivate::r_spatial.clear();
	return						(_res);
}
bool CObjectSpace::_RayQuery2(collide::rq_results& r_dest, const collide::ray_defs& R, collide::rq_callback* CB, LPVOID user_data, collide::test_callback* tb, CObject* ignore_object)
{
	// initialize query
	r_dest.r_clear();
	CObjectSpacePrivate::r_temp.r_clear();

	rq_target	s_mask = rqtStatic;
	rq_target	d_mask = rq_target(((R.tgt&rqtObject) ? rqtObject : rqtNone) |
		((R.tgt&rqtObstacle) ? rqtObstacle : rqtNone) |
		((R.tgt&rqtShape) ? rqtShape : rqtNone));
	u32			d_flags = STYPE_COLLIDEABLE | ((R.tgt&rqtObstacle) ? STYPE_OBSTACLE : 0) | ((R.tgt&rqtShape) ? STYPE_SHAPE : 0);

	// Test static
	if (R.tgt&s_mask) {
		CObjectSpacePrivate::xrc.ray_options(R.flags);
		CObjectSpacePrivate::xrc.ray_query(&Static, R.start, R.dir, R.range);
		if (!CObjectSpacePrivate::xrc.r_empty())
			for (auto _I = CObjectSpacePrivate::xrc.r_realBegin(); _I != CObjectSpacePrivate::xrc.r_realEnd(); _I++)
				CObjectSpacePrivate::r_temp.append_result(rq_result().set(0, _I->range, _I->id));
	}
	// Test dynamic
	if (R.tgt&d_mask) {
		// Traverse object database
		g_SpatialSpace->q_ray(CObjectSpacePrivate::r_spatial, 0, d_flags, R.start, R.dir, R.range);
		for (u32 o_it = 0; o_it < CObjectSpacePrivate::r_spatial.size(); o_it++) {
			CObject*	collidable = CObjectSpacePrivate::r_spatial[o_it]->dcast_CObject();
			if (!collidable)				continue;
			if (collidable == ignore_object)	continue;
			ICollisionForm*	cform = collidable->collidable.model;
			ECollisionFormType tp = collidable->collidable.model->Type();
			if (((R.tgt&(rqtObject | rqtObstacle)) && (tp == cftObject)) || ((R.tgt&rqtShape) && (tp == cftShape))) {
				if (tb && !tb(R, collidable, user_data))continue;
				cform->_RayQuery(R, CObjectSpacePrivate::r_temp);
			}
		}
	}
	if (CObjectSpacePrivate::r_temp.r_count()) {
		CObjectSpacePrivate::r_temp.r_sort();
		
		for (auto _I = CObjectSpacePrivate::r_temp.r_realBegin(); _I != CObjectSpacePrivate::r_temp.r_realEnd(); _I++) {
			r_dest.append_result(*_I);
			if (!(CB ? CB(*_I, user_data) : true))						return !r_dest.r_empty();
			if (R.flags&(CDB::OPT_ONLYNEAREST | CDB::OPT_ONLYFIRST))	return !r_dest.r_empty();
		}
	}
	return !!r_dest.r_count();
}

bool CObjectSpace::_RayQuery3(collide::rq_results& r_dest, const collide::ray_defs& R, collide::rq_callback* CB, LPVOID user_data, collide::test_callback* tb, CObject* ignore_object)
{
	// initialize query
	r_dest.r_clear();

	ray_defs	d_rd(R);
	ray_defs	s_rd(R.start, R.dir, R.range, CDB::OPT_ONLYNEAREST | R.flags, R.tgt);
	rq_target	s_mask = rqtStatic;
	rq_target	d_mask = rq_target(((R.tgt&rqtObject) ? rqtObject : rqtNone) |
		((R.tgt&rqtObstacle) ? rqtObstacle : rqtNone) |
		((R.tgt&rqtShape) ? rqtShape : rqtNone));
	u32			d_flags = STYPE_COLLIDEABLE | ((R.tgt&rqtObstacle) ? STYPE_OBSTACLE : 0) | ((R.tgt&rqtShape) ? STYPE_SHAPE : 0);
	float		d_range = 0.f;

	do {
		CObjectSpacePrivate::r_temp.r_clear();
		if (R.tgt&s_mask) {
			// static test allowed

			// test static
			CObjectSpacePrivate::xrc.ray_options(s_rd.flags);
			CObjectSpacePrivate::xrc.ray_query(&Static, s_rd.start, s_rd.dir, s_rd.range);

			if (!CObjectSpacePrivate::xrc.r_empty()) {
				VERIFY(CObjectSpacePrivate::xrc.r_count() == 1);
				rq_result		s_res;
				s_res.set(0, CObjectSpacePrivate::xrc.r_realBegin()->range, CObjectSpacePrivate::xrc.r_realBegin()->id);
				// update dynamic test range
				d_rd.range = s_res.range;
				// set next static start & range
				s_rd.range -= (s_res.range + EPS_L);
				s_rd.start.mad(s_rd.dir, s_res.range + EPS_L);
				s_res.range = R.range - s_rd.range - EPS_L;
				CObjectSpacePrivate::r_temp.append_result(s_res);
			}
			else {
				d_rd.range = s_rd.range;
			}
		}
		// test dynamic
		if (R.tgt&d_mask) {
			// Traverse object database
			g_SpatialSpace->q_ray(CObjectSpacePrivate::r_spatial, 0, d_flags, d_rd.start, d_rd.dir, d_rd.range);
			for (u32 o_it = 0; o_it < CObjectSpacePrivate::r_spatial.size(); o_it++) {
				CObject*	collidable = CObjectSpacePrivate::r_spatial[o_it]->dcast_CObject();
				if (0 == collidable)				continue;
				if (collidable == ignore_object)	continue;
				ICollisionForm*	cform = collidable->collidable.model;
				ECollisionFormType tp = collidable->collidable.model->Type();
				if (((R.tgt&(rqtObject | rqtObstacle)) && (tp == cftObject)) || ((R.tgt&rqtShape) && (tp == cftShape))) {
					if (tb && !tb(d_rd, collidable, user_data))continue;
					u32 r_cnt = CObjectSpacePrivate::r_temp.r_count();
					cform->_RayQuery(d_rd, CObjectSpacePrivate::r_temp);
					for (int k = r_cnt; k < CObjectSpacePrivate::r_temp.r_count(); k++) {
						rq_result& d_res = *(CObjectSpacePrivate::r_temp.r_getElement(k));
						d_res.range += d_range;
					}
				}
			}
		}
		// set dynamic ray def
		d_rd.start = s_rd.start;
		d_range = R.range - s_rd.range;
		if (CObjectSpacePrivate::r_temp.r_count())
		{
			CObjectSpacePrivate::r_temp.r_sort();
			for (auto _I = CObjectSpacePrivate::r_temp.r_realBegin(); _I != CObjectSpacePrivate::r_temp.r_realEnd(); _I++)
			{
				r_dest.append_result(*_I);
				if (!(CB ? CB(*_I, user_data) : true))	return !r_dest.r_empty();
				if (R.flags&CDB::OPT_ONLYFIRST)			return !r_dest.r_empty();
			}
		}
		if ((R.flags&(CDB::OPT_ONLYNEAREST | CDB::OPT_ONLYFIRST)) && r_dest.r_count()) return !!r_dest.r_count();
	} while (CObjectSpacePrivate::r_temp.r_count());
	return !!r_dest.r_count();
}

bool CObjectSpace::_RayQuery(collide::rq_results& r_dest, const collide::ray_defs& R, collide::rq_callback* CB, LPVOID user_data, collide::test_callback* tb, CObject* ignore_object)
{
#ifdef DEBUG
	if (R.range<EPS || !_valid(R.range))
		Debug.fatal(DEBUG_INFO, "Invalid RayQuery range passed: %f.", R.range);
#endif
	// initialize query
	r_dest.r_clear();
	CObjectSpacePrivate::r_temp.r_clear();

	Flags32		sd_test;	sd_test.assign(R.tgt);
	rq_target	next_test = R.tgt;

	rq_result	s_res;
	ray_defs	s_rd(R.start, R.dir, R.range, CDB::OPT_ONLYNEAREST | R.flags, R.tgt);
	ray_defs	d_rd(R.start, R.dir, R.range, CDB::OPT_ONLYNEAREST | R.flags, R.tgt);
	rq_target	s_mask = rqtStatic;
	rq_target	d_mask = rq_target(((R.tgt&rqtObject) ? rqtObject : rqtNone) |
		((R.tgt&rqtObstacle) ? rqtObstacle : rqtNone) |
		((R.tgt&rqtShape) ? rqtShape : rqtNone));
	u32			d_flags = STYPE_COLLIDEABLE | ((R.tgt&rqtObstacle) ? STYPE_OBSTACLE : 0) | ((R.tgt&rqtShape) ? STYPE_SHAPE : 0);

	s_res.set(0, s_rd.range, -1);
	do {
		if ((R.tgt&s_mask) && sd_test.is(s_mask) && (next_test&s_mask)) {
			s_res.set(0, s_rd.range, -1);
			// Test static model
			if (s_rd.range>EPS) {
				CObjectSpacePrivate::xrc.ray_options(s_rd.flags);
				CObjectSpacePrivate::xrc.ray_query(&Static, s_rd.start, s_rd.dir, s_rd.range);
				if (!CObjectSpacePrivate::xrc.r_empty()) {
					if (s_res.set_if_less(&(*CObjectSpacePrivate::xrc.r_realBegin()))) {
						// set new static start & range
						s_rd.range -= (s_res.range + EPS_L);
						s_rd.start.mad(s_rd.dir, s_res.range + EPS_L);
						s_res.range = R.range - s_rd.range - EPS_L;
#ifdef DEBUG
						if (!(fis_zero(s_res.range, EPS) || s_res.range >= 0.f))
							Debug.fatal(DEBUG_INFO, "Invalid RayQuery static range: %f (%f). /#1/", s_res.range, s_rd.range);
#endif
					}
				}
			}
			if (!s_res.valid())	sd_test.set(s_mask, false);
		}
		if ((R.tgt&d_mask) && sd_test.is_any(d_mask) && (next_test&d_mask)) {
			CObjectSpacePrivate::r_temp.r_clear();

			if (d_rd.range>EPS) {
				// Traverse object database
				g_SpatialSpace->q_ray(CObjectSpacePrivate::r_spatial, 0, d_flags, d_rd.start, d_rd.dir, d_rd.range);
				// Determine visibility for dynamic part of scene
				for (u32 o_it = 0; o_it < CObjectSpacePrivate::r_spatial.size(); o_it++) {
					CObject*	collidable = CObjectSpacePrivate::r_spatial[o_it]->dcast_CObject();
					if (0 == collidable)				continue;
					if (collidable == ignore_object)	continue;
					ICollisionForm*	cform = collidable->collidable.model;
					ECollisionFormType tp = collidable->collidable.model->Type();
					if (((R.tgt&(rqtObject | rqtObstacle)) && (tp == cftObject)) || ((R.tgt&rqtShape) && (tp == cftShape))) {
						if (tb && !tb(d_rd, collidable, user_data))continue;
						cform->_RayQuery(d_rd, CObjectSpacePrivate::r_temp);
					}
#ifdef DEBUG
					if (!((CObjectSpacePrivate::r_temp.r_empty()) || (CObjectSpacePrivate::r_temp.r_count() && (fis_zero(CObjectSpacePrivate::r_temp.r_realBegin()->range, EPS) || (CObjectSpacePrivate::r_temp.r_realBegin()->range >= 0.f)))))
						Debug.fatal(DEBUG_INFO, "Invalid RayQuery dynamic range: %f (%f). /#2/", CObjectSpacePrivate::r_temp.r_realBegin()->range, d_rd.range);
#endif
				}
			}
			if (!CObjectSpacePrivate::r_temp.r_empty()) {
				// set new dynamic start & range
				rq_result& d_res = *CObjectSpacePrivate::r_temp.r_getElement(0);
				d_rd.range -= (d_res.range + EPS_L);
				d_rd.start.mad(d_rd.dir, d_res.range + EPS_L);
				d_res.range = R.range - d_rd.range - EPS_L;
#ifdef DEBUG
				if (!(fis_zero(d_res.range, EPS) || d_res.range >= 0.f))
					Debug.fatal(DEBUG_INFO, "Invalid RayQuery dynamic range: %f (%f). /#3/", d_res.range, d_rd.range);
#endif
			}
			else {
				sd_test.set(d_mask, false);
			}
		}
		if (s_res.valid() && CObjectSpacePrivate::r_temp.r_count()) {
			// all test return result
			if (s_res.range < CObjectSpacePrivate::r_temp.r_realBegin()->range) {
				// static nearer
				bool need_calc = CB ? CB(s_res, user_data) : true;
				next_test = need_calc ? s_mask : rqtNone;
				r_dest.append_result(s_res);
			}
			else {
				// dynamic nearer
				bool need_calc = CB ? CB(*CObjectSpacePrivate::r_temp.r_realBegin(), user_data) : true;
				next_test = need_calc ? d_mask : rqtNone;
				r_dest.append_result(*CObjectSpacePrivate::r_temp.r_realBegin());
			}
		}
		else if (s_res.valid()) {
			// only static return result
			bool need_calc = CB ? CB(s_res, user_data) : true;
			next_test = need_calc ? s_mask : rqtNone;
			r_dest.append_result(s_res);
		}
		else if (CObjectSpacePrivate::r_temp.r_count()) {
			// only dynamic return result
			bool need_calc = CB ? CB(*CObjectSpacePrivate::r_temp.r_realBegin(), user_data) : true;
			next_test = need_calc ? d_mask : rqtNone;
			r_dest.append_result(*CObjectSpacePrivate::r_temp.r_realBegin());
		}
		else {
			// nothing selected
			next_test = rqtNone;
		}
		if ((R.flags&CDB::OPT_ONLYFIRST) || (R.flags&CDB::OPT_ONLYNEAREST)) break;
	} while (next_test != rqtNone);
	return !!r_dest.r_count();
}

bool CObjectSpace::RayQuery(collide::rq_results& r_dest, ICollisionForm* target, const collide::ray_defs& R)
{
	VERIFY(target);
	r_dest.r_clear();
	return !!target->_RayQuery(R, r_dest);
}

IC int	CObjectSpace::GetNearest(xr_vector<CObject*>& q_nearest, const Fvector& point, float range, CObject* ignore_object)
{
	return(GetNearest(CObjectSpacePrivate::r_spatial, q_nearest, point, range, ignore_object));
}

//----------------------------------------------------------------------
IC int   CObjectSpace::GetNearest(xr_vector<CObject*>& q_nearest, ICollisionForm* obj, float range)
{
	CObject* O = obj->Owner();
	return GetNearest(q_nearest, O->spatial.sphere.P, range + O->spatial.sphere.R, O);
}


bool CObjectSpace::BoxQuery(Fvector const& box_center,
	Fvector const& box_z_axis,
	Fvector const& box_y_axis,
	Fvector const& box_sizes,
	xr_vector<Fvector>* out_tris)
{
	Fvector z_axis = box_z_axis;
	z_axis.normalize();
	Fvector y_axis = box_y_axis;
	y_axis.normalize();
	Fvector x_axis;
	x_axis.crossproduct(box_y_axis, box_z_axis).normalize();

	Fplane planes[6];
	enum { left_plane, right_plane, top_plane, bottom_plane, front_plane, near_plane };

	planes[left_plane].build(box_center - (x_axis * (box_sizes.x * 0.5f)), -x_axis);
	planes[right_plane].build(box_center + (x_axis * (box_sizes.x * 0.5f)), x_axis);
	planes[top_plane].build(box_center + (y_axis * (box_sizes.y * 0.5f)), y_axis);
	planes[bottom_plane].build(box_center - (y_axis * (box_sizes.y * 0.5f)), -y_axis);
	planes[front_plane].build(box_center - (z_axis * (box_sizes.z * 0.5f)), -z_axis);
	planes[near_plane].build(box_center + (z_axis * (box_sizes.z * 0.5f)), z_axis);

	CFrustum	frustum;
	frustum.CreateFromPlanes(planes, sizeof(planes) / sizeof(planes[0]));

	CObjectSpacePrivate::xrc.frustum_options(CDB::OPT_FULL_TEST);
	CObjectSpacePrivate::xrc.frustum_query(&Static, frustum);

	if (out_tris)
	{
		for (auto	result = CObjectSpacePrivate::xrc.r_realBegin();
			result != CObjectSpacePrivate::xrc.r_realEnd();
			++result)
		{
			out_tris->push_back(result->verts[0]);
			out_tris->push_back(result->verts[1]);
			out_tris->push_back(result->verts[2]);
		}
	}

	return						!CObjectSpacePrivate::xrc.r_empty();
}