//  ================================================================
//  Created by Gregory Kramida on 1/22/18.
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
//VTK
#include <vtkActor.h>
#include <vtkFloatArray.h>
#include <vtkPointData.h>
#include <vtkGlyph3DMapper.h>
#include <vtkBox.h>
#include <vtkProperty.h>
#include <vtkLookupTable.h>
#include <vtkPolyDataMapper.h>

//Local
#include "SDFSceneVizPipe.h"
#include "VizPipeShared.h"

//ITMLib
#include "../../ITMLib/Utils/ITMLibSettings.h"
#include "../../ITMLib/Utils/ITMSceneStatisticsCalculator.h"
#include "SDFVizGlobalDefines.h"


template<typename TVoxel, typename TIndex>
const char* SDFSceneVizPipe<TVoxel, TIndex>::colorPointAttributeName = "color";
template<typename TVoxel, typename TIndex>
const char* SDFSceneVizPipe<TVoxel, TIndex>::scalePointAttributeName = "scale";
template<typename TVoxel, typename TIndex>
const char* SDFSceneVizPipe<TVoxel, TIndex>::alternativeScalePointAttributeName = "alternative_scale";

template<typename TVoxel, typename TIndex>
SDFSceneVizPipe<TVoxel, TIndex>::SDFSceneVizPipe(std::array<double, 4> negativeVoxelColor, std::array<double, 4> positiveVoxelColor,
                                                 std::array<double, 4> highlightVoxelColor, std::array<double, 3> hashBlockEdgeColor)
		:
		voxelPolydata(vtkSmartPointer<vtkPolyData>::New()),
		voxelColorLookupTable(vtkSmartPointer<vtkLookupTable>::New()),
		voxelMapper(vtkSmartPointer<vtkGlyph3DMapper>::New()),
		voxelActor(vtkSmartPointer<vtkActor>::New()),

		hashBlockGrid(vtkSmartPointer<vtkPolyData>::New()),
		hashBlockActor(vtkSmartPointer<vtkActor>::New()),
		hashBlockMapper(vtkSmartPointer<vtkGlyph3DMapper>::New()),

		negativeVoxelColor(negativeVoxelColor),
		positiveVoxelColor(positiveVoxelColor),
		highlightVoxelColor(highlightVoxelColor),
		hashBlockEdgeColor(hashBlockEdgeColor),
        scaleMode(VOXEL_SCALE_HIDE_UNKNOWNS){
	auto* settings = new ITMLibSettings();
	scene = new ITMScene<TVoxel, TIndex>(
			&settings->sceneParams, settings->swappingMode ==
			                        ITMLibSettings::SWAPPINGMODE_ENABLED, settings->GetMemoryType());
	// Create the color maps
	SetUpSDFColorLookupTable(voxelColorLookupTable, negativeVoxelColor.data(), positiveVoxelColor.data(), highlightVoxelColor.data());
	delete settings;
}

template<typename TVoxel, typename TIndex>
SDFSceneVizPipe<TVoxel, TIndex>::~SDFSceneVizPipe() {
	delete scene;
}

template<typename TVoxel, typename TIndex>
ITMScene<TVoxel, TIndex>* SDFSceneVizPipe<TVoxel, TIndex>::GetScene() {
	return scene;
}

