# Configuration file for the Sphinx documentation builder.
#
# This file only contains a selection of the most common options. For a full
# list see the documentation:
# https://www.sphinx-doc.org/en/master/usage/configuration.html

# -- Path setup --------------------------------------------------------------

# If extensions (or modules to document with autodoc) are in another directory,
# add these directories to sys.path here. If the directory is relative to the
# documentation root, use os.path.abspath to make it absolute, like shown here.
#
import os
import sys

sys.path.insert(0, os.path.abspath('.'))
sys.path.append(os.path.abspath('../cli/'))
autodoc_mock_imports = ["pathlib", "cryptography", "nvs_partition_gen", "oauth2client", "serial", "user_mapping", "rmaker_tools.rmaker_prov.esp_rainmaker_prov", "rmaker_tools.rmaker_prov.security", "rmaker_tools.rmaker_prov.prov", "rmaker_tools.rmaker_prov.prov_util", "rmaker_tools.rmaker_claim.claim"]


# -- Project information -----------------------------------------------------

project = u'ESP RainMaker Programming Guide'
copyright = u'2020, Espressif Systems (Shanghai) CO., LTD'
author = 'Espressif'

try:
    builddir = os.environ['BUILDDIR']
except KeyError:
    builddir = '_build'

def call_with_python(cmd):
    # using sys.executable ensures that the scripts are called with the same Python interpreter
    if os.system('{} {}'.format(sys.executable, cmd)) != 0:
        raise RuntimeError('{} failed'.format(cmd))


# Call Doxygen to get XML files from the header files
print("Calling Doxygen to generate latest XML files")
if os.system("doxygen Doxyfile") != 0:
    raise RuntimeError('Doxygen call failed')

# Generate 'api_name.inc' files using the XML files by Doxygen
call_with_python('./gen-dxd.py')
# -- General configuration ---------------------------------------------------

# Add any Sphinx extension module names here, as strings. They can be
# extensions coming with Sphinx (named 'sphinx.ext.*') or your custom
# ones.
extensions = [
    'breathe',
    'sphinx.ext.autodoc',
    'link-roles'
]

# Setup the breathe extension
breathe_projects = {
    "ESP RainMaker": "./xml"
}
breathe_default_project = "ESP RainMaker"

# Add any paths that contain templates here, relative to this directory.
templates_path = ['_templates']

# List of patterns, relative to source directory, that match files and
# directories to ignore when looking for source files.
# This pattern also affects html_static_path and html_extra_path.
exclude_patterns = ['_build', 'Thumbs.db', '.DS_Store']


# -- Options for HTML output -------------------------------------------------

# The theme to use for HTML and HTML Help pages.  See the documentation for
# a list of builtin themes.
#
html_theme = 'sphinx_rtd_theme'

# Add any paths that contain custom static files (such as style sheets) here,
# relative to this directory. They are copied after the builtin static files,
# so a file named "default.css" will overwrite the builtin "default.css".
html_static_path = ['_static']

html_logo = "_static/esp-rainmaker-logo.png"

def setup(app):
    app.add_stylesheet('theme_overrides.css')
