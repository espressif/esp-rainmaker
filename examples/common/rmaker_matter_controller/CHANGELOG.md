# Changelog

## 2.0.0

### Features
- Add support for device_type_list on every endpoint
- Add Matter command-response support for remote invoke, write, and read commands.
- Add Matter attribute report publishing through the `MTDevices` setup-service parameter.
- Add PSRAM-preferred allocation options for controller buffers and report task stack allocation.

### Changed
- Device-list update callbacks now receive a temporary read-only device list. Applications must copy it if they need to retain it.
- Matter controller no longer stores a shared device-list copy internally; ownership is moved to applications that need a cached list.

## 1.0.1 Jun 5

### Bug fix
- Set payload_is_json for PUT/POST calling of app_rmaker_user_api

## 1.0.0

- First version of the ESP RainMaker Matter Controller component

### Added
- RainMaker Matter Controller Setup service
