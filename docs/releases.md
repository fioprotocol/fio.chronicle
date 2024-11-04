# Release Notes

## Release 1.0

This release is based on Block.one libraries of particular older
versions. It uses `abieos` library from November 13th, with an
additional patch. Newer versions of those libraries are introducing some
incompatible changes, and the work is in progress to adapt Chronicle to
those changes.

## Release 1.1

* Unidirectional mode is no longer supported.

* `skip-to` option is removed.

* `exp_zmq_plugin` is removed because of instable work with Boost ASIO.

* Newest libraries from Block.one repositories are used, and the most
  dramatic change is that channels are processed asynchronously. Also
  all asynchronous tasks must be wrapped in `appbase` priority queue.

* In addition to latest copy of ABI for each contract, the internal
  state database stores a history of all ABI revisions for all
  contracts. This is used in interactive mode.

* New configuration option: `mode` and 3 modes: `scan`, `scan-noexport`,
  and `interactive`. Interactive mode allows requesting individual
  blocks and block ranges.

* New options: `irreversible-only`, `end-block`.

## Release 1.2

This release supports nodeos versions 1.8 ans 2.0, and not compatible with
nodeos-1.7. 

* New message types: 1011, 1012, 1013 (PERMISSION, PERMISSION_LINK,
  ACC_METADATA).

* New field `block_id` in BLOCK and BLOCK_COMPLETED messages.

* New options: `stale-deadline`, `exp-ws-path`, `start-block`,
  `skip-traces`.

* Bugfixes and improvements.

## Release 1.3

* New options for filtering: `enable-receiver-filter`,
  `include-receiver`, `enable-auth-filter`, `include-auth`,
  `blacklist-action`

## Release 1.5

* changed default value for receiver-state-db-size from 1024 to 16384

* new option: blacklist-tables-contract

* Debian package builder and packages published on Github

## Release 1.6

* Replaced external dependencies from B1 repo to our own repo

* Debian package includes `/usr/local/share/chronicle_receiver@.service`

## Release 2.0

* Added compatibility with Leap 3.1

* Added pinned_build scripts, fixating on Boost 1.80.0 and Clang 11.0.1

* The state database is not compatible with 1.6 state, so Chronicle needs to be reinitialized.

## Release 2.1

* Bugfix: primary_key resolved as boolean in decoder_plugin.cpp

## Release 2.2

* Bugfix: ack for a block lower than the previously aknowledged was crashing chronicle

## Release 2.3

* Bugfix: if action arguments or a table row contained trailing garbage, Chronicle failed to decode it.

## Release 2.4

* Bugfix: consumer sending websocket close message causes a crash

## Release 2.5

* Updated external dependencies to match Leap 4.0

## Release 2.6

* Bugfix in integer to JSON conversion: primary key in table deltas lost lower 32 bits. 

## Release 2.7

* Added options: `--save-snapshot` and `--restore-snapshot`

## Release 3.0

* Chronicle 3.0 database is not compatible with that of 2.x, so a
  rescan or restart from a snapshot is required.

* CMake files optimized for customized builds, like the Chronos project.

* Enabled compiler optimization for faster performance.

* Bugfix in Chronicle database: it had an entry for every account even
  if ABI was empty.

* Improvements in socket error handling.

* New event type: `BLOCK_STARTED (1014)`.

* New attributes in `BLOCK_COMPLETED (1010)` event: `producer`,
  `previous`, `transaction_mroot`, `action_mroot`, `trx_count`.

* More frequent pause events; slower pause timer ramp-up.

* default value for receiver-state-db-size set to 1024

* The package is renamed from `eosio-chronicle` to `antelope-chronicle`.

## Release 3.1

* Bugfix in integer to JSON conversion: primary key in table deltas lost lower 32 bits. 

## Release 3.2

* Added options: `--save-snapshot` and `--restore-snapshot`

## Release 3.3

* abieos updated to PR#24, fixing a floating-point overflow bug

