// pluginMain.cpp

#include "Tensify.h"
#include <maya/MGlobal.h>
#include <maya/MFnPlugin.h>


MStatus initializePlugin(MObject mObject) {
    MFnPlugin pluginInstance(mObject, "Author", "1.0", "Any");
    MStatus status = pluginInstance.registerNode(
        "tensify",
        tensify::id,
        tensify::createInstance,
        tensify::initializeNode
    );

    if (!status) {
        MGlobal::displayError("Error: Failed to register tensify");
    }
    return status;
}

MStatus uninitializePlugin(MObject mObject) {
    MFnPlugin pluginInstance(mObject);
    MStatus status = pluginInstance.deregisterNode(tensify::id);

    if (!status) {
        MGlobal::displayError("Error: Failed to deregister tensify");
    }
    return status;
}
