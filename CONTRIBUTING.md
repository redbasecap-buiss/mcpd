# Contributing to mcpd

Thanks for your interest in contributing! 🎉

## How to Contribute

1. **Fork** this repo
2. **Create a branch**: `git checkout -b feature/my-feature`
3. **Make your changes** and test them on real hardware if possible
4. **Commit**: `git commit -m "Add my feature"`
5. **Push**: `git push origin feature/my-feature`
6. **Open a Pull Request**

## Development Guidelines

- Code must compile for ESP32 with Arduino framework
- Use ArduinoJson v7 for JSON handling
- Follow existing code style (4-space indent, `_` prefix for private members)
- Add examples for new features
- Update README.md if adding new tools or capabilities

## Adding a Built-in Tool

1. Create `src/tools/MCP<Name>Tool.h`
2. Follow the pattern in `MCPGPIOTool.h` — static `attach(Server&)` method
3. Add an example showing usage
4. Update the tools table in README.md

## Reporting Issues

- Include your board type and Arduino/PlatformIO version
- Include the Serial Monitor output
- Minimal reproduction sketch is appreciated

## License

By contributing, you agree that your contributions will be licensed under the MIT License.
