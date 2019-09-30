import sys
from os.path import dirname, abspath

# allow absolute imports from inside the package
package_root = dirname(dirname(abspath(__file__)))
sys.path.insert(0, package_root)

from pypredict.lm_wrapper import *

