# Changelog

## 1.6.0

### Enhancements

- Enhance OTA fetch reliability
    - Monitor message publish acknowledgement for the otafetch message
    - Add retry logic if otafetch fails
- Add OTA retry on failure mechanism
    - Try OTA multiple times (as per `CONFIG_ESP_RMAKER_OTA_MAX_RETRIES`, set to 3 by default) if it fails
    - Schedule an OTA fetch as per `CONFIG_ESP_RMAKER_OTA_RETRY_DELAY_MINUTES` if all retries fail