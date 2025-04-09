# ESP RainMaker Unit Tests

This directory contains unit tests for the ESP RainMaker component.

## Prerequisites

- ESP-IDF (v5.1 or newer)
- An ESP32 development board

## Building and Running the Tests

```bash
idf.py build
idf.py -p [PORT] flash monitor
```

## Test Components

The tests cover the following functionalities:

1. **Core RainMaker Features**
   - Node creation and management
   - Device creation and management
   - Parameter operations
   - Node attributes
   - Node cleanup (teardown)

2. **MQTT Operations**
   - Initialization
   - Subscription
   - Publishing
   - Data handling
   - NULL and invalid input validation

3. **OTA** 
   - OTA enable with params
   - Report status (NULL handle)
   - Mark valid / mark invalid (state checks)

4. **Auth Service**
   - Token status update
   - Get token and URL when disabled or NULL
   - Enable and disable lifecycle

5. **Groups** – Groups service enable with node

6. **Scenes** – Scenes enable with node

7. **Schedule** – Schedule enable with node

Test order is fixed: Core (node/device) runs first; OTA, Groups, Scenes, and Schedule depend on the node; Node Cleanup runs last.

## Known Issues

1. Some tests require network connectivity and proper credentials.
2. The linter errors related to header files are expected during development as the test project needs to be built with ESP-IDF to resolve includes.

## Resolving Include Path Issues

If you're facing linter errors related to missing include paths, you can:

1. Build the project once using `idf.py build`
2. Point your editor to use the compile_commands.json generated in the build directory
3. For VS Code, add this to .vscode/c_cpp_properties.json:

```json
{
    "compileCommands": "${workspaceFolder}/components/esp_rainmaker/test_app/build/compile_commands.json"
}
```
