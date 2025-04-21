import maya.cmds as cmds

# Create the Tensify node
sel = cmds.ls(sl=True)
if not sel:
    cmds.error("⚠️ Please select a mesh before running the script.")

obj = sel[0]
tensify = cmds.createNode("tensify")

# Connect Tensify to the selected mesh
shape = cmds.listRelatives(obj, s=True, f=True)[0]
input_plug = cmds.listConnections(shape + ".inMesh", s=True, d=False, plugs=True)[0]  
source, out_attr = input_plug.split(".", 1)  

ref = cmds.duplicate(obj, name=obj + "_tensifyRef")[0]
ref_shape = cmds.listRelatives(ref, s=True, f=True)[0]
cmds.connectAttr(ref_shape + ".worldMesh[0]", tensify + ".refInput")
cmds.setAttr(ref + ".visibility", l=False)
cmds.setAttr(ref + ".visibility", 0)

cmds.connectAttr(f"{source}.{out_attr}", f"{tensify}.input")
cmds.connectAttr(f"{tensify}.output", f"{shape}.inMesh", f=True)

cmds.setAttr(ref_shape + ".caching", 1)

cmds.connectAttr(tensify + ".tensifyShader", shape + ".displayColors", f=True)
cmds.setDrivenKeyframe(ref_shape + ".nodeState", cd=tensify + ".tensifyShader", dv=1, v=0)
cmds.setDrivenKeyframe(ref_shape + ".nodeState", cd=tensify + ".tensifyShader", dv=0, v=1)
