# Wayland C++ Library (waypp)

_Note: waypp is a work in progress and is not ABI stable_

## Introduction

waypp is a modern and efficient C++ library for Wayland. This library aims to provide a smooth and easy-to-use interface
for developers to build Wayland-based applications using C++.

## Tested Compositors

* AGL (Coming)
* Mutter
* Weston
* WLRoots

## Installation

### Debian

This project requires the following packages/libraries:

- libwayland-dev
- libxkbcommon-dev
- wayland-protocols
- glib-2.0

### If your graphics driver is supported by Mesa, then use:
- mesa-common-dev
- libgles2-mesa-dev
- libegl1-mesa-dev

## Usage

Follow these steps to start using the project:


### Clone the repository
```git clone https://github.com/jwinarske/waypp```

### Move into the cloned repository
```cd waypp```

### Generate makefile with CMake
```cmake ..```

### Build the project
```make -j```

### Run demo applications
```./examples/simple-egl```
```./examples/simple-shm```

## License

This project is licensed under Apache-2.0.

## Contributing

We welcome contributions from everyone. To contribute to this project:

1. Clone the project.
2. Create a new feature branch.
3. Make your changes.
4. Test and format your code before committing.
5. Make a pull request with detailed changes.

If you're not sure where to start, check out open issues for tasks you can help with.