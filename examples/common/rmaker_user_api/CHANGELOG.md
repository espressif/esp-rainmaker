## v1.1.1 - 2026-08-19

### Changes

* Make the HTTP TX buffer size configurable with `CONFIG_RM_USER_HTTP_TX_BUFFER_SIZE`
  (2048 to 16384 bytes). Change the option to fit the request line and headers,
  including the access token in the `Authorization` header.

## v1.1.0 - 2026-07-24

### Changes

* Support HTTP session reuse on all supported ESP-IDF versions.
* Bump minimal IDF version requirement to `>=5.1`.

## v1.0.1 - 2026-06-05

### Bug Fix

* Set payload_is_json for PUT/POST calling

## v1.0.0 - 2026-02-10

### Enhancements

* Initial version, support RainMaker User API
