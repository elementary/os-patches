import autopilot.introspection.gtk
import os
import pyatspi.registry
import pyatspi.utils
import time
import unity.tests

def print_accessible(root, level=0):
    print level * ' ', root

    for node in root:
        print_accessible(node, level + 1)

def get_accessible_with_name_and_role(root, name, role):
    is_accessible = lambda a: a.name == name and a.get_role_name() == role
    return pyatspi.utils.findDescendant(root, is_accessible, True);

def get_panel_accessible(root):
    return get_accessible_with_name_and_role(root, 'unity-panel-service', 'application')

def is_indicator_accessible(root):
    return root.get_role_name() == 'panel' and \
           len(root) == 1 and \
           root[0].get_role_name() == 'image' and \
           len(root[0]) == 1 and \
           root[0][0].get_role_name() == 'menu' and \
           len(root[0][0]) > 3 and \
           root[0][0][-3].name == 'Character Map' and \
           root[0][0][-3].get_role_name() == 'check menu item' and \
           root[0][0][-2].name == 'Keyboard Layout Chart' and \
           root[0][0][-2].get_role_name() == 'check menu item' and \
           root[0][0][-1].name == 'Text Entry Settings...' and \
           root[0][0][-1].get_role_name() == 'check menu item'

def get_indicator_accessible(root):
    return pyatspi.utils.findDescendant(root, is_indicator_accessible, True)

def get_accessible_index(root, node):
    for i in xrange(len(root)):
        if root[i] == node:
            return i

    return -1

class IndicatorKeyboardTestCase(unity.tests.UnityTestCase):

    def setUp(self):
        super(IndicatorKeyboardTestCase, self).setUp()

        registry = pyatspi.registry.Registry()
        desktop = registry.getDesktop(0)
        panel = get_panel_accessible(desktop)
        self.indicator = get_indicator_accessible(panel)

        # This is needed on systems other than the EN locale
        os.putenv("LC_ALL", "C")
        self.addCleanup(os.unsetenv, "LC_ALL")

    def test_indicator(self):
        print_accessible(self.indicator)
