// Tensify.cpp

#include <maya/MFnMeshData.h>
#include <maya/MItMeshEdge.h>
#include <maya/MItMeshVertex.h>
#include <maya/MFnMesh.h>
#include <maya/MRampAttribute.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MFnDoubleArrayData.h>
#include "Tensify.h"
#include <vector>
#include <tuple>
#include <numeric>

// Node attributes
MTypeId tensify::id(0x10000000);
MObject tensify::refShp, tensify::shp, tensify::finalShp, tensify::tensifyShader, tensify::clrAttribute;
MObject tensify::stretchMap, tensify::compressionMap;

MString REF_INPUT("refInput");
MString INPUT("input");

namespace {
    // Preset color ramp values
    const int i1 = 0;
    const float p1 = 0;
    const MColor c1(0, 1, 0); // Green

    const int i2 = 1;
    const float p2 = 0.5;
    const MColor c2(0, 0, 0); // Black

    const int i3 = 2;
    const float p3 = 1;
    const MColor c3(1, 0, 0); // Red

    // Helper function to set a color ramp entry
    MStatus setColor(const MObject& activeObj, const MObject& colorObj, int num, float loc, const MColor& vClr, int mix) {
        MStatus status;
        MPlug colorPlg(activeObj, colorObj);
        colorPlg = colorPlg.elementByLogicalIndex(num, &status);
        colorPlg.child(0).setFloat(loc);
        auto componentPlg = colorPlg.child(1);
        componentPlg.child(0).setFloat(vClr.r);
        componentPlg.child(1).setFloat(vClr.g);
        componentPlg.child(2).setFloat(vClr.b);
        colorPlg.child(2).setInt(mix);
        return MS::kSuccess;
    }
}

// Node attribute initialization
MStatus tensify::initializeNode() {
    MFnTypedAttribute maker;

    // Input meshes
    refShp = maker.create(REF_INPUT, REF_INPUT, MFnMeshData::kMesh); maker.setStorable(true);
    shp = maker.create(INPUT, INPUT, MFnMeshData::kMesh); maker.setStorable(true);
    
    // Output mesh
    finalShp = maker.create("output", "output", MFnMeshData::kMesh); maker.setWritable(false); maker.setStorable(false);
    
    // Color ramp attribute
    clrAttribute = MRampAttribute::createColorRamp("tensifyColor", "tensifyColor");

    addAttribute(refShp);
    addAttribute(shp);
    addAttribute(finalShp);
    addAttribute(clrAttribute);

    // Define attribute dependencies
    attributeAffects(refShp, finalShp);
    attributeAffects(shp, finalShp);
    attributeAffects(clrAttribute, finalShp);

    MFnNumericAttribute nAttr;

    // Toggle for applying shader
    tensifyShader = nAttr.create("tensifyShader", "ts", MFnNumericData::kBoolean, false);
    nAttr.setKeyable(true);
    nAttr.setStorable(true);
    nAttr.setReadable(true);
    nAttr.setWritable(true);
    addAttribute(tensifyShader);

    
    MFnTypedAttribute tAttr;

    // Output: Stretch map values
    stretchMap = tAttr.create("stretchMap", "stretchMap", MFnData::kDoubleArray);
    tAttr.setStorable(false);
    tAttr.setWritable(false);
    tAttr.setReadable(true);
    addAttribute(stretchMap);
    attributeAffects(refShp, stretchMap);
    attributeAffects(shp, stretchMap);

    // Output: Compression map values
    compressionMap = tAttr.create("compressionMap", "compressionMap", MFnData::kDoubleArray);
    tAttr.setStorable(false);
    tAttr.setWritable(false);
    tAttr.setReadable(true);
    addAttribute(compressionMap);
    attributeAffects(refShp, compressionMap);
    attributeAffects(shp, compressionMap);

    return MStatus::kSuccess;
}

// Initialize the color ramp after construction
void tensify::postConstructor() {
    std::vector<std::tuple<int, float, MColor>> rampValues = {
        {i1, p1, c1},
        {i2, p2, c2},
        {i3, p3, c3}
    };
    for (const auto& ramp : rampValues) {
        setColor(thisMObject(), clrAttribute, std::get<0>(ramp), std::get<1>(ramp), std::get<2>(ramp), 1);
    }
}

