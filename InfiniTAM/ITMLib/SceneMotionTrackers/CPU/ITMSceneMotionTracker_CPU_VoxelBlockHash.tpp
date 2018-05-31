//  ================================================================
//  Created by Gregory Kramida on 10/18/17.
//  Copyright (c) 2017-2025 Gregory Kramida
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



//stdlib
#include <cmath>
#include <iomanip>
#include <unordered_set>
#include <chrono>

//_DEBUG -- OpenCV
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>
#include <unordered_map>
#include <opencv/cv.hpp>

//local
#include "ITMSceneMotionTracker_CPU.h"

#include "../Shared/ITMSceneMotionTracker_Shared_Old.h"
#include "../../Utils/Analytics/ITMSceneStatisticsCalculator.h"
#include "../../Objects/Scene/ITMTrilinearDistribution.h"
#include "../../Objects/Scene/ITMSceneManipulation.h"
#include "../../Utils/ITMLibSettings.h"
#include "../../Objects/Scene/ITMSceneTraversal.h"


using namespace ITMLib;

// region ================================ CONSTRUCTORS AND DESTRUCTORS ================================================


template<typename TVoxelCanonical, typename TVoxelLive>
ITMSceneMotionTracker_CPU<TVoxelCanonical, TVoxelLive, ITMVoxelBlockHash>::ITMSceneMotionTracker_CPU(
		const ITMLibSettings* settings, ITMDynamicFusionLogger<TVoxelCanonical, TVoxelLive, ITMVoxelBlockHash>& logger)
		:ITMSceneMotionTracker<TVoxelCanonical, TVoxelLive, ITMVoxelBlockHash>(settings, logger),
		 calculateGradientFunctor(this->parameters, this->switches, logger) {

};
// endregion ============================== END CONSTRUCTORS AND DESTRUCTORS============================================

// region ===================================== HOUSEKEEPING ===========================================================

template<typename TVoxel>
struct WarpClearFunctor {
	static void run(TVoxel& voxel) {
		voxel.warp = Vector3f(0.0f);
	}
};

template<typename TVoxelCanonical, typename TVoxelLive>
void ITMSceneMotionTracker_CPU<TVoxelCanonical, TVoxelLive, ITMVoxelBlockHash>::ResetWarps(
		ITMScene<TVoxelCanonical, ITMVoxelBlockHash>* canonicalScene) {
	StaticVoxelTraversal_CPU<WarpClearFunctor<TVoxelCanonical>>(canonicalScene);
};

// endregion ===========================================================================================================

//_DEBUG
template<typename TVoxel, typename TIndex>
inline static void PrintSceneStatistics(
		ITMScene<TVoxel, TIndex>* scene,
		std::string description) {
	ITMSceneStatisticsCalculator<TVoxel, TIndex> calculator;
	std::cout << green << "=== Stats for scene '" << description << "' ===" << reset << std::endl;
	std::cout << "    Total voxel count: " << calculator.ComputeAllocatedVoxelCount(scene) << std::endl;
	std::cout << "    NonTruncated voxel count: " << calculator.ComputeNonTruncatedVoxelCount(scene) << std::endl;
	std::cout << "    +1.0 voxel count: " << calculator.ComputeVoxelWithValueCount(scene,1.0f)  << std::endl;
	std::vector<int> allocatedHashes = calculator.GetFilledHashBlockIds(scene);
	std::cout << "    Allocated hash count: " << allocatedHashes.size() << std::endl;
	std::cout << "    NonTruncated SDF sum: " << calculator.ComputeNonTruncatedVoxelAbsSdfSum(scene) << std::endl;
	std::cout << "    Truncated SDF sum: " << calculator.ComputeTruncatedVoxelAbsSdfSum(scene) << std::endl;

};

// region ===================================== CALCULATE GRADIENT SMOOTHING ===========================================

template<typename TVoxelCanonical>
struct ClearOutGradientStaticFunctor {
	static void run(TVoxelCanonical& voxel) {
		voxel.gradient0 = Vector3f(0.0f);
		voxel.gradient1 = Vector3f(0.0f);
	}
};

template<typename TVoxelCanonical, typename TVoxelLive>
void
ITMSceneMotionTracker_CPU<TVoxelCanonical, TVoxelLive, ITMVoxelBlockHash>::CalculateWarpGradient(
		ITMScene<TVoxelCanonical, ITMVoxelBlockHash>* canonicalScene,
		ITMScene<TVoxelLive, ITMVoxelBlockHash>* liveScene, bool hasFocusCoordinates,
		const Vector3i& focusCoordinates, int sourceFieldIndex, bool restrictZTrackingForDebugging) {

	StaticVoxelTraversal_CPU<ClearOutGradientStaticFunctor<TVoxelCanonical>>(canonicalScene);
	hashManager.AllocateCanonicalFromLive(canonicalScene, liveScene);
	calculateGradientFunctor.PrepareForOptimization(liveScene, canonicalScene, sourceFieldIndex, hasFocusCoordinates,
	                                                focusCoordinates, restrictZTrackingForDebugging);

	DualVoxelPositionTraversal_CPU(liveScene, canonicalScene, calculateGradientFunctor);

	calculateGradientFunctor.FinalizePrintAndRecordStatistics();
}