template<typename TVoxel, typename TIndex>
void SDFSceneVizPipe<TVoxel, TIndex>::PreparePointsForRendering() {
	vtkSmartPointer<vtkPoints> points = vtkSmartPointer<vtkPoints>::New();
	vtkSmartPointer<vtkPoints> hashBlockPoints = vtkSmartPointer<vtkPoints>::New();

	//holds color for each voxel
	vtkSmartPointer<vtkIntArray> colorAttribute = vtkSmartPointer<vtkIntArray>::New();
	colorAttribute->SetName(colorPointAttributeName);

	//holds scale of each voxel
	vtkSmartPointer<vtkFloatArray> scaleAttribute = vtkSmartPointer<vtkFloatArray>::New();
	scaleAttribute->SetName(scalePointAttributeName);

	//holds alternative scale of each voxel (showing -1 value voxels)
	vtkSmartPointer<vtkFloatArray> alternativeScaleAttribute = vtkSmartPointer<vtkFloatArray>::New();
	alternativeScaleAttribute->SetName(alternativeScalePointAttributeName);

	TVoxel* voxelBlocks = scene->localVBA.GetVoxelBlocks();
	const ITMHashEntry* canonicalHashTable = scene->index.GetEntries();
	int noTotalEntries = scene->index.noTotalEntries;

	for (int entryId = 0; entryId < noTotalEntries; entryId++) {
		const ITMHashEntry& currentHashEntry = canonicalHashTable[entryId];

		//skip unfilled hash
		if (currentHashEntry.ptr < 0) continue;

		//position of the current entry in 3D space
		Vector3i currentBlockPositionVoxels = currentHashEntry.pos.toInt() * SDF_BLOCK_SIZE;

		const double centerOffset = -0.5;

		//draw hash block
		hashBlockPoints->InsertNextPoint((currentBlockPositionVoxels.x + centerOffset),
		                                 -(currentBlockPositionVoxels.y + centerOffset),
		                                 -(currentBlockPositionVoxels.z + centerOffset));

		TVoxel* localVoxelBlock = &(voxelBlocks[currentHashEntry.ptr * (SDF_BLOCK_SIZE3)]);

		for (int z = 0; z < SDF_BLOCK_SIZE; z++) {
			for (int y = 0; y < SDF_BLOCK_SIZE; y++) {
				for (int x = 0; x < SDF_BLOCK_SIZE; x++) {
					ComputeVoxelAttributes(currentBlockPositionVoxels, x, y, z, localVoxelBlock, points, scaleAttribute,
					                       alternativeScaleAttribute, colorAttribute);
				}
			}
		}
	}

	std::cout << "Scene voxel count: " << points->GetNumberOfPoints() << std::endl;
	std::cout << "Allocated hash block count: " << hashBlockPoints->GetNumberOfPoints() << std::endl;

	//Points pipeline
	voxelPolydata->SetPoints(points);
	voxelPolydata->GetPointData()->AddArray(colorAttribute);
	voxelPolydata->GetPointData()->AddArray(scaleAttribute);
	voxelPolydata->GetPointData()->AddArray(alternativeScaleAttribute);
	voxelPolydata->GetPointData()->SetActiveScalars(colorPointAttributeName);

	hashBlockGrid->SetPoints(hashBlockPoints);
}

template<typename TVoxel, typename TIndex>
void SDFSceneVizPipe<TVoxel, TIndex>::PreparePipeline(vtkAlgorithmOutput* voxelSourceGeometry,
                                                      vtkAlgorithmOutput* hashBlockSourceGeometry) {
	PreparePointsForRendering();

	// scene statistics
	ITMSceneStatisticsCalculator<TVoxel, TIndex> statCalculator;
	statCalculator.ComputeVoxelBounds(scene, minPoint, maxPoint);
	std::cout << "Voxel ranges ( min x,y,z; max x,y,z): " << minPoint << "; " << maxPoint << std::endl;

	// set up hash block mapper
	SetUpSceneHashBlockMapper(hashBlockSourceGeometry, hashBlockMapper, hashBlockGrid);

	// set up voxel mapper
	SetUpSceneVoxelMapper(voxelSourceGeometry, voxelMapper, voxelColorLookupTable, voxelPolydata);
	scaleMode = VOXEL_SCALE_HIDE_UNKNOWNS;// reset scale mode

	// set up voxel actor
	voxelActor->SetMapper(voxelMapper);
	voxelActor->GetProperty()->SetPointSize(20.0f);
	voxelActor->VisibilityOn();

	// set up hash block actor
	hashBlockActor->SetMapper(hashBlockMapper);
	hashBlockActor->GetProperty()->SetRepresentationToWireframe();
	hashBlockActor->GetProperty()->SetColor(hashBlockEdgeColor.data());
	hashBlockActor->VisibilityOff();
}

template<typename TVoxel, typename TIndex>
void SDFSceneVizPipe<TVoxel, TIndex>::SetUpSceneHashBlockMapper(vtkAlgorithmOutput* sourceOutput,
                                                                vtkSmartPointer<vtkGlyph3DMapper>& mapper,
                                                                vtkSmartPointer<vtkPolyData>& pointsPolydata) {
	mapper->SetInputData(pointsPolydata);
	mapper->SetSourceConnection(sourceOutput);
	mapper->ScalarVisibilityOff();
	mapper->ScalingOff();
	mapper->SetScaleFactor(1.0);
}