// Compute average edge length per vertex
MDoubleArray tensify::calculateLen(const MDataHandle& dataSource) {
    MStatus status;
    MObject elementSet = dataSource.asMesh();
    MItMeshEdge spanTracker(elementSet, &status);
    MItMeshVertex pointTracker(elementSet, &status);
    MDoubleArray distanceRecords;

    int null;
    for (pointTracker.reset(); !pointTracker.isDone(); pointTracker.next()) {
        double totalTens = 0.0;
        MIntArray chain;
        pointTracker.getConnectedEdges(chain);
        int chainLength = chain.length();

        if (chainLength > 0) {
            std::vector<double> edgeLengths(chainLength, 0.0);
            for (int i = 0; i < chainLength; ++i) {
                spanTracker.setIndex(chain[i], null);
                spanTracker.getLength(edgeLengths[i]);
            }
            totalTens = std::accumulate(edgeLengths.begin(), edgeLengths.end(), 0.0) / chainLength;
        }
        distanceRecords.append(totalTens);
    }
    return distanceRecords;
}

// Main compute function 
MStatus tensify::compute(const MPlug& plug, MDataBlock& data) {
    MStatus status;

    // Handle only defined output plugs
    if (plug != finalShp && plug != stretchMap && plug != compressionMap) return MS::kUnknownParameter;

    auto refGeoData = data.inputValue(refShp, &status);
    auto meshState = data.inputValue(shp, &status);
    auto resultGeo = data.outputValue(finalShp, &status);
    MRampAttribute colorAttribute(thisMObject(), clrAttribute, &status);

    if (refDrt) refLen = calculateLen(refGeoData);
    if (shpDrt) shpLen = calculateLen(meshState);

    resultGeo.copy(meshState);
    resultGeo.set(meshState.asMesh());
    MFnMesh meshFn(resultGeo.asMesh(), &status);

    MColorArray shader(meshFn.numVertices(&status));
    MIntArray geoPoint(shader.length());

    MDoubleArray stretchArray, compressionArray;

    // Per-vertex deformation ratio and color mapping
    for (int i = 0; i < shader.length(); ++i) {
        double deformationRatio = (refLen.length() == shpLen.length()) ? ((refLen[i] - shpLen[i]) / refLen[i]) + 0.5 : 0.5;
        deformationRatio = std::max(0.0, std::min(1.0, deformationRatio));

        colorAttribute.getColorAtPosition(deformationRatio, shader[i], &status);
        geoPoint[i] = i;

        stretchArray.append(deformationRatio < 0.5 ? (0.5 - deformationRatio) * 2.0 : 0.0);
        compressionArray.append(deformationRatio > 0.5 ? (deformationRatio - 0.5) * 2.0 : 0.0);
    }

    // Apply vertex colors
    meshFn.setVertexColors(shader, geoPoint);

    
    if (plug == stretchMap || plug == finalShp) {
        MDataHandle stretchHandle = data.outputValue(stretchMap);
        stretchHandle.set(MFnDoubleArrayData().create(stretchArray));
    }

    if (plug == compressionMap || plug == finalShp) {
        MDataHandle compressionHandle = data.outputValue(compressionMap);
        compressionHandle.set(MFnDoubleArrayData().create(compressionArray));
    }

    data.setClean(plug);
    return MStatus::kSuccess;
}

// Track attribute dirtiness
MStatus tensify::preEvaluation(const MDGContext& context, const MEvaluationNode& evaluationNode) {
    if (evaluationNode.dirtyPlugExists(MPlug(thisMObject(), refShp))) {
        refDrt = true;
    }
    if (evaluationNode.dirtyPlugExists(MPlug(thisMObject(), shp))) {
        shpDrt = true;
    }
    return MStatus::kSuccess;
}

// Called when input plugs become dirty
MStatus tensify::setDependentsDirty(const MPlug& drtPlg, MPlugArray&) {
    this->refDrt = (drtPlg.partialName() == REF_INPUT);
    this->shpDrt = (drtPlg.partialName() == INPUT);
    return MStatus::kSuccess;
}
