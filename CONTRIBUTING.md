# Contributing to Vitracker

Thank you for your interest in contributing to Vitracker! This document provides guidelines and information for contributors.

## How to Contribute

### Reporting Bugs

Before creating a bug report, please check existing issues to avoid duplicates. When creating a bug report, include:

- **Clear title** describing the issue
- **Steps to reproduce** the behavior
- **Expected behavior** vs what actually happened
- **Screenshots** if applicable
- **System information**: OS, version, audio interface (if relevant)
- **Vitracker version** (check Help or About)

### Suggesting Features

Feature requests are welcome! Please:

- Check existing issues and discussions first
- Describe the use case and why it would be valuable
- Consider how it fits with Vitracker's vim-modal design philosophy

### Pull Requests

1. **Fork** the repository
2. **Create a branch** for your feature (`git checkout -b feature/amazing-feature`)
3. **Make your changes** following the code style below
4. **Test** your changes thoroughly
5. **Commit** with clear messages (`git commit -m 'Add amazing feature'`)
6. **Push** to your branch (`git push origin feature/amazing-feature`)
7. **Open a Pull Request** with a clear description

## Development Setup

### Prerequisites

- CMake 3.22+
- C++17 compiler (Clang, GCC, or MSVC)
- Platform audio libraries:
  - macOS: Core Audio (included with Xcode)
  - Linux: ALSA and/or JACK development headers
  - Windows: ASIO SDK (optional, for low-latency)

### Building

```bash
git clone https://github.com/masseyis/vitracker.git
cd vitracker
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

### Running Tests

```bash
cmake --build build --target test
```

## Code Style

### C++ Guidelines

- Use modern C++17 features where appropriate
- Follow existing naming conventions:
  - `camelCase` for functions and variables
  - `PascalCase` for classes and structs
  - `snake_case_` for private member variables (with trailing underscore)
  - `kConstantName` for constants
- Use JUCE types (`juce::String`, `juce::File`, etc.) for UI code
- Use `std::` types for model/audio code where possible
- Keep lines under 100 characters
- Use 4 spaces for indentation (no tabs)

### Architecture

Vitracker follows a layered architecture:

- **`model/`** - Data structures (Project, Pattern, Instrument, etc.)
- **`audio/`** - Audio engine, voice management
- **`dsp/`** - Signal processing (oscillators, filters, effects)
- **`ui/`** - JUCE UI components and screens
- **`input/`** - Keyboard handling and mode management

### Commit Messages

- Use present tense ("Add feature" not "Added feature")
- Use imperative mood ("Fix bug" not "Fixes bug")
- Keep first line under 50 characters
- Add detail in body if needed

## Areas for Contribution

### Good First Issues

Look for issues labeled `good first issue` - these are ideal for newcomers.

### Current Priorities

- **Cross-platform testing** - Testing on Windows and Linux
- **Accessibility** - Screen reader support, keyboard navigation improvements
- **Documentation** - Tutorials, examples, video guides
- **Presets** - Instrument presets for different synthesis engines
- **Bug fixes** - See open issues

### Future Features

Check the Discussions tab for planned features and roadmap items.

## Community

- **GitHub Discussions** - Questions, ideas, show & tell
- **GitHub Issues** - Bug reports and feature requests
- **Ko-fi** - Support development: https://ko-fi.com/masseyis

## License

By contributing to Vitracker, you agree that your contributions will be licensed under the GPLv3 license.

## Questions?

Feel free to open a Discussion or reach out through GitHub issues. We're happy to help!
