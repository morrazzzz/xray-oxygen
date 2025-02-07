#include "stdafx.h"
#include "xrstripify.h"

#include "NvTriStrip.h"
#include "VertexCache.h"

int xrSimulate (xr_vector<u16> &indices, int iCacheSize )
{
	VertexCache C(iCacheSize);

	int count=0;
	for (u32 i=0; i<indices.size(); i++)
	{
		int id = indices[i];
		if (C.InCache(id)) continue;
		count			++;
		C.AddEntry		(id);
	}
	return count;
}

void xrStripify(xr_vector<u16>& indices, xr_vector<u16>& perturb, int iCacheSize, int iMinStripLength)
{
	SetCacheSize(iCacheSize);
	SetMinStripSize(iMinStripLength);
	SetListsOnly(true);

	// Generate strips
	PrimitiveGroup* PGROUP[2] =
	{
		new PrimitiveGroup[1],
		new PrimitiveGroup[1]
	};

	u16 GroupCount = 0;
	GenerateStrips(&*indices.begin(), (u32)indices.size(), &PGROUP[0], &GroupCount);

	// Remap indices
	RemapIndices(PGROUP[0], GroupCount, u16(perturb.size()), &PGROUP[1]);

	// Build perturberation table
	for (u32 index = 0; index < PGROUP[0][0].numIndices; index++)
	{
		u16 oldIndex = PGROUP[0][0].indices[index];
		u16 newIndex = PGROUP[1][0].indices[index];
		perturb[newIndex] = oldIndex;
	}

	// Copy indices
	indices.clear();
	for (u32 Iter = 0; Iter < PGROUP[1][0].numIndices; Iter++)
		indices.emplace_back(PGROUP[1][0].indices[Iter]);
}
