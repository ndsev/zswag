import zserio
from os.path import dirname, abspath

working_dir = dirname(abspath(__file__))
zserio.generatePython(
    zsDir=working_dir,
    mainZsFile="calculator.zs",
    genDir=working_dir,
    extraArgs=["-withPythonProperties"])
