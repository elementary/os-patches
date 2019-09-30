import sys
from os.path import dirname, abspath

# redirect import to the real pypredict
package_root = dirname(dirname(dirname(abspath(__file__))))
sys.path.insert(0, package_root)
del sys.modules["pypredict"]
from pypredict import *

