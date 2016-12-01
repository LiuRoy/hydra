#-*- coding=utf8 -*-

import os
import glob
from setuptools import setup, Extension

SOURCE_FILES = [glob.glob(os.path.join('hydra', '*.c'))

hydra_extension = Extension(
    '_hydra',
    sources       = SOURCE_FILES,
    libraries     = ['ev'],
    include_dirs  = ['/usr/include/libev'],
    define_macros = [('WANT_SIGINT_HANDLING', '1')],
    extra_compile_args = ['-std=c99', '-fno-strict-aliasing', '-fcommon',
                          '-fPIC', '-Wall', '-Wextra', '-Wno-unused-parameter',
                          '-Wno-missing-field-initializers', '-g']
)

setup(
    name         = 'hydra',
    author       = 'roy liu',
    author_email = 'lrysjtu@gmail.com',
    license      = 'Apache License',
    url          = 'https://github.com/LiuRoy/hydra',
    description  = 'A Python thrift server written in C.',
    version      = '0.1',
    classifiers  = ['Programming Language :: C',
                    'Programming Language :: Python :: 2',
                    'Topic :: thrift :: Server'],
    py_modules   = ['hydra'],
    ext_modules  = [hydra_extension]
)
