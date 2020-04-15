# Documentation Source Folder

This folder contains source files of **ESP RainMaker API documentation**.

The sources do not render well in GitHub and some information is not visible at all.

Use actual documentation generated within about 20 minutes on each commit:

# Hosted Documentation

* Check here: https://docs.espressif.com/projects/esp-rainmaker/en/latest/

The above URL is for the master branch latest version.

# Building Documentation

* Install `make` and `doxygen` for your platform (`make` may already be installed as an ESP-IDF prerequisite).
* Change to the docs directory and run `make html`
* `make` will probably prompt you to run a python pip install step to get some other Python-related prerequisites. Run the command as shown, then re-run `make html` to build the docs.

## For MSYS2 MINGW32 on Windows

If using Windows and the MSYS2 MINGW32 terminal, run this command before running "make html" the first time:

```
pacman -S doxygen mingw-w64-i686-python2-pillow
```

Note: Currently it is not possible to build docs on Windows without using a Unix-on-Windows layer such as MSYS2 MINGW32.
