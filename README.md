# FIO Chronicle Project
FIO.Chronicle is a software application designed to process the state and trace history of the [FIO](https://github.com/fioprotocol/fio) blockchain and is a fork of the [EOSChronicle](https://github.com/EOSChronicleProject/eos-chronicle) project. For more detailed information regarding FIO.Chronicle and its origin, EOSChronicle, see the [FIO.Chronicle Overview](https://github.com/fioprotocol/fio.chronicle/blob/develop/docs/overview.md).

# FIO Protocol
The Foundation for Interwallet Operability (FIO) or, in short, the FIO Protocol, is an open-source project based on EOSIO 1.8+.

* For information on FIO Protocol, visit [FIO](https://fio.net).
* For information on the FIO Chain, API, and SDKs, including detailed clone, build and deploy instructions, visit [FIO Protocol Developer Hub](https://dev.fio.net).
* To get updates on the development roadmap, visit [FIO Improvement Proposals](https://github.com/fioprotocol/fips). Anyone is welcome and encouraged to contribute.
* To contribute, please review [Contributing to FIO](https://dev.fio.net/docs/contributing-to-fio)
* To join the community, visit [Discord](https://discord.com/invite/pHBmJCc)

## Licenses and Copyrights
[FIO License](https://github.com/fioprotocol/fio/blob/master/LICENSE)

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

## Relevant Documentation (Tutorial, Chat, Clients, Uses)

* [Chronicle Telegram chat](https://t.me/+TMWWcV1gBxQiqIkm)

* [Chronicle Tutorial](https://github.com/EOSChronicleProject/chronicle-tutorial) explains the nodeos and Chronicle server installation in detail.

* [Chronicle Consumer Module](https://github.com/EOSChronicleProject/chronicle-consumer-npm) is a Node.js module that consumer processes can be based on.

* [Chronicle Consumer Module Examples](https://github.com/EOSChronicleProject/chronicle-consumer-npm-examples) is a number of examples using the Node.js module.

* [Awesome Chronicle](https://github.com/EOSChronicleProject/awesome-chronicle) is a list of software projects and services using the software.

* [Docker File](https://github.com/EOSTribe/eos-chronicle-docker) provided by EOS Tribe

### Clone the repository
To clone the FIO.Chronicle repository, execute the command; `git clone --recursive https://github.com/fioprotocol/fio.chronicle.git`. see [Cloning a repository](https://docs.github.com/en/repositories/creating-and-managing-repositories/cloning-a-repository) for more information.

### Build and Install Instructions
Minimum build requirements: Cmake 3.11, GCC 8.3.0

Dependencies:
* Boost, version 1.80.0
* Clang, version 11.0.1
* CMake, version 3.31.2
* LLVM, version 7.1.0
* PostgreSQL, version 16*

**Currently PostgreSQL is a mandatory dependency despite the optional use of plugins that may require it. To install PostgreSQL execute the following commands (taken from https://www.postgresql.org/download/linux/ubuntu/)**
```shell
sudo apt install curl ca-certificates
sudo install -d /usr/share/postgresql-common/pgdg
sudo curl -o /usr/share/postgresql-common/pgdg/apt.postgresql.org.asc --fail https://www.postgresql.org/media/keys/ACCC4CF8.asc
sudo sh -c 'echo "deb [signed-by=/usr/share/postgresql-common/pgdg/apt.postgresql.org.asc] https://apt.postgresql.org/pub/repos/apt $(lsb_release -cs)-pgdg main" > /etc/apt/sources.list.d/pgdg.list'
sudo apt update
sudo apt -y install postgresql-16
```

See the [FIO.Relic Readme](https://github.com/fioprotocol/fio.relic/blob/develop/README.md) for specific instructions to stand up the FIO.Relic database.

#### Build
The build script takes one argument, the directory where to find or install the build dependencies including Boost, Clang, and LLVM. It is recommended to use a non-system level directory such as '/opt'. Note that any future builds, if given the same directory, will reuse those build dependencies.

To build fio.chronicle, execute the following commands;
```shell
cd fio.chronicle
./scripts/build.sh /opt
```

#### Install
To install fio.chronicle along with the default configuration to '/opt/fio-chronicle', execute the following command;
```shell
./scripts/install.sh
```

If desired, you may install the executable to '/usr/local/bin' by doing the following;
```shell
cd build
sudo make install
```
Note that the latter command will not install a default configuration; to do that, copy the [config.ini.sample](./config/config.ini.sample) to '/opt/fio-chronicle/config/config.ini'. See the following configuration overview for more insight into the default configuration as well as how to customize it.

#### Configuration
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

#### Starting the application
In the following command both a start and an end block number is specified to limit block processing. Note that the start block will default to 1 so only an end block is neccessary. As the `--end-block` parameter is required, specify a very large number to process blocks for the foreseeable future.

To start the fio-chronicle-receiver, execute the command;
```shell
/opt/fio-chronicle/chronicle-receiver --config-dir=/opt/fio-chronicle/config --data-dir=/opt/fio-chronicle/data --start-block=1 --end-block=10000
```

**WARNING**: An exporter plugin as well as the application supporting that exporter should be in place before executing the command above. See the [FIO.Relic Readme](https://github.com/fioprotocol/fio.relic/blob/develop/README.md) for specific instructions to stand up the FIO.Relic database or the addendum below to set up a web socket server.


#### Data Capture: Export and Import
To periodically take a snapshot of the FIO.Chronicle state, primarily for stand-up of another server or fallback to a previous state, the _snapshot.sh_ script is provided. This script provides functionality to both export and import state. Note that this state should coincide with the state resident in the FIO.Relic database.

To export the FIO.Chronicle state execute the script, _snapshot.sh_, passing the argument '-e' (for export). In addition, providing the '-s' argument will export data to a specific directory or file. In the case that a directory is provided, a default file name will be created in the form 'fc-snapshot-<Current Date and Time>.tar.gz'. Following are three examples for exporting data;

```shell
sudo ./scripts/snapshot.sh -e
```
The snapshot file, _/opt/fio-chronicle/bkup/fc-snapshot-2025-03-16T132648.tar.gz_, will be created in the **/opt/fio-chronicle/bkup** directory containing the FIO.Chronicle Relic DB Exporter state at March 16, 2025, 1:26:48 pm. Note that _/opt/fio-chronicle_ is the default installation direction for FIO.Chronicle.

Other examples include;
```shell
sudo ./scripts/snapshot.sh -e -s /tmp
```
The snapshot file, _/tmp/fc-snapshot-2025-03-16T132648.tar.gz_, will be created in the **/tmp** directory containing the FIO.Chronicle Relic DB Exporter state at March 16, 2025, 1:26:48 pm.

```shell
sudo ./scripts/snapshot.sh -e -s /tmp/relicdb_snapshot
```
The snapshot file, _/tmp/relicdb_snapshot_, will be created containing the FIO.Chronicle Relic DB Exporter state at March 16, 2025, 1:26:48 pm. Note that the file format is a tar gzip file regardless of the extention provided.

To import the FIO.Relic database schema including tables, functions, users and data execute the script, _snapshot.sh_, passing the argument '-i' (for import), and the '-s' argument specifying the snapshot file. For example, to import the data previously exported to the file _/tmp/relicdb_snapshot_, execute the command; 
```shell
sudo ./scripts/snapshot.sh -i -s /tmp/relicdb_snapshot
```

Note: ***The snapshot file is a complete export of the FIO.Relic database. Executing the import script will clear any and all data as well as tables, functions and users!***


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
