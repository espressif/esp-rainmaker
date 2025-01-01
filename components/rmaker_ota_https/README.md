# ESP RainMaker HTTPS OTA

This contains the ESP RainMaker component for performing OTA over HTTPS. It uses [RainMaker Node APIs](https://swaggerapis.rainmaker.espressif.com/?urls.primaryName=RainMaker%20Node%20APIs) for retriving the OTA information and uses the callback from RainMaker itself for applying OTA.

## Rollback Mechanism
Since regular OTA rollback is based on MQTT connection(which isn't applicable in this case), rollback mechanism works as follows:  
1. A rollback timer is created for specified period.
2. Devices tries to report "successful" for firmware.
3. If reporting is successful, firmware is marked valid and rollback timer is canceled.
4. If reporting fails, it is retried continuosly at 5 seconds interval; until the rollback timer expires.
5. If firmware cannot be verified within the specified rollback timeout, firmware is marked invalid and device reboots in the previous firmware.   

---

**Note:** Since this component uses callbacks from generic RainMaker OTA component, callback specific settings(like project name, version verification) needs to be configured from *ESP RainMaker Config -> ESP RainMaker OTA Config* in ESP-IDF menuconfig.  
**Note:** It is does not currently work with ESP Secure Cert.
