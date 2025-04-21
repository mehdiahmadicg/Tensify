// Tensify.h

#ifndef TENSIFY.H
#include <maya/MGlobal.h>
#include <maya/MPxNode.h>
#include <maya/MEvaluationNode.h>
#include <maya/MDGContext.h>

class tensify : public MPxNode {
public:
    static MTypeId id;
    MDoubleArray refLen;
    MDoubleArray shpLen;
    bool refDrt = true;
    bool shpDrt = true;
    static MObject clrAttribute;
    static MObject shp;
    static MObject finalShp;
    static MObject refShp;
    static MObject tensifyShader;

    static MStatus initializeNode();
    MStatus preEvaluation(const MDGContext& context, const MEvaluationNode& evaluationNode) override;
    static void* createInstance() { return new tensify(); }
    void postConstructor() override;


    MStatus compute(const MPlug& plug, MDataBlock& data) override;
    static MDoubleArray calculateLen(const MDataHandle& meshData);
    MStatus setDependentsDirty(const MPlug& dirtyPlug, MPlugArray& affectedPlugs) override;

};

#endif
