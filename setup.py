from setuptools import setup, Extension

vec2x_ext = Extension(
    'tools.vec2x_py._vec2x',
    sources=[
        'tools/vec2x_py/_vec2xmodule.c',
        'tools/vec2x_py/csrc/vec2x.c',
        'tools/vec2x_py/csrc/e6809.c',
    ],
    include_dirs=['tools/vec2x_py/csrc'],
    define_macros=[('VEC2X_NO_SOUND', '1')],
    extra_compile_args=['-O2'],
)

setup(name='vectrex-hero-tools', ext_modules=[vec2x_ext])
