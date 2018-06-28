//  ================================================================
//  Created by Gregory Kramida on 1/24/18.
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
#pragma once

#include "ITMScene3DSliceVisualizer.h"
#include "../../ITMLibDefines.h"
#include "../Collections/ITM3DNestedMap.h"

namespace ITMLib{
class ITMCanonicalScene3DSliceVisualizer_Deprecated : public ITMScene3DSliceVisualizer<ITMVoxelCanonical, ITMVoxelIndex> {
public:
	// region ==================== CONSTANTS ==================================================

	static const std::array<double, 3> sliceExtremaMarkerColor;
	// endregion
	// region ==================== CONSTRUCTORS / DESTRUCTORS =================================

	ITMCanonicalScene3DSliceVisualizer_Deprecated(const std::array<double, 4>& positiveTruncatedNonInterestVoxelColor,
	                   const std::array<double, 4>& positiveNonTruncatedNonInterestVoxelColor,
	                   const std::array<double, 4>& negativeNonTruncatedNonInterestVoxelColor,
	                   const std::array<double, 4>& negativeTruncatedNonInterestVoxelColor,
	                   const std::array<double, 4>& unknownNonInterestVoxelColor,
	                   const std::array<double, 4>& positiveInterestVoxelColor,
	                   const std::array<double, 4>& negativeInterestVoxelColor,
	                   const std::array<double, 4>& highlightVoxelColor,
	                   const std::array<double, 3>& hashBlockEdgeColor, int frameIx);

	// endregion
	// region ==================== MEMBER FUNCTIONS ===========================================

	void UpdatePointPositionsFromBuffer(void* buffer);
	void UpdateInterestRegionsFromBuffers(void* buffer);

	// *** getter/setter/printer ***
	Vector3d GetHighlightPosition(int hash, int locId) const;
	std::vector<Vector3d> GetHighlightNeighborPositions(int hash, int locId) const;
	void SetInterestRegionInfo(std::vector<int> interestRegionHashes,
	                           ITM3DNestedMapOfArrays<ITMHighlightIterationInfo> highlights);
	void SetFrameIndex(int frameIx);
	vtkSmartPointer<vtkActor>& GetInterestVoxelActor();
	vtkSmartPointer<vtkActor>& GetWarplessVoxelActor();
	vtkSmartPointer<vtkActor>& GetSelectionVoxelActor();
	vtkSmartPointer<vtkActor>& GetSliceSelectionActor(int index);
	vtkSmartPointer<vtkActor>& GetSlicePreviewActor();
	bool GetWarpEnabled() const;
	void PrintHighlightIndexes();
	bool GetSliceCoordinatesAreSet() const;
	void GetSliceCoordinates(Vector3i& coord0, Vector3i& coord1) const;
	Vector3i GetSelectedVoxelCoordinates() const;

	// *** setup ***
	void PrepareInterestRegions(vtkAlgorithmOutput* voxelSourceGeometry);
	void PrepareWarplessVoxels(vtkAlgorithmOutput* voxelSourceGeometry);


	// *** modify state ***
	void ToggleScaleMode() override;
	void ToggleWarpEnabled();
	void SelectOrDeselectVoxel(vtkIdType pointId, bool highlightOn,
		                           const ITMScene<ITMVoxelCanonical, ITMVoxelIndex>* scene);
	void SetSliceSelection(vtkIdType pointId, bool& continueSliceSelection,
	                       const ITMScene<ITMVoxelCanonical, ITMVoxelIndex>* scene);
	void ClearSliceSelection();

	// endregion
protected:
	// *** setup ***
	void BuildVoxelAndHashBlockPolydataFromScene() override;
	void PreparePipeline() override;
	vtkSmartPointer<vtkPoints> initialNonInterestPoints;
	vtkSmartPointer<vtkPoints> initialInterestPoints;
private:
	// region =============== MEMBER FUNCTIONS =========================================================================

	void PrintVoxelInfromation(vtkIdType pointId, const ITMScene<ITMVoxelCanonical, ITMVoxelIndex>* scene);
	void RetrieveInitialCoordinates(vtkIdType pointId, int initialCoordinates[3], double vizCoordinates[3]) const;
	// endregion
	// region =============== MEMBER VARIABLES =========================================================================

	//frame of the warp
	int frameIx;

	// ** colors **
	std::array<double, 4> negativeInterestVoxelColor;
	std::array<double, 4> positiveInterestVoxelColor;
	std::array<double, 4> highlightVoxelColor;

	// ** state **
	bool interestRegionHashesAreSet = false;
	bool preparePipelineWasCalled = false;
	bool firstSliceBoundSelected = false;
	bool warpEnabled = true;
	vtkIdType selectedPointId = -1;

	// ** interest region info **
	std::vector<int> interestRegionHashes;
	std::set<int> interestRegionHashSet;
	int totalVoxelCount = 0;//includes both interest regions and the rest
	std::vector<std::tuple<int, int>> interestRegionRanges;

	ITM3DNestedMap<int> highlightIndexes;
	ITM3DNestedMap<std::tuple<int, int>> highlightByNeighbor;
	ITM3DNestedMapOfArrays<int> highlightNeighborIndexes;

	// ** individual voxels **
	vtkSmartPointer<vtkPolyData> interestVoxelPolydata;
	vtkSmartPointer<vtkLookupTable> interestVoxelColorLookupTable;
	vtkSmartPointer<vtkGlyph3DMapper> interestVoxelMapper;
	vtkSmartPointer<vtkActor> interestVoxelActor;
	vtkSmartPointer<vtkPolyData> warplessVoxelPolydata;
	vtkSmartPointer<vtkGlyph3DMapper> warplessVoxelMapper;
	vtkSmartPointer<vtkActor> warplessVoxelActor;

	// ** interaction **
	vtkSmartPointer<vtkActor> selectedVoxelActor;
	vtkSmartPointer<vtkActor> selectedSliceExtrema[2];
	vtkSmartPointer<vtkActor> selectedSlicePreview;
	Vector3i selectedSliceExtremaCoordinates[2];
	bool haveSliceCoordinates = false;
	//endregion
};
}//namespace ITMLib