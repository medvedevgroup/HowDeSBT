from distutils.core import setup
from Cython.Build import cythonize

setup(
    ext_modules = cythonize(["query_fp_rate_compiled.pyx"])
)
