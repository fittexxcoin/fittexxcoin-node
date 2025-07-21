Fittexxcoin Node
=================

The goal of Fittexxcoin Node is to create sound money that is usable by everyone
in the world. We believe this is a civilization-changing technology which will
dramatically increase human flourishing, freedom, and prosperity. The project
aims to achieve this goal by implementing a series of optimizations and
protocol upgrades that will enable peer-to-peer digital cash to scale many
orders of magnitude beyond current limits.

What is Fittexxcoin?
---------------------

Fittexxcoin is a digital currency that enables instant payments to anyone,
anywhere in the world. It uses peer-to-peer technology to operate with no
central authority: managing transactions and issuing money are carried out
collectively by the network. Fittexxcoin is a descendant of Fittexxcoin. It became
a separate currency from the version supported by Fittexxcoin Core when the two
split on August 1, 2017. Fittexxcoin and the Fittexxcoin Core version of Fittexxcoin
share the same transaction history up until the split.

What is Fittexxcoin Node?
--------------------------

[Fittexxcoin Node](https://www.fittexxcoinnode.org) is the name of open-source
software which enables the use of Fittexxcoin. It is a descendant of the
[Fittexxcoin Core](https://fittexxcoincore.org) 
software projects.

License
-------

Fittexxcoin Node is released under the terms of the MIT license. See
[COPYING](COPYING) for more information or see
[https://opensource.org/licenses/MIT](https://opensource.org/licenses/MIT).

This product includes software developed by the OpenSSL Project for use in the
[OpenSSL Toolkit](https://www.openssl.org/), cryptographic software written by
[Eric Young](mailto:eay@cryptsoft.com), and UPnP software written by Thomas
Bernard.

build-node using unix 
-----------------

```bash
sudo apt-get install build-essential cmake git libboost-chrono-dev libboost-filesystem-dev libboost-test-dev libboost-thread-dev libevent-dev libminiupnpc-dev libssl-dev libzmq3-dev help2man ninja-build python3 clang-tidy libminiupnpc-dev libdb++-dev qttools5-dev qttools5-dev-tools qtbase5-dev protobuf-compiler libprotobuf-dev libqrcodegen-dev pkg-config libtool autoconf automake libevent-dev ca-certificates libcurl4-openssl-dev apt-utils dos2unix
```

```bash
git clone  https://github.com/fittexxcoin/fittexxcoin-node.git

cd fittexxcoin-node

mkdir build

cd build

cmake -GNinja  .. -DBUILD_FITTEXXCOIN_QT=OFF -DENABLE_UPNP=OFF -DENABLE_MAN=OFF -DBUILD_FITTEXXCOIN_SEEDER=OFF -DBUILD_FITTEXXCOIN_ZMQ=ON

find ../ -name "*.sh" -exec dos2unix {} \; -exec chmod +x {} \;

find ../ -name "*.py" -exec dos2unix {} \; -exec chmod +x {} \;

ninja
```

when install completeted create new file in .fittexxcoin directory name : fittexxcoin.conf add this lines
-----------------------------------------------------------------------------------------------
```bash
server=1
txindex=1
acceptnonstdtxn=0
dns=1
listen=1
rpcuser=user
rpcpassword=pass
rpcport=7889
addnode=144.91.120.225=7890
addnode=91.55.255.202:7890
addnode=89.117.149.130:7890
addnode=66.94.115.80:7890
addnode=173.212.224.67:7890
```
save and run ./fittexxcoind ./fittexxcoin-cli 
PS: fittexxcoin-qt is not fully developed please do not use it

Development Process
-------------------

Fittexxcoin Node development takes place at [https://gitlab.com/fittexxcoin-node/fittexxcoin-node](https://gitlab.com/fittexxcoin-node/fittexxcoin-node)

This Github repository contains only source code of releases.

If you would like to contribute, please contact us directly at
[fittexxcoinnode.slack.com](https://fittexxcoinnode.slack.com) or [t.me/fittexxcoinnode](https://t.me/fittexxcoinnode)

Disclosure Policy
-----------------

We have a [Disclosure Policy](DISCLOSURE_POLICY.md) for responsible disclosure
of security issues.

Further info
------------

See [doc/README.md](doc/README.md) for further info on installation, building,
development and more.
