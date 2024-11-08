### FIO.Chronicle Configuration
Similarly to `nodeos`, `chronicle-receiver` needs a configuration directory with `config.ini` in it, and a data directory where it stores its internal state.

See the [Chronicle Tutorial](https://github.com/EOSChronicleProject/chronicle-tutorial) for a more detailed and complete example.

Here's a minimal configuration for the receiver using Websocket exporter. It connects to `nodeos` process running `state_history_plugin` at `localhost:8080` and exports the data to a websocket server at `localhost:8800`. In a production environment, hosts may be different machines in the network.

The receiver would stop immediately if the websocket server is not responding. For further tests, you need a consumer server ready.

The Perl script `testing/chronicle-ws-dumper.pl` can be used as a test websocket server that dumps the input to standard output.

```
mkdir -p /srv/memento_wax1/chronicle-config
cat >/srv/memento_wax1/chronicle-config/config.ini <<'EOT'
host = 127.0.0.1
port = 8080
mode = scan
plugin = exp_ws_plugin
exp-ws-host = 127.0.0.1
exp-ws-port = 8800
exp-ws-bin-header = true
EOT
```

### FIO Nodeos Configuration
FIO.Chronicle depends on fio-nodeos processing state history; to configure fio-nodeos to do so either pass the appropriate parameters on the command line or update the fio-nodeos configuration, located in '/etc/fio/nodeos/config.ini', to have the following attributers and values;
* contracts-console = true
* validation-mode = light
* trace-history = true
* chain-state-history = true
* plugin = eosio::state_history_plugin
* state-history-endpoint = 0.0.0.0:8080

In addition, the following attributes may be set;
* trace-history-debug-mode = true

Note that the state-history-endpoint address and port need to be reachable by the FIO.Chronicle receiver, chronicle-receiver.

### FIO.Chronicle Execution
```
# Start the receiver to check that everything is working as
# expected. Use Ctrl-C to stop it.
/usr/local/bin/chronicle-receiver \
  --config-dir=/srv/memento_wax1/chronicle-config --data-dir=/srv/memento_wax1/chronicle-data

# install systemd unit file
cp /usr/local/share/chronicle_receiver\@.service /etc/systemd/system/
systemctl daemon-reload

# You may need to initialize the Chronicle database from the first block
# in the state history archive. See the Chronicle Tutorial for more
# details. You may point it to some other state history source during
# the initialization. Here we launch it in scan-noexport mode for faster initialization.
/usr/local/bin/chronicle-receiver --config-dir=/srv/memento_wax1/chronicle-config \
 --data-dir=/srv/memento_wax1/chronicle-data \
 --host=my.ship.host.domain.com --port=8080 \
 --start-block=186332760 --mode=scan-noexport --end-block=186332800

# Once it stops at the end block, launch the service
systemctl enable chronicle_receiver@memento_wax1
systemctl start chronicle_receiver@memento_wax1

# watch the log
journalctl -u memento_dbwriter@wax1 -f
```

