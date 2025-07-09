# Fittexxcoin Node Setup

Fittexxcoin Node is a node and wallet implementation for the Fittexxcoin network.
It downloads and, by default, stores the entire history of Fittexxcoin
transactions, which requires a few hundred gigabytes of disk space. Depending on
the speed of your computer and network connection, the synchronization process
can take anywhere from a few hours to a day or more.

To download Fittexxcoin Node, visit [fittexxcoinnode.org](https://fittexxcoinnode.org/).

## Verify

If you download the associated signature files with the binaries from the above link,
you can verify the integrity of the binaries by following these instructions, replacing
VERSION with the value relevant to you:

Get the keys for versions 25.0.0 or later:

```
VERSION="25.0.0"
URL="https://download.fittexxcoinnode.org/releases/${VERSION}/src/fittexxcoin-node-${VERSION}.tar.gz"
KEYS_FILE="fittexxcoin-node-${VERSION}/contrib/gitian-signing/keys.txt"
wget -q -O - "${URL}" | tar -zxOf - "${KEYS_FILE}" | while read FINGERPRINT _; do gpg --recv-keys "${FINGERPRINT}"; done
```

Get the keys for version 25.0.0:

```
URL="https://download.fittexxcoinnode.org/keys/keys.txt"
wget -q -O - "${URL}" | while read FINGERPRINT _; do gpg --recv-keys "${FINGERPRINT}"; done
```

Check the binaries (all versions):

```
FILE_PATTERN="./*-sha256sums.${VERSION}.asc"
gpg --verify-files ${FILE_PATTERN}
grep "fittexxcoin-node-${VERSION}" ${FILE_PATTERN} | cut -d " " -f 2- | xargs ls 2> /dev/null |\
  xargs -i grep -h "{}" ${FILE_PATTERN} | uniq | sha256sum -c
```

*IMPORTANT NOTE:* The first time you run this, all of the signing keys will be
UNTRUSTED and you will see warnings indicating this. For best security practices,
you should `gpg --sign-key <signer key>` for each release signer key and rerun
the above script (there should be no warnings the second time). If the keys change
unexpectedly, the presence of those warnings should be heeded with extreme caution.

## Running

The following are some helpful notes on how to run Fittexxcoin Node on your
native platform.

### Unix

Unpack the files into a directory and run:

- `bin/fittexxcoin-qt` (GUI) or
- `bin/fittexxcoind` (headless)

### Windows

Unpack the files into a directory, and then run `fittexxcoin-qt.exe`.

### macOS

Drag `fittexxcoin-node` to your applications folder, and then run `fittexxcoin-node`.

## Help

- Ask for help on the [Fittexxcoin Node Subreddit](https://www.reddit.com/r/fxxnode/), [Fittexxcoin Node Slack](https://join.slack.com/t/fittexxcoinnode/shared_invite/zt-egg3c36d-2cglIrKcbnGpIQFaKFzCWA) or [Fittexxcoin Node Telegram](https://t.me/fittexxcoinnode).
