"""brdstepper's board, variant A: see ../../brdstepper_common/board_base.py."""
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "brdstepper_common"))
import board_base

globals().update(board_base.variant("A"))
