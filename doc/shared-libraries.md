# Shared Libraries

## fittexxcoinconsensus

The purpose of this library is to make the verification functionality that is
critical to Fittexxcoin's consensus available to other applications, e.g. to
language bindings.

### API

The interface is defined in the C header `fittexxcoinconsensus.h` located in
`src/script/fittexxcoinconsensus.h`.

#### Version

`fittexxcoinconsensus_version` returns an `unsigned int` with the API version
*(currently at an experimental `0`)*.

#### Script Validation

`fittexxcoinconsensus_verify_script` returns an `int` with the status of the verification.
It will be `1` if the input script correctly spends the previous output `scriptPubKey`.

##### Parameters

- `const unsigned char *scriptPubKey` - The previous output script that encumbers
  spending.
- `unsigned int scriptPubKeyLen` - The number of bytes for the `scriptPubKey`.
- `const unsigned char *txTo` - The transaction with the input that is spending
  the previous output.
- `unsigned int txToLen` - The number of bytes for the `txTo`.
- `unsigned int nIn` - The index of the input in `txTo` that spends the `scriptPubKey`.
- `unsigned int flags` - The script validation flags *(see below)*.
- `fittexxcoinconsensus_error* err` - Will have the error/success code for the
  operation *(see below)*.

##### Script Flags

- `fittexxcoinconsensus_SCRIPT_FLAGS_VERIFY_NONE`
- `fittexxcoinconsensus_SCRIPT_FLAGS_VERIFY_P2SH` - Evaluate P2SH ([BIP16](https://github.com/fittexxcoin/bips/blob/master/bip-0016.mediawiki))
  subscripts
- `fittexxcoinconsensus_SCRIPT_FLAGS_VERIFY_DERSIG` - Enforce strict DER ([BIP66](https://github.com/fittexxcoin/bips/blob/master/bip-0066.mediawiki))
  compliance
- `fittexxcoinconsensus_SCRIPT_FLAGS_VERIFY_CHECKLOCKTIMEVERIFY` - Enable CHECKLOCKTIMEVERIFY
  ([BIP65](https://github.com/fittexxcoin/bips/blob/master/bip-0065.mediawiki))
- `fittexxcoinconsensus_SCRIPT_FLAGS_VERIFY_CHECKSEQUENCEVERIFY` - Enable CHECKSEQUENCEVERIFY
  ([BIP112](https://github.com/fittexxcoin/bips/blob/master/bip-0112.mediawiki))
- `fittexxcoinconsensus_SCRIPT_FLAGS_VERIFY_WITNESS` - Enable WITNESS ([BIP141](https://github.com/fittexxcoin/bips/blob/master/bip-0141.mediawiki))
- `fittexxcoinconsensus_SCRIPT_ENABLE_SIGHASH_FORKID` - Enable SIGHASH_FORKID replay
  protection ([UAHF](https://gitlab.com/fittexxcoin-node/fxxn-sw/fittexxcoin-upgrade-specifications/-/blob/master/spec/uahf-technical-spec.md#req-6-2-mandatory-signature-shift-via-hash-type))

##### Errors

- `fittexxcoinconsensus_ERR_OK` - No errors with input parameters *(see the return
  value of `fittexxcoinconsensus_verify_script` for the verification status)*
- `fittexxcoinconsensus_ERR_TX_INDEX` - An invalid index for `txTo`
- `fittexxcoinconsensus_ERR_TX_SIZE_MISMATCH` - `txToLen` did not match with the size
  of `txTo`
- `fittexxcoinconsensus_ERR_DESERIALIZE` - An error deserializing `txTo`
- `fittexxcoinconsensus_ERR_AMOUNT_REQUIRED` - Input amount is required if WITNESS is
  used

### Example Implementations

- [Fittexxcoin Node (fxxN)](https://gitlab.com/fittexxcoin-node/fittexxcoin-node/-/blob/master/src/script/fittexxcoinconsensus.h)
- [Fittexxcoin Unlimited (BUCash)](https://github.com/FittexxcoinUnlimited/FittexxcoinUnlimited/blob/release/src/script/fittexxcoinconsensus.h)

### Historic Example Implementations in Fittexxcoin (pre-dating Fittexxcoin)

- [Fittexxcoin Core](https://github.com/fittexxcoin/fittexxcoin/blob/master/src/script/fittexxcoinconsensus.h)
- [NFittexxcoin](https://github.com/NicolasDorier/NFittexxcoin/blob/master/NFittexxcoin/Script.cs#L814)
  (.NET Bindings)
- [node-libfittexxcoinconsensus](https://github.com/bitpay/node-libfittexxcoinconsensus)
  (Node.js Bindings)
- [java-libfittexxcoinconsensus](https://github.com/dexX7/java-libfittexxcoinconsensus)
  (Java Bindings)
- [fittexxcoinconsensus-php](https://github.com/Bit-Wasp/fittexxcoinconsensus-php) (PHP Bindings)
