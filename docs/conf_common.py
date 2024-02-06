# -*- coding: utf-8 -*-
#
# Common (non-language-specific) configuration for Sphinx
#

# type: ignore
# pylint: disable=wildcard-import
# pylint: disable=undefined-variable

from __future__ import print_function, unicode_literals

from esp_docs.conf_docs import *  # noqa: F403,F401
import os
import sys

sys.path.insert(0, os.path.abspath('.'))

extensions += ['sphinx_copybutton',
               # Needed as a trigger for running doxygen
               'esp_docs.esp_extensions.dummy_build_system',
               'esp_docs.esp_extensions.run_doxygen',
               'sphinx.ext.autodoc'
               ]

# link roles config
github_repo = 'espressif/esp-rainmaker'

# context used by sphinx_idf_theme
html_context['github_user'] = 'espressif'
html_context['github_repo'] = 'esp-rainmaker'
html_static_path = ['../_static']

# Extra options required by sphinx_idf_theme
project_slug = 'esp-rainmaker'

idf_targets = ['esp32', 'esp32s3', 'esp32c2', 'esp32c3']
languages = ['en']