// endregion ===========================================================================================================
// region ========================================== SOBOLEV GRADIENT SMOOTHING ========================================

enum TraversalDirection : int {
	X = 0, Y = 1, Z = 2
};


template<typename TVoxelCanonical, typename TVoxelLive, TraversalDirection TDirection>
struct GradientSmoothingPassFunctor {
	GradientSmoothingPassFunctor(ITMScene<TVoxelCanonical, ITMVoxelBlockHash>* canonicalScene) :
			canonicalScene(canonicalScene),
			canonicalVoxels(canonicalScene->localVBA.GetVoxelBlocks()),
			canoincalHashEntries(canonicalScene->index.GetEntries()),
			canonicalCache(){}

	void operator()(TVoxelCanonical& voxel, Vector3i position) {
		int vmIndex;

		const int directionIndex = (int) TDirection;

		Vector3i receptiveVoxelPosition = position;
		receptiveVoxelPosition[directionIndex] -= (sobolevFilterSize / 2);
		Vector3f smoothedGradient(0.0f);

		for (int iVoxel = 0; iVoxel < sobolevFilterSize; iVoxel++, receptiveVoxelPosition[directionIndex]++) {
			const TVoxelCanonical& receptiveVoxel = readVoxel(canonicalVoxels, canoincalHashEntries,
			                                                  receptiveVoxelPosition, vmIndex, canonicalCache);
			smoothedGradient += sobolevFilter1D[iVoxel] * GetGradient(receptiveVoxel);
		}
		SetGradient(voxel, smoothedGradient);
	}

private:
	Vector3f GetGradient(const TVoxelCanonical& voxel) const {
		switch (TDirection) {
			case X:
				return voxel.gradient0;
			case Y:
				return voxel.gradient1;
			case Z:
				return voxel.gradient0;
		}
	}

	void SetGradient(TVoxelCanonical& voxel, const Vector3f gradient) const {
		switch (TDirection) {
			case X:
				voxel.gradient1 = gradient;
				return;
			case Y:
				voxel.gradient0 = gradient;
				return;
			case Z:
				voxel.gradient1 = gradient;
				return;
		}
	}

	ITMScene<TVoxelCanonical, ITMVoxelBlockHash>* canonicalScene;
	TVoxelCanonical* canonicalVoxels;
	ITMHashEntry* canoincalHashEntries;
	typename ITMVoxelBlockHash::IndexCache canonicalCache;

	static const int sobolevFilterSize;
	static const float sobolevFilter1D[];
};

template<typename TVoxelCanonical, typename TVoxelLive, TraversalDirection TDirection>
const int GradientSmoothingPassFunctor<TVoxelCanonical, TVoxelLive, TDirection>::sobolevFilterSize = 7;
template<typename TVoxelCanonical, typename TVoxelLive, TraversalDirection TDirection>
const float GradientSmoothingPassFunctor<TVoxelCanonical, TVoxelLive, TDirection>::sobolevFilter1D[] = {
		2.995861099047703036e-04f,
		4.410932423926419363e-03f,
		6.571314272194948847e-02f,
		9.956527876693953560e-01f,
		6.571314272194946071e-02f,
		4.410932423926422832e-03f,
		2.995861099045313996e-04f};


template<typename TVoxelCanonical, typename TVoxelLive>
void ITMSceneMotionTracker_CPU<TVoxelCanonical, TVoxelLive, ITMVoxelBlockHash>::SmoothWarpGradient(
		ITMScene<TVoxelCanonical, ITMVoxelBlockHash>* canonicalScene) {

	if (this->switches.enableGradientSmoothing) {
		GradientSmoothingPassFunctor<TVoxelCanonical, TVoxelLive, X> passFunctorX(canonicalScene);
		GradientSmoothingPassFunctor<TVoxelCanonical, TVoxelLive, Y> passFunctorY(canonicalScene);
		GradientSmoothingPassFunctor<TVoxelCanonical, TVoxelLive, Z> passFunctorZ(canonicalScene);

		VoxelPositionTraversal_CPU(canonicalScene, passFunctorX);
		VoxelPositionTraversal_CPU(canonicalScene, passFunctorY);
		VoxelPositionTraversal_CPU(canonicalScene, passFunctorZ);
	}
}

// endregion ===========================================================================================================

