# Chainparams Constants

Utilities to generate chainparams constants that are compiled into the client
(see [src/chainparamsconstants.h](/src/chainparamsconstants.h).

The chainparams constants are fetched from fittexxcoind, dumped to intermediate
files, and then compiled into [src/chainparamsconstants.h](/src/chainparamsconstants.h).
If you're running fittexxcoind locally, the following instructions will work
out-of-the-box:

## Mainnet

```
fittexxcoind
python3 make_chainparams.py > chainparams_main.txt
```

## Testnet3

```
fittexxcoind --testnet
python3 make_chainparams.py -a 127.0.0.1:17889 > chainparams_test.txt
```

## Testnet4

```
fittexxcoind --testnet4
python3 make_chainparams.py -a 127.0.0.1:27889 > chainparams_testnet4.txt
```

## Scalenet

```
fittexxcoind --scalenet
python3 make_chainparams.py -a 127.0.0.1:37889 > chainparams_scalenet.txt
```

**Note**: Scalenet should not be updated since it already has the chainparams it
needs to be reorged back to height 10,000.  Without manually editing to comment-out
the sys.exit call, the above script will exit with an error message if executed
against a `fittexxcoind` that is on scalenet.

## Build C++ Header File

```
python3 generate_chainparams_constants.py . > ../../../src/chainparamsconstants.h
```

## Testing

Updating these constants should be reviewed carefully, with a
reindex-chainstate, checkpoints=0, and assumevalid=0 to catch any defect that
causes rejection of blocks in the past history.