template<typename TVoxel, typename TIndex>
void SDFSceneVizPipe<TVoxel, TIndex>::SetUpSDFColorLookupTable(vtkSmartPointer<vtkLookupTable>& table,
                                                              const double* rgbaFirstColor,
                                                              const double* rgbaSecondColor,
                                                              const double* rgbaHighlightColor) {
	table->SetTableRange(0.0, 3.0);
	table->SetNumberOfTableValues(3);
	table->SetNumberOfColors(3);
	table->SetTableValue(NEGATIVE_SDF_COLOR_INDEX, rgbaFirstColor);
	table->SetTableValue(POSITIVE_SDF_COLOR_INDEX, rgbaSecondColor);
	table->SetTableValue(HIGHLIGHT_SDF_COLOR_INDEX, rgbaHighlightColor);
	table->SetNanColor(0.4, 0.7, 0.1, 1.0);
}

//GPU glyph version with filtering
template<typename TVoxel, typename TIndex>
void SDFSceneVizPipe<TVoxel, TIndex>::SetUpSceneVoxelMapper(vtkAlgorithmOutput* sourceOutput,
                                                            vtkSmartPointer<vtkGlyph3DMapper>& mapper,
                                                            vtkSmartPointer<vtkLookupTable>& table,
                                                            vtkSmartPointer<vtkExtractPolyDataGeometry> extractor) {
	mapper->SetInputConnection(extractor->GetOutputPort());
	mapper->SetSourceConnection(sourceOutput);
	mapper->SetLookupTable(table);
	mapper->ScalingOn();
	mapper->SetScaleModeToScaleByMagnitude();
	mapper->SetScaleArray(scalePointAttributeName);
	mapper->ScalarVisibilityOn();
	mapper->SetScalarModeToUsePointData();
	mapper->SetColorModeToMapScalars();
	mapper->SetScalarRange(0.0, 3.0);
	mapper->InterpolateScalarsBeforeMappingOff();
	mapper->Update();
}

//GPU glyph version w/o filtering
template<typename TVoxel, typename TIndex>
void SDFSceneVizPipe<TVoxel, TIndex>::SetUpSceneVoxelMapper(
		vtkAlgorithmOutput* sourceOutput,
		vtkSmartPointer<vtkGlyph3DMapper>& mapper,
		vtkSmartPointer<vtkLookupTable>& table,
		vtkSmartPointer<vtkPolyData>& pointsPolydata) {
	mapper->SetInputData(pointsPolydata);
	mapper->SetSourceConnection(sourceOutput);
	mapper->SetLookupTable(table);
	mapper->ScalingOn();
	mapper->SetScaleArray(scalePointAttributeName);
	mapper->SetScaleModeToScaleByMagnitude();
	mapper->ScalarVisibilityOn();
	mapper->SetScalarModeToUsePointData();
	mapper->SetColorModeToMapScalars();
	mapper->SetScalarRange(0.0, 3.0);
	mapper->InterpolateScalarsBeforeMappingOff();
	mapper->Update();
}

template<typename TVoxel, typename TIndex>
vtkSmartPointer<vtkActor>& SDFSceneVizPipe<TVoxel, TIndex>::GetVoxelActor() {
	return voxelActor;
}

template<typename TVoxel, typename TIndex>
vtkSmartPointer<vtkActor>& SDFSceneVizPipe<TVoxel, TIndex>::GetHashBlockActor() {
	return hashBlockActor;
}

template<typename TVoxel, typename TIndex>
void SDFSceneVizPipe<TVoxel, TIndex>::ToggleScaleMode() {
	if(scaleMode == VoxelScaleMode::VOXEL_SCALE_HIDE_UNKNOWNS){
		scaleMode = VoxelScaleMode::VOXEL_SCALE_SHOW_UNKNOWNS;
		voxelMapper->SetScaleArray(alternativeScalePointAttributeName);
	}else{
		scaleMode = VoxelScaleMode::VOXEL_SCALE_HIDE_UNKNOWNS;
		voxelMapper->SetScaleArray(scalePointAttributeName);
	}
}

template<typename TVoxel, typename TIndex>
VoxelScaleMode SDFSceneVizPipe<TVoxel, TIndex>::GetCurrentScaleMode() {
	return this->scaleMode;
}


