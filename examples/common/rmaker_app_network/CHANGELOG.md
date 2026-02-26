# Changelog

All notable changes to this project will be documented in this file.

## [1.3.1]

### Fixed
- POP mismatch event was getting handled even after provisioning ends, causing incorrect prints due to failures coming from local control module.

## [1.3.0]

### Added
- Exposed `app_network_get_device_pop()` API to get PoP string for a given pop_type
- Added `app_network_get_device_pop_default()` API to get PoP using cached pop_type from `app_network_start()`
- Added `app_network_get_device_service_name()` API to get the provisioning service name (BLE device name / SoftAP SSID)

### Fixed
- Fixed buffer size for service name to accommodate longer `CONFIG_APP_NETWORK_PROV_NAME_PREFIX` values

## [1.2.0]

### Fixed
- WiFi event handlers are now registered only after provisioning succeeds or when device is already provisioned,
  preventing interference with the provisioning manager's internal WiFi connection handling
- Fixed issue where `network_prov_mgr_reset_wifi_sm_state_on_failure()` could fail due to WiFi being in connecting state
- Removed retry logic when `CONFIG_APP_NETWORK_RESET_PROV_ON_FAILURE` is not set, allowing re-provisioning from phone app after failure

## [1.1.0]

### Added
- Asynchronous network connection support using CONFIG_APP_NETWORK_ASYNCHRONOUS_CONNECTION
- User authentication device type
- AI agent device subtype

## [1.0.0]

### Added
- First version of the RainMaker App Network component on idf registry
- WiFi management and configuration functionality
- Thread network connectivity support
- Unified API for different network protocols
- Seamless ESP RainMaker framework integration
