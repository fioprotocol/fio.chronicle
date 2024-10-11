# Local/Dev Build, Install and Deployment Guide

## Clone and build Fio.Chronicle
The following command is a one-stop shop for the clone and build of fio.chronicle and will
* clone the fio.chronicle repo
* cd to the fio.chronicle repo and make the build directory
* Install the necessary fio.chronicle build dependencies
* execute the build script passing in the build directory

```shell
git clone --recursive https://github.com/fioprotocol/fio.chronicle.git && cd ./fio.chronicle && git checkout develop && git pull && mkdir -p build && sudo ./pinned_build/install_deps.sh && nice ./pinned_build/chronicle_pinned_build.sh /opt build $(nproc)
```

Install fio.chronicle to /opt
```shell
sudo mkdir -p /opt/fio-chronicle && sudo cp -r build/* /opt/fio-chronicle
```

## Configure the FIO blockchain to capture history

### FIO
As this is a localnet deployment of FIO, the fio.devtools framework will be used and everything should be set up by default. However, for clarity it is important to understand the fio.devtools localnet configuration as well as the fio.devtools history node configuration.

The localnet 3-node default blockchain as well as the state history node is started using the fio.devtools start script, start.sh. The 3-node blockchain is configured to process blocks on ports 9876, 9877, and 9878 and have chain api plugin ports of 8879, 8889 and 8890.

The history node, which is run as a docker container, will connect to the 3-node blockchain described above, ingest blocks and store state history. Its state history api port will be 8080 and any connnections to pull state history will utilize this port.

Refering to this in the EOS Chronicle doc (https://github.com/EOSChronicleProject/eos-chronicle?tab=readme-ov-file#state-history-plugin-in-nodeos) the state history node configuration will have the following attributes/values;

```shell
contracts-console = true
validation-mode = light
plugin = eosio::state_history_plugin
trace-history = true
chain-state-history = true
trace-history-debug-mode = true
state-history-endpoint = 0.0.0.0:8080
```

To expedite this update, the fio.devtools start script has been updated to copy the appropriate history node configuration. For example when starting a state history docker node, the script performs the following;
```shell
cp scripts/launch/history/container/etc/config.ini-statehistory scripts/launch/history/container/etc/config.ini
```

This will be used below when starting the history node

### EOS-Chronicle
Create the config and the data directory and initialize the fio-chronicle config.ini. The config.ini connection options are as follows;
* host: the nodeos state history host (upstream connection to fio nodeos state history api endpoint)
* port: the nodeos state history api port (upstream connection to fio nodeos state history api endpoint port)
* exp-ws-host: the websocket server host (the downstream connnection to a web socket server host)
* exp-ws-port: the websocket server port (the downstream connnection to a web socket server port)

```shell
mkdir -p /opt/fio-chronicle/config /opt/fio-chronicle/data

cat >/opt/fio-chronicle/config/config.ini <<'EOT'
host = 127.0.0.1
port = 8080
mode = scan
plugin = exp_ws_plugin
exp-ws-host = 127.0.0.1
exp-ws-port = 8891
exp-ws-bin-header = false
EOT
```

## Run the fio-chronicle-webSocket server ecosystem
### Start fio-nodeos
```shell
# Build contracts (optional)
./start 3.5, option 2

# Start local 3-node blockchain
./start 3.5, option 1, option 1
```

### Start fio-nodoes state history nodeos
```shell
# Start state history node (as a docker container)
./start 3.5, option 1, option 6

# Select the default P2P Nodeos IP address and P2P Nodeos Port
P2P Nodeos IP address [172.31.20.163]:<Enter>
P2P Nodeos Port [8889]:<Enter>

# Select option 2 to start a state history node
1. V1 History Node 2. State History Node
Choose(#):2<Enter>
```

### Start chronicle test web socket server
Note that this web socket server represents a downstream server showing json formatted state history data processed by EOS-Chronicle and is for test purposes only

```shell
perl /opt/fio-chronicle/testing/chronicle-ws-dumper.pl --port=8891
```

Note that if any missing module errors occur running the script above, then you may be missing one or more module dependencies. These dependencies may be found in the comment section at the top of the script.

```shell
sudo apt install cpanminus libjson-xs-perl libjson-perl
sudo cpanm Net::WebSocket::Server
```

## Start eos-chronicle
```shell
/opt/fio-chronicle/chronicle-receiver --config-dir=/opt/fio-chronicle/config --data-dir=/opt/fio-chronicle/data --end-block=846511
```
