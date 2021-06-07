import sys
import os

#generate os-compatible path
def __generateImportPath(*path):
    joinedPath = path[0]
    for i in range(1, len(path)):
        subPath = path[i]
        if os.name == 'nt':
            joinedPath += '\\'
        else:
            joinedPath += '/'
        joinedPath += subPath
    return joinedPath

# add path to custom, gloabl object_map
sys.path.append(__generateImportPath('..', '..', 'common', 'shared_object_names'))

from objectmaphelper import *
# add custom, global object_map
from object_namespace import *

# generated by squish
gtMainWin_Explorer_GtExplorerDock = {"name": "Explorer", "type": "GtExplorerDock", "visible": 1, "window": gtMainWindow}
explorer_GtExplorerView = {"container": gtMainWin_Explorer_GtExplorerDock, "type": "GtExplorerView", "unnamed": 1, "visible": 1}
gtMainWin_QMenu = {"type": "QMenu", "unnamed": 1, "visible": 1, "window": gtMainWindow}
delete_from_Session_QCheckBox = {"type": "QCheckBox", "unnamed": 1, "visible": 1, "window": gtConfirmDeleteProjectDialog}
