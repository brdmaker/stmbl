"""brdstepper, variant A: see ../../brdstepper_common/parts_base.py."""
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "brdstepper_common"))
import parts_base

globals().update(parts_base.variant("A"))
