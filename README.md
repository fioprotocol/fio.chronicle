# FIO Chronicle Project

FIO.Chronicle is a software application designed to process the history of the [FIO](https://github.com/fioprotocol/fio) blockchain and is a fork of the [EOSChronicle](https://github.com/EOSChronicleProject/eos-chronicle) project. For more detailed information regarding FIO.Chronicle and its origin, EOSChronicle, see the [overview](https://github.com/fioprotocol/fio.chronicle/blob/develop/docs/overview.md)

# FIO Protocol
The Foundation for Interwallet Operability (FIO) or, in short, the FIO Protocol, is an open-source project based on EOSIO 1.8+.

* For information on FIO Protocol, visit [FIO](https://fio.net).
* For information on the FIO Chain, API, and SDKs, including detailed clone, build and deploy instructions, visit [FIO Protocol Developer Hub](https://dev.fio.net).
* To get updates on the development roadmap, visit [FIO Improvement Proposals](https://github.com/fioprotocol/fips). Anyone is welcome and encouraged to contribute.
* To contribute, please review [Contributing to FIO](CONTRIBUTING.md)
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

### Build and Install Instructions
Minimum build requirements: Cmake 3.11, GCC 8.3.0

Dependencies:
* Boost, version 1.80.0
* Clang, version 11.0.1
* LLVM, version 7.1.0.

#### Build
The build script takes one argument, the directory where to find or install the build dependencies including Boost, Clang, and LLVM. It is recommended to use a non-system level directory such as '/opt'. Note that any future builds, if given the same directory, will reuse those build dependencies.

To build fio.chronicle, execute the following command;
```shell
./scripts/build.sh /opt
```

#### Install
The installation of Fio.Chronicle has two parts,
1) configuration
2) installation

##### Configuration
The configuration of Fio.Chronicle is designated via options specified on the command-line as well as captured in a config.ini that is read as part of start up. The configuration options include, but are limited to, the following;
Command-Line Options;
* --config-dir=\<directory where to find the config.ini\>
* --data-dir=\<directory where to store data\>

Config.Ini Options;
* host = \<the nodeos state history host (upstream connection to fio nodeos state history api endpoint)\>
* port = \<the nodeos state history api port (upstream connection to fio nodeos state history api endpoint port)\>
* exp-ws-host = \<the websocket server host (the downstream connnection to a web socket server host)\>
* exp-ws-port = \<the websocket server port (the downstream connnection to a web socket server port)\>

The options specified above would suit a minimal configuration but other configuration options may be found [here](docs/config-options.md). The config.ini would be as follows;
```shell
host = 127.0.0.1
port = 8080
mode = scan
plugin = exp_ws_plugin
exp-ws-host = 127.0.0.1
exp-ws-port = 8891
exp-ws-bin-header = false
```
It connects to `nodeos` process running `state_history_plugin` at `localhost:8080` and exports the data to a websocket server at `localhost:8800`. In a production environment, hosts may be different machines in the network.

##### Installation
To install fio.chronicle along with the default config.ini file, to /opt/fio-chronicle, execute the following command;
```shell
./scripts/install.sh
```

If desired, you may install the executable to '/usr/local/bin' by doing the following;
```shell
cd build
sudo make install
```
Note that the above command will not install the config.ini. This must be manually.