template<typename TVoxelLive, typename TVoxelCanonical>
struct WarpUpdateFunctor{
	WarpUpdateFunctor(float learningRate, bool gradientSmoothingEnabled):
			learningRate(learningRate),gradientSmoothingEnabled(gradientSmoothingEnabled),
			maxWarpLength(0.0f),maxWarpUpdateLength(0.0f), maxWarpPosition(0),maxWarpUpdatePosition(0){}

	void operator()(TVoxelLive& liveVoxel, TVoxelCanonical& canonicalVoxel, const Vector3i& position){
		Vector3f warpUpdate = -learningRate * (gradientSmoothingEnabled ?
		                                       canonicalVoxel.gradient1 : canonicalVoxel.gradient0);

		canonicalVoxel.gradient0 = warpUpdate;
		canonicalVoxel.warp += warpUpdate;

		// update stats
		float warpLength = ORUtils::length(canonicalVoxel.warp);
		float warpUpdateLength = ORUtils::length(warpUpdate);
		if (warpLength > maxWarpLength) {
			maxWarpLength = warpLength;
			maxWarpPosition = position;
		}
		if (warpUpdateLength > maxWarpUpdateLength) {
			maxWarpUpdateLength = warpUpdateLength;
			maxWarpUpdatePosition = position;
		}
	}
	float maxWarpLength;
	float maxWarpUpdateLength;
	Vector3i maxWarpPosition;
	Vector3i maxWarpUpdatePosition;

	void PrintWarp(){
		std::cout << green << "Max warp: [" << maxWarpLength << " at " << maxWarpPosition << "] Max update: ["
		          << maxWarpUpdateLength << " at " << maxWarpUpdatePosition << "]." << reset << std::endl;
	}
private:
	const float learningRate;
	const bool gradientSmoothingEnabled;
};

template<typename TVoxelLive, typename TVoxelCanonical>
struct WarpHistogramFunctor{
	WarpHistogramFunctor(float maxWarpLength, float maxWarpUpdateLength):
			maxWarpLength(maxWarpLength),maxWarpUpdateLength(maxWarpUpdateLength){}
	static const int histBinCount = 10;
	void operator()(TVoxelLive& liveVoxel, TVoxelCanonical& canonicalVoxel){
		float warpLength = ORUtils::length(canonicalVoxel.warp);
		float warpUpdateLength = ORUtils::length(canonicalVoxel.gradient0);
		const int histBinCount = WarpHistogramFunctor<TVoxelLive,TVoxelCanonical>::histBinCount;
		int binIdx = 0;
		if (maxWarpLength > 0) {
			binIdx = std::min(histBinCount - 1, (int) (warpLength * histBinCount / maxWarpLength));
		}
		warpBins[binIdx]++;
		if (maxWarpUpdateLength > 0) {
			binIdx = std::min(histBinCount - 1,
			                  (int) (warpUpdateLength * histBinCount / maxWarpUpdateLength));
		}
		updateBins[binIdx]++;
	}

	int warpBins[histBinCount];
	int updateBins[histBinCount];

	void PrintHistogram(){
		std::cout << "  Warp length histogram: ";
		for (int iBin = 0; iBin < histBinCount; iBin++) {
			std::cout << std::setfill(' ') << std::setw(7) << warpBins[iBin] << "  ";
		}
		std::cout << std::endl;
		std::cout << "Update length histogram: ";
		for (int iBin = 0; iBin < histBinCount; iBin++) {
			std::cout << std::setfill(' ') << std::setw(7) << updateBins[iBin] << "  ";
		}
		std::cout << std::endl;
	}
private:
	const float maxWarpLength;
	const float maxWarpUpdateLength;

	// <20%, 40%, 60%, 80%, 100%
};



template<typename TVoxelCanonical, typename TVoxelLive>
float ITMSceneMotionTracker_CPU<TVoxelCanonical, TVoxelLive, ITMVoxelBlockHash>::UpdateWarps(
		ITMScene<TVoxelCanonical, ITMVoxelBlockHash>* canonicalScene,
		ITMScene<TVoxelLive, ITMVoxelBlockHash>* liveScene) {

	WarpUpdateFunctor<TVoxelLive, TVoxelCanonical>
			warpUpdateFunctor(this->parameters.gradientDescentLearningRate,this->switches.enableGradientSmoothing);

	DualVoxelPositionTraversal_CPU(liveScene,canonicalScene,warpUpdateFunctor);

	WarpHistogramFunctor<TVoxelLive, TVoxelCanonical>
	        warpHistogramFunctor(warpUpdateFunctor.maxWarpLength,warpUpdateFunctor.maxWarpUpdateLength);
	DualVoxelTraversal_CPU(liveScene,canonicalScene,warpHistogramFunctor);

	warpHistogramFunctor.PrintHistogram();
	warpUpdateFunctor.PrintWarp();

	return warpUpdateFunctor.maxWarpUpdateLength;
}


//endregion ============================================================================================================