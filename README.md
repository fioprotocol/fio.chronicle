# FIO Chronicle Project
FIO.Chronicle is a software application designed to process the state and trace history of the [FIO](https://github.com/fioprotocol/fio) blockchain and is a fork of the [EOSChronicle](https://github.com/EOSChronicleProject/eos-chronicle) project. For more detailed information regarding FIO.Chronicle and its origin, EOSChronicle, see the [FIO.Chronicle Overview](https://github.com/fioprotocol/fio.chronicle/blob/develop/docs/overview.md).

* For information on FIO Protocol, visit [FIO](https://fio.net).
* For information on the FIO Chain, API, and SDKs, including detailed clone, build and deploy instructions, visit [FIO Protocol Developer Hub](https://dev.fio.net).

## Licenses and Copyrights
[FIO.Chronicle License](https://github.com/fioprotocol/fio.chronicle/blob/develop/LICENSE.txt)

Source code repository: https://github.com/fioprotocol/fio.chronicle

Forked Source code repository: https://github.com/EOSChronicleProject/eos-chronicle

Copyright 2018-2023 cc32d9@gmail.com

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

## Releases

See the [release notes](https://github.com/fioprotocol/fio.chronicle/blob/develop/docs/releases.md) for information regarding FIO/EOS Chronicle releases.

## Clone the repository
To clone the FIO.Chronicle repository, execute the command;
```shell
git clone --recursive https://github.com/fioprotocol/fio.chronicle.git
```

## Build and Install Instructions
Minimum build requirements: Cmake 3.11, GCC 8.3.0

Dependencies:
* Boost, version 1.80.0
* Clang, version 11.0.1
* CMake, version 3.31.2
* LLVM, version 7.1.0
* PostgreSQL, version 16*

**PostgreSQL is a mandatory dependency regardless of the plugin used. To install PostgreSQL execute the following commands (taken from https://www.postgresql.org/download/linux/ubuntu/)**
```shell
sudo apt install curl ca-certificates
sudo install -d /usr/share/postgresql-common/pgdg
sudo curl -o /usr/share/postgresql-common/pgdg/apt.postgresql.org.asc --fail https://www.postgresql.org/media/keys/ACCC4CF8.asc
sudo sh -c 'echo "deb [signed-by=/usr/share/postgresql-common/pgdg/apt.postgresql.org.asc] https://apt.postgresql.org/pub/repos/apt $(lsb_release -cs)-pgdg main" > /etc/apt/sources.list.d/pgdg.list'
sudo apt update
sudo apt -y install libpq-dev
```

See the [FIO.Relic Readme](https://github.com/fioprotocol/fio.relic/blob/develop/README.md) for specific instructions to stand up the FIO.Relic database.

### Build
The build script takes one argument, the directory where to find or install the build dependencies including Boost, Clang, and LLVM. It is recommended to use a non-system directory such as '/opt'. Note that any future builds, if given the same directory, will reuse those build dependencies.

To build fio.chronicle, execute the following commands;
```shell
cd fio.chronicle
./scripts/build.sh /opt
```

### Install
To install fio.chronicle to the default install directory, '/opt/fio-chronicle', execute the following command;
```shell
./scripts/install.sh
```

To install fio.chronicle to the custom location, i.e. '/home/ubuntu/fio/chronicle', execute the following command;
```shell
./scripts/install.sh -i /home/ubuntu/fio/chronicle
```

To install fio.chronicle as a service, under control of systemd and managed using systemctl, execute the following command;
```shell
./scripts/install.sh -s
```

For non-service installations, the FIO.Chronicle executable, configuration, and state are installed to the target directory. Startup, shutdown, and state capture are accomplished either manually or through use of the convenience scripts; _start.sh_, _stop.sh_, and _snapshot.sh_.

For service installations the FIO.Chronicle executable is installed to _/usr/local/sbin_, the configuration and state are installed to _/srv/fio_. Startup and shutdown are accomplished through use of systemctl.

The script will output installation details including information about configuration. See the following configuration overview for more insight into the default configuration as well as how to customize it.

### Configuration
The configuration of FIO.Chronicle is designated via options specified on the command-line as well as captured in a config.ini that is read as part of start up. The configuration options include, but are not limited to, the following;

<ins>Command-Line Options</ins>
* --config-dir=\<directory where to find the config.ini\>
* --data-dir=\<directory where to store data\>

<ins>Config.Ini Options</ins>
* host = \<the nodeos state history host (upstream connection to FIO.Nodeos state history api endpoint)\>
* port = \<the nodeos state history api port (upstream connection to FIO.Nodeos state history api endpoint port)\>
* plugin = \<the active plugin (max 1 active); options include 'exp-ws-plugin', 'exp-relic-plugin' \>

_Relic DB Exporter Plugin_:
* plugin = exp-relic-plugin
* exp-relic-host = \<the FIO.Relic database server host, valid for 'exp-ws-plugin' only (the downstream connnection to a PostgreSQL server host)\>
* exp-relic-port = \<the FIO.Relic database port, valid for 'exp-ws-plugin' only (the downstream connnection to a PostgreSQL server port)\>
* exp-relic-username = \<the FIO.Relic database user name, valid for 'exp-ws-plugin' only\>
* exp-relic-password = \<the FIO.Relic database password, valid for 'exp-ws-plugin' only\>

Given configuration of the Relic DB Exporter plugin, the config.ini is as follows;
```shell
host = 127.0.0.1
port = 8080
mode = scan
plugin = exp_relic_plugin
exp-relic-host = 127.0.0.1
exp-relic-port = 5432
exp-relic-username = chronicle_user
exp-relic-password = password123!
```

These options will allow FIO.Chronicle to connect to a FIO State History node at `127.0.0.1:8080` (host:port) and will export processed data to the FIO.Relic PostgreSQL DB server running on host `127.0.0.1` and port `5432`.

### Starting the application
To manually start FIO.Chronicle, execute the command;
```shell
/opt/fio-chronicle/chronicle-receiver --config-dir=/opt/fio-chronicle/config --data-dir=/opt/fio-chronicle/data --start-block=1 --end-block=1000
```
Note that in the above command both a start and an end block number are specified, which limits processing to that block range. Specify a very large number for the `--end-block` parameter to process blocks for the foreseeable future.

To capture standard output to a log, execute the above command redirecting standard ouput as follows;
```shell
mkdir -p /opt/fio-chronicle/log
/opt/fio-chronicle/chronicle-receiver --config-dir=/opt/fio-chronicle/config --data-dir=/opt/fio-chronicle/data --start-block=1 --end-block=1000 2>&1 | tee -a /opt/fio-chronicle/log/chronicle.log &
```

For FIO.Chronicle service installations;  
```shell
systemctl start chronicle_receiver@fio
```

Using the start script, _start.sh_, execute the command (note that logging is captured in chronicle.log);
```shell
./scripts/start.sh
```
or
```shell
./scripts/start.sh -b /home/ubuntu/fio/chronicle
```
In the case that FIO.Chronicle is a service;
```shell
./scripts/start.sh -s
```

### Stopping the application
To stop FIO.Chronicle, execute the command;
```shell
pgrep chronicle
kill -INT <PID>
```

For FIO.Chronicle service installations;  
```shell
systemctl stop chronicle_receiver@fio
```

Using the start script, _stop.sh_, execute the command;
```shell
./scripts/start.sh
```
In the case that FIO.Chronicle is a service;
```shell
./scripts/start.sh -s
```

**WARNING**: An exporter plugin as well as the application supporting that exporter should be in place before executing the command above. See the [FIO.Relic Readme](https://github.com/fioprotocol/fio.relic/blob/develop/README.md) for specific instructions to stand up the FIO.Relic database or the addendum below to set up a web socket server.

### Logging:
For local/non-service installations tail the log if standard output was redirected as follows;
```shell
tail -f /opt/fio-chronicle/log/chronicle.log
```

For service installations, execute the following command;
```shell
journalctl -u chronicle_receiver@fio -f
```
### Data Capture: Export and Import
To periodically take a snapshot of the FIO.Chronicle state, primarily for stand-up of another server or fallback to a previous state, the _snapshot.sh_ script is provided. This script provides functionality to both export and import state. Note that this state should coincide with the state resident in the FIO.Relic database.

To export the FIO.Chronicle state execute the script, _snapshot.sh_, passing the argument '-e' (for export). In addition, providing the '-s' argument will export data to a specific directory or file. In the case that a directory is provided, a default file name will be created in the form 'fc-snapshot-<Current Date and Time>.tar.gz'. Following are three examples for exporting data;

```shell
sudo ./scripts/snapshot.sh -e
```
The snapshot file, _/opt/fio-chronicle/bkup/fc-snapshot-2025-03-16T132648.tar.gz_, will be created in the **/opt/fio-chronicle/bkup** directory containing the FIO.Chronicle state at March 16, 2025, 1:26:48 pm. Note that _/opt/fio-chronicle_ is the default installation directory for FIO.Chronicle.

Other examples include;
```shell
sudo ./scripts/snapshot.sh -e -a /tmp
```
The snapshot file, _/tmp/fc-snapshot-2025-03-16T132648.tar.gz_, will be created in the **/tmp** directory containing the FIO.Chronicle state at March 16, 2025, 1:26:48 pm.

```shell
sudo ./scripts/snapshot.sh -e -a /tmp/chronicle_snapshot
```
The snapshot file, _/tmp/chronicle_snapshot_, will be created containing the FIO.Chronicle state at March 16, 2025, 1:26:48 pm. Note that the file format is a tar gzip file regardless of the extention provided.

```shell
sudo ./scripts/snapshot.sh -e -s
```
The snapshot file, /srv/fio/chronicle-bkup/fc-snapshot-2025-03-16T132648.tar.gz_, will be created containing the FIO.Chronicle state at March 16, 2025, 1:26:48 pm. Note that the '-s' argument signified that FIO.Chronicle was installed as a service.

To import the FIO.Chronicle state execute the script, _snapshot.sh_, passing the argument '-i' (for import), and the '-s' argument specifying the snapshot file. For example, to import the data previously exported to the file _/tmp/chronicle_snapshot_, execute the command; 
```shell
sudo ./scripts/snapshot.sh -i -a /tmp/chronicle_snapshot
```

### Addendum
For the purposes of confirming end-to-end connectivity refer to the configuration as well as the documents below to start a local FIO state history node as well as a local web socket server

_Web Socker Exporter Plugin Configuration_:
* plugin = exp-ws-plugin
* exp-ws-host = \<the websocket server host (the downstream connnection to a web socket server host)\>
* exp-ws-port = \<the websocket server port (the downstream connnection to a web socket server port)\>

_Supporting Documentation_:
* [LocalNet Deployment Guide - Start FIO Nodeos](https://github.com/fioprotocol/fio.relic/blob/develop/docs/localnet-standup.md#start-fio-nodeos)
* [LocalNet Deployment Guide - Start FIO Nodeos History Node](https://github.com/fioprotocol/fio.relic/blob/develop/docs/localnet-standup.md#start-fio-nodoes-state-history-nodeos)
* [LocalNet Deployment Guide - Start FIO.Chronicle Web Socket Server](https://github.com/fioprotocol/fio.relic/blob/develop/docs/localnet-standup.md#start-fio-chronicle-test-web-socket-server)
