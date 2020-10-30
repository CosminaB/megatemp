mosquitto_pub -h mqttserver.example.com -t 10/setconfig -m "`cat temp_config.json`" -r
