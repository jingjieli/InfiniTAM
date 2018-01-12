//  ================================================================
//  Created by Gregory Kramida on 1/5/18.
//  Copyright (c) 2018-2025 Gregory Kramida
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at

//  http://www.apache.org/licenses/LICENSE-2.0

//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.
//  ================================================================
#include "ITMSceneStatisticsCalculator.h"
#include "ITMMath.h"
#include "../Objects/Scene/ITMScene.h"
#include "../Objects/Scene/ITMVoxelBlockHash.h"

using namespace ITMLib;

template<typename TVoxel, typename TIndex>
void ITMSceneStatisticsCalculator<TVoxel, TIndex>::ComputeVoxelBounds(ITMScene<TVoxel, TIndex>* scene,
                                                                      Vector3i& minVoxelPoint,
                                                                      Vector3i& maxVoxelPoint) {

	minVoxelPoint = maxVoxelPoint = Vector3i(0);
	TVoxel* voxelBlocks = scene->localVBA.GetVoxelBlocks();
	const ITMHashEntry* canonicalHashTable = scene->index.GetEntries();
	int noTotalEntries = scene->index.noTotalEntries;

	//TODO: if OpenMP standard is 3.1 or above, use OpenMP parallel for reduction clause with (max:maxVoxelPointX,...) -Greg (GitHub: Algomorph)
	for (int entryId = 0; entryId < noTotalEntries; entryId++) {

		const ITMHashEntry& currentHashEntry = canonicalHashTable[entryId];

		if (currentHashEntry.ptr < 0) continue;

		//position of the current entry in 3D space
		Vector3i currentHashBlockPositionVoxels = currentHashEntry.pos.toInt() * SDF_BLOCK_SIZE;
		Vector3i hashBlockLimitPositionVoxels = (currentHashEntry.pos.toInt() + Vector3i(1, 1, 1)) * SDF_BLOCK_SIZE;

		if (minVoxelPoint.x > currentHashBlockPositionVoxels.x) {
			minVoxelPoint.x = currentHashBlockPositionVoxels.x;
		}
		if (maxVoxelPoint.x < hashBlockLimitPositionVoxels.x) {
			maxVoxelPoint.x = hashBlockLimitPositionVoxels.x;
		}
		if (minVoxelPoint.y > currentHashBlockPositionVoxels.y) {
			minVoxelPoint.y = currentHashBlockPositionVoxels.y;
		}
		if (maxVoxelPoint.y < hashBlockLimitPositionVoxels.y) {
			maxVoxelPoint.y = hashBlockLimitPositionVoxels.y;
		}
		if (minVoxelPoint.z > currentHashBlockPositionVoxels.z) {
			minVoxelPoint.z = currentHashBlockPositionVoxels.z;
		}
		if (maxVoxelPoint.z < hashBlockLimitPositionVoxels.z) {
			maxVoxelPoint.z = hashBlockLimitPositionVoxels.z;
		}
	}
}

template<typename TVoxel, typename TIndex>
int ITMSceneStatisticsCalculator<TVoxel, TIndex>::ComputeHashedVoxelCount(ITMScene<TVoxel, TIndex>* scene) {
	int count = 0;

	TVoxel* voxelBlocks = scene->localVBA.GetVoxelBlocks();
	const ITMHashEntry* canonicalHashTable = scene->index.GetEntries();
	int noTotalEntries = scene->index.noTotalEntries;
#ifdef WITH_OPENMP
#pragma omp parallel for reduction(+:count)
#endif
	for (int entryId = 0; entryId < noTotalEntries; entryId++) {
		const ITMHashEntry& currentHashEntry = canonicalHashTable[entryId];
		if (currentHashEntry.ptr < 0) continue;
		count += SDF_BLOCK_SIZE3;
	}
	return count;
}
