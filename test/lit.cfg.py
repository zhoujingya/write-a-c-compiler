# -*- Python -*-

# Configuration file for the 'lit' test runner.

import platform, os

import lit.formats

# Global instance of LLVMConfig provided by lit
from lit.llvm import llvm_config
from lit.llvm.subst import ToolSubst

# name: The name of this test suite.
# (config is an instance of TestingConfig created when discovering tests)
config.name = "tinycc-compiler"

# testFormat: The test format to use to interpret tests.
# As per shtest.py (my formatting):
#   ShTest is a format with one file per test. This is the primary format for
#   regression tests (...)
# I couldn't find any more documentation on this, but it seems to be exactly
# what we want here.
config.test_format = lit.formats.ShTest(not llvm_config.use_lit_shell)

# suffixes: A list of file extensions to treat as test files. This is overriden
# by individual lit.local.cfg files in the test subdirectories.
config.suffixes = [".c"]

# test_source_root: The root path where tests are located.
config.test_source_root = os.path.dirname(__file__)

# excludes: A list of directories to exclude from the testsuite. The 'Inputs'
# subdirectories contain auxiliary inputs for various tests in their parent
# directories.
config.excludes = ["Inputs"]

llvm_config.add_tool_substitutions(
  ["tinycc"],
  config.bin_dir,
)
# The LIT variable to hold the file extension for shared libraries (this is
# platform dependent)
config.substitutions.append(("%shlibext", config.llvm_shlib_ext))
# The LIT variable to hold the location of plugins/libraries
config.substitutions.append(("%shlibdir", config.llvm_shlib_dir))

# Add the 'utils' directory to the environment path
utils_path = os.path.join(os.path.dirname(__file__), "..", "utils")
config.environment["PATH"] = os.pathsep.join(
    (utils_path, config.environment.get("PATH", ""))
)
# Define a substitution for the utils path
config.substitutions.append(("%utils_path", utils_path))
