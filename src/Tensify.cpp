// Tensify.cpp

#include <maya/MFnMeshData.h>
#include <maya/MItMeshEdge.h>
#include <maya/MItMeshVertex.h>
#include <maya/MFnMesh.h>
#include <maya/MRampAttribute.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MFnNumericAttribute.h>
#include "Tensify.h"
#include <vector>
#include <tuple>
#include <numeric>

// Static members initialization
MTypeId tensify::id(0x10000000);

namespace {
    const int i1 = 0;
    const float p1 = 0;
    const MColor c1(0, 1, 0);
    const int i2 = 1;
    const float p2 = 0.5;
    const MColor c2(0, 0, 0);
    const int i3 = 2;
    const float p3 = 1;
    const MColor c3(1, 0, 0);
}

MObject tensify::refShp, tensify::shp, tensify::finalShp, tensify::tensifyShader, tensify::clrAttribute;
//bool tensify::refDrt = false, tensify::shpDrt = false;
//MDoubleArray tensify::refLen, tensify::shpLen;
MString REF_INPUT("refInput");
MString INPUT("input");

// Utility function for setting colors
namespace {
    MStatus setColor(const MObject& activeObj, const MObject& colorObj, int num, float loc, const MColor& vClr, int mix) {
        MStatus status;
        MPlug colorPlg(activeObj, colorObj);
        colorPlg = colorPlg.elementByLogicalIndex(num, &status);
        colorPlg.child(0).setFloat(loc);
        auto componentPlg = colorPlg.child(1);
        componentPlg.child(0).setFloat(vClr.r), componentPlg.child(1).setFloat(vClr.g), componentPlg.child(2).setFloat(vClr.b);
        colorPlg.child(2).setInt(mix);
        return MS::kSuccess;
    }
}

// Node attribute initialization
MStatus tensify::initializeNode() {
    MFnTypedAttribute maker;
    refShp = maker.create(REF_INPUT, REF_INPUT, MFnMeshData::kMesh); maker.setStorable(true);
    shp = maker.create(INPUT, INPUT, MFnMeshData::kMesh); maker.setStorable(true);
    finalShp = maker.create("output", "output", MFnMeshData::kMesh); maker.setWritable(false); maker.setStorable(false);
    clrAttribute = MRampAttribute::createColorRamp("tensifyColor", "tensifyColor");

    for (auto attr : { refShp, shp, finalShp, clrAttribute }) addAttribute(attr);
    for (auto attr : { refShp, shp, clrAttribute }) attributeAffects(attr, finalShp);

    // Add "tensifyShader"
    MFnNumericAttribute  nAttr;

    tensifyShader = nAttr.create("tensifyShader", "ts", MFnNumericData::kBoolean, false);
    nAttr.setKeyable(true);
    nAttr.setStorable(true);
    nAttr.setReadable(true);
    nAttr.setWritable(true);
    addAttribute(tensifyShader);

    return MStatus::kSuccess;
}

// Initializes the colors
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

// Function to calculate lengths
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

// Compute function
MStatus tensify::compute(const MPlug& plug, MDataBlock& data) {
    if (plug != finalShp) return MS::kUnknownParameter;
    MStatus status;
    auto refGeoData = data.inputValue(refShp, &status), meshState = data.inputValue(shp, &status);
    auto resultGeo = data.outputValue(finalShp, &status);
    MRampAttribute colorAttribute(thisMObject(), clrAttribute, &status);

    if (refDrt) refLen = calculateLen(refGeoData);
    if (shpDrt) shpLen = calculateLen(meshState);

    resultGeo.copy(meshState);
    resultGeo.set(meshState.asMesh());
    MFnMesh meshFn(resultGeo.asMesh(), &status);

    MColorArray shader(meshFn.numVertices(&status));
    MIntArray geoPoint(shader.length());

    for (int i = 0; i < shader.length(); ++i) {
        double deformationRatio = (refLen.length() == shpLen.length()) ? ((refLen[i] - shpLen[i]) / refLen[i]) + 0.5 : 0.5;
        colorAttribute.getColorAtPosition(deformationRatio, shader[i], &status);
        geoPoint[i] = i;
    }
    meshFn.setVertexColors(shader, geoPoint);
    data.setClean(plug);
    return MStatus::kSuccess;
}

MStatus tensify::preEvaluation(const MDGContext& context, const MEvaluationNode& evaluationNode)
{
    if (evaluationNode.dirtyPlugExists(MPlug(thisMObject(), refShp))) {
        refDrt = true;
    }
    if (evaluationNode.dirtyPlugExists(MPlug(thisMObject(), shp))) {
        shpDrt = true;
    }
    return MS::kSuccess;
}

// Mark dependents dirty when inputs change
MStatus tensify::setDependentsDirty(const MPlug& drtPlg, MPlugArray&) {
    this->refDrt = (drtPlg.partialName() == REF_INPUT);
    this->shpDrt = (drtPlg.partialName() == INPUT);
    return MStatus::kSuccess;
}

