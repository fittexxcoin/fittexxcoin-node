#!/usr/bin/env python3
# Copyright (c) 2015-2019 The Fittexxcoin Core developers
# Copyright (c) 2020 The Fittexxcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test decoding scripts via decodescript RPC command."""

from test_framework.messages import CTransaction, FromHex, ToHex
from test_framework.test_framework import FittexxcoinTestFramework
from test_framework.util import assert_equal, hex_str_to_bytes


class DecodeScriptTest(FittexxcoinTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 1

    def decodescript_script_sig(self):
        signature = '304502207fa7a6d1e0ee81132a269ad84e68d695483745cde8b541e3bf630749894e342a022100c1f7ab20e13e22fb95281a870f3dcf38d782e53023ee313d741ad0cfbc0c509001'
        push_signature = '48' + signature
        public_key = '03b0da749730dc9b4b1f4a14d6902877a92541f5368778853d9c4a0cb7802dcfb2'
        push_public_key = '21' + public_key

        # below are test cases for all of the standard transaction types

        # 1) P2PK scriptSig
        # the scriptSig of a public key scriptPubKey simply pushes a signature
        # onto the stack
        rpc_result = self.nodes[0].decodescript(push_signature)
        assert_equal(signature, rpc_result['asm'])
        assert 'reqSigs' not in rpc_result
        assert_equal('nonstandard', rpc_result['type'])
        assert 'addresses' not in rpc_result
        assert_equal('fxxreg:pqcelzf04c0k40ngx4f24dc8zr2mqstqyusveenvss', rpc_result['p2sh'])

        # 2) P2PKH scriptSig
        rpc_result = self.nodes[0].decodescript(push_signature + push_public_key)
        assert_equal(signature + ' ' + public_key, rpc_result['asm'])
        assert 'reqSigs' not in rpc_result
        assert_equal('nonstandard', rpc_result['type'])
        assert 'addresses' not in rpc_result
        assert_equal('fxxreg:ppzuwl0gnrzrkvac57rj4vvgpmj7gaulrsqcud3lwf', rpc_result['p2sh'])

        # 3) multisig scriptSig
        # this also tests the leading portion of a P2SH multisig scriptSig
        # OP_0 <A sig> <B sig>
        rpc_result = self.nodes[0].decodescript('00' + push_signature + push_signature)
        assert_equal('0 ' + signature + ' ' + signature, rpc_result['asm'])
        assert 'reqSigs' not in rpc_result
        assert_equal('nonstandard', rpc_result['type'])
        assert 'addresses' not in rpc_result
        assert_equal('fxxreg:pq8n9kr6ye6heaxa8s64d225thxz7t64nql7350qz7', rpc_result['p2sh'])

        # 4) P2SH scriptSig
        # an empty P2SH redeemScript is valid and makes for a very simple test case.
        # thus, such a spending scriptSig would just need to pass the outer redeemScript
        # hash test and leave true on the top of the stack.
        rpc_result = self.nodes[0].decodescript('5100')
        assert_equal('1 0', rpc_result['asm'])
        assert 'reqSigs' not in rpc_result
        assert_equal('nonstandard', rpc_result['type'])
        assert 'addresses' not in rpc_result
        assert_equal('fxxreg:pq42f3g7mt8h9qaf3dxq9u3dk8fm22unvqv2l22pjr', rpc_result['p2sh'])

        # 5) null data scriptSig - no such thing because null data scripts can not be spent.
        # thus, no test case for that standard transaction type is here.

    def decodescript_script_pub_key(self):
        public_key = '03b0da749730dc9b4b1f4a14d6902877a92541f5368778853d9c4a0cb7802dcfb2'
        push_public_key = '21' + public_key
        public_key_hash = '11695b6cd891484c2d49ec5aa738ec2b2f897777'
        push_public_key_hash = '14' + public_key_hash

        # below are test cases for all of the standard transaction types

        # 1) P2PK scriptPubKey
        # <pubkey> OP_CHECKSIG
        rpc_result = self.nodes[0].decodescript(push_public_key + 'ac')
        assert_equal(public_key + ' OP_CHECKSIG', rpc_result['asm'])
        assert_equal(1, rpc_result['reqSigs'])
        assert_equal('pubkey', rpc_result['type'])
        assert_equal(['fxxreg:qpwar5aqfqgecfajs2fs2eeym9fz7fkeg565hwlcwt'], rpc_result['addresses'])
        assert_equal('fxxreg:pr7a3024aymtt7jw2j37pktad4apv479aymf867e30', rpc_result['p2sh'])

        # 2) P2PKH scriptPubKey
        # OP_DUP OP_HASH160 <PubKeyHash> OP_EQUALVERIFY OP_CHECKSIG
        rpc_result = self.nodes[0].decodescript('76a9' + push_public_key_hash + '88ac')
        assert_equal('OP_DUP OP_HASH160 ' + public_key_hash + ' OP_EQUALVERIFY OP_CHECKSIG', rpc_result['asm'])
        assert_equal(1, rpc_result['reqSigs'])
        assert_equal('pubkeyhash', rpc_result['type'])
        assert_equal(['fxxreg:qqgkjkmvmzg5snpdf8k94fecas4jlzthwuwpda8j83'], rpc_result['addresses'])
        assert_equal('fxxreg:pqpylx2y5cdcc6e6mxxzdglu2t6uxhrnh5xmaq84mw', rpc_result['p2sh'])

        # 3) multisig scriptPubKey
        # <m> <A pubkey> <B pubkey> <C pubkey> <n> OP_CHECKMULTISIG
        # just imagine that the pub keys used below are different.
        # for our purposes here it does not matter that they are the same even
        # though it is unrealistic.
        rpc_result = self.nodes[0].decodescript('52' + push_public_key * 3 + '53ae')
        assert_equal('2 ' + public_key + ' ' + public_key + ' ' + public_key + ' 3 OP_CHECKMULTISIG', rpc_result['asm'])
        assert_equal(2, rpc_result['reqSigs'])
        assert_equal('multisig', rpc_result['type'])
        assert_equal(['fxxreg:qpwar5aqfqgecfajs2fs2eeym9fz7fkeg565hwlcwt'] * 3, rpc_result['addresses'])
        assert_equal('fxxreg:pqewz2rkyl3988qlmve9lfyaagtk7m8xl5vc0lceaf', rpc_result['p2sh'])

        # 4) P2SH scriptPubKey
        # OP_HASH160 <Hash160(redeemScript)> OP_EQUAL.
        # push_public_key_hash here should actually be the hash of a redeem script.
        # but this works the same for purposes of this test.
        rpc_result = self.nodes[0].decodescript('a9' + push_public_key_hash + '87')
        assert_equal('OP_HASH160 ' + public_key_hash + ' OP_EQUAL', rpc_result['asm'])
        assert_equal(1, rpc_result['reqSigs'])
        assert_equal('scripthash', rpc_result['type'])
        assert_equal(['fxxreg:pqgkjkmvmzg5snpdf8k94fecas4jlzthwueysjq3uv'], rpc_result['addresses'])
        assert 'p2sh' not in rpc_result

        # 5) null data scriptPubKey
        # use a signature look-alike here to make sure that we do not decode random data as a signature.
        # this matters if/when signature sighash decoding comes along.
        # would want to make sure that no such decoding takes place in this
        # case.
        signature_imposter = '48304502207fa7a6d1e0ee81132a269ad84e68d695483745cde8b541e3bf630749894e342a022100c1f7ab20e13e22fb95281a870f3dcf38d782e53023ee313d741ad0cfbc0c509001'
        # OP_RETURN <data>
        rpc_result = self.nodes[0].decodescript('6a' + signature_imposter)
        assert_equal('OP_RETURN ' + signature_imposter[2:], rpc_result['asm'])
        assert 'reqSigs' not in rpc_result
        assert_equal('nulldata', rpc_result['type'])
        assert 'addresses' not in rpc_result
        assert_equal('fxxreg:pzevmdfm7rq8cnsl00gp8jdlw2fvshhmh5pchcjpq7', rpc_result['p2sh'])

        # 6) a CLTV redeem script. redeem scripts are in-effect scriptPubKey scripts, so adding a test here.
        # OP_NOP2 is also known as OP_CHECKLOCKTIMEVERIFY.
        # just imagine that the pub keys used below are different.
        # for our purposes here it does not matter that they are the same even though it is unrealistic.
        #
        # OP_IF
        #   <receiver-pubkey> OP_CHECKSIGVERIFY
        # OP_ELSE
        #   <lock-until> OP_CHECKLOCKTIMEVERIFY OP_DROP
        # OP_ENDIF
        # <sender-pubkey> OP_CHECKSIG
        #
        # lock until block 500,000
        rpc_result = self.nodes[0].decodescript('63' + push_public_key + 'ad670320a107b17568' + push_public_key + 'ac')
        assert_equal(
            'OP_IF ' +
            public_key +
            ' OP_CHECKSIGVERIFY OP_ELSE 500000 OP_CHECKLOCKTIMEVERIFY OP_DROP OP_ENDIF ' +
            public_key +
            ' OP_CHECKSIG',
            rpc_result['asm'])
        assert 'reqSigs' not in rpc_result
        assert_equal('nonstandard', rpc_result['type'])
        assert 'addresses' not in rpc_result
        assert_equal('fxxreg:pq8s6jh54dp8u2693h8v90r68dlr8vn9dq5h6x2leg', rpc_result['p2sh'])

    def decoderawtransaction_asm_sighashtype(self):
        """Test decoding scripts via RPC command "decoderawtransaction".

        This test is in with the "decodescript" tests because they are testing the same "asm" script decodes.
        """

        # this test case uses a random plain vanilla mainnet transaction with a
        # single P2PKH input and output
        tx = '0100000001696a20784a2c70143f634e95227dbdfdf0ecd51647052e70854512235f5986ca010000008a47304402207174775824bec6c2700023309a168231ec80b82c6069282f5133e6f11cbb04460220570edc55c7c5da2ca687ebd0372d3546ebc3f810516a002350cac72dfe192dfb014104d3f898e6487787910a690410b7a917ef198905c27fb9d3b0a42da12aceae0544fc7088d239d9a48f2828a15a09e84043001f27cc80d162cb95404e1210161536ffffffff0100e1f505000000001976a914eb6c6e0cdb2d256a32d97b8df1fc75d1920d9bca88ac00000000'
        rpc_result = self.nodes[0].decoderawtransaction(tx)
        assert_equal(
            '304402207174775824bec6c2700023309a168231ec80b82c6069282f5133e6f11cbb04460220570edc55c7c5da2ca687ebd0372d3546ebc3f810516a002350cac72dfe192dfb[ALL] 04d3f898e6487787910a690410b7a917ef198905c27fb9d3b0a42da12aceae0544fc7088d239d9a48f2828a15a09e84043001f27cc80d162cb95404e1210161536', rpc_result['vin'][0]['scriptSig']['asm'])

        # this test case uses a mainnet transaction that has a P2SH input and both P2PKH and P2SH outputs.
        # it's from James D'Angelo's awesome introductory videos about multisig: https://www.youtube.com/watch?v=zIbUSaZBJgU and https://www.youtube.com/watch?v=OSA1pwlaypc
        # verify that we have not altered scriptPubKey decoding.
        tx = '01000000018d1f5635abd06e2c7e2ddf58dc85b3de111e4ad6e0ab51bb0dcf5e84126d927300000000fdfe0000483045022100ae3b4e589dfc9d48cb82d41008dc5fa6a86f94d5c54f9935531924602730ab8002202f88cf464414c4ed9fa11b773c5ee944f66e9b05cc1e51d97abc22ce098937ea01483045022100b44883be035600e9328a01b66c7d8439b74db64187e76b99a68f7893b701d5380220225bf286493e4c4adcf928c40f785422572eb232f84a0b83b0dea823c3a19c75014c695221020743d44be989540d27b1b4bbbcfd17721c337cb6bc9af20eb8a32520b393532f2102c0120a1dda9e51a938d39ddd9fe0ebc45ea97e1d27a7cbd671d5431416d3dd87210213820eb3d5f509d7438c9eeecb4157b2f595105e7cd564b3cdbb9ead3da41eed53aeffffffff02611e0000000000001976a914dc863734a218bfe83ef770ee9d41a27f824a6e5688acee2a02000000000017a9142a5edea39971049a540474c6a99edf0aa4074c588700000000'
        rpc_result = self.nodes[0].decoderawtransaction(tx)
        assert_equal(
            '8e3730608c3b0bb5df54f09076e196bc292a8e39a78e73b44b6ba08c78f5cbb0', rpc_result['txid'])
        assert_equal(
            '0 3045022100ae3b4e589dfc9d48cb82d41008dc5fa6a86f94d5c54f9935531924602730ab8002202f88cf464414c4ed9fa11b773c5ee944f66e9b05cc1e51d97abc22ce098937ea[ALL] 3045022100b44883be035600e9328a01b66c7d8439b74db64187e76b99a68f7893b701d5380220225bf286493e4c4adcf928c40f785422572eb232f84a0b83b0dea823c3a19c75[ALL] 5221020743d44be989540d27b1b4bbbcfd17721c337cb6bc9af20eb8a32520b393532f2102c0120a1dda9e51a938d39ddd9fe0ebc45ea97e1d27a7cbd671d5431416d3dd87210213820eb3d5f509d7438c9eeecb4157b2f595105e7cd564b3cdbb9ead3da41eed53ae', rpc_result['vin'][0]['scriptSig']['asm'])
        assert_equal(
            'OP_DUP OP_HASH160 dc863734a218bfe83ef770ee9d41a27f824a6e56 OP_EQUALVERIFY OP_CHECKSIG',
            rpc_result['vout'][0]['scriptPubKey']['asm'])
        assert_equal(
            'OP_HASH160 2a5edea39971049a540474c6a99edf0aa4074c58 OP_EQUAL',
            rpc_result['vout'][1]['scriptPubKey']['asm'])
        txSave = FromHex(CTransaction(), tx)

        # make sure that a specifically crafted op_return value will not pass
        # all the IsDERSignature checks and then get decoded as a sighash type
        tx = '01000000015ded05872fdbda629c7d3d02b194763ce3b9b1535ea884e3c8e765d42e316724020000006b48304502204c10d4064885c42638cbff3585915b322de33762598321145ba033fc796971e2022100bb153ad3baa8b757e30a2175bd32852d2e1cb9080f84d7e32fcdfd667934ef1b012103163c0ff73511ea1743fb5b98384a2ff09dd06949488028fd819f4d83f56264efffffffff0200000000000000000b6a0930060201000201000180380100000000001976a9141cabd296e753837c086da7a45a6c2fe0d49d7b7b88ac00000000'
        rpc_result = self.nodes[0].decoderawtransaction(tx)
        assert_equal('OP_RETURN 300602010002010001',
                     rpc_result['vout'][0]['scriptPubKey']['asm'])

        # verify that we have not altered scriptPubKey processing even of a
        # specially crafted P2PKH pubkeyhash and P2SH redeem script hash that
        # is made to pass the der signature checks
        tx = '01000000018d1f5635abd06e2c7e2ddf58dc85b3de111e4ad6e0ab51bb0dcf5e84126d927300000000fdfe0000483045022100ae3b4e589dfc9d48cb82d41008dc5fa6a86f94d5c54f9935531924602730ab8002202f88cf464414c4ed9fa11b773c5ee944f66e9b05cc1e51d97abc22ce098937ea01483045022100b44883be035600e9328a01b66c7d8439b74db64187e76b99a68f7893b701d5380220225bf286493e4c4adcf928c40f785422572eb232f84a0b83b0dea823c3a19c75014c695221020743d44be989540d27b1b4bbbcfd17721c337cb6bc9af20eb8a32520b393532f2102c0120a1dda9e51a938d39ddd9fe0ebc45ea97e1d27a7cbd671d5431416d3dd87210213820eb3d5f509d7438c9eeecb4157b2f595105e7cd564b3cdbb9ead3da41eed53aeffffffff02611e0000000000001976a914301102070101010101010102060101010101010188acee2a02000000000017a91430110207010101010101010206010101010101018700000000'
        rpc_result = self.nodes[0].decoderawtransaction(tx)
        assert_equal(
            'OP_DUP OP_HASH160 3011020701010101010101020601010101010101 OP_EQUALVERIFY OP_CHECKSIG',
            rpc_result['vout'][0]['scriptPubKey']['asm'])
        assert_equal(
            'OP_HASH160 3011020701010101010101020601010101010101 OP_EQUAL',
            rpc_result['vout'][1]['scriptPubKey']['asm'])

        # some more full transaction tests of varying specific scriptSigs. used instead of
        # tests in decodescript_script_sig because the decodescript RPC is specifically
        # for working on scriptPubKeys (argh!).
        push_signature = txSave.vin[0].scriptSig.hex()[2:(0x48 * 2 + 4)]
        signature = push_signature[2:]
        der_signature = signature[:-2]
        signature_sighash_decoded = der_signature + '[ALL]'
        signature_2 = der_signature + '82'
        push_signature_2 = '48' + signature_2
        signature_2_sighash_decoded = der_signature + '[NONE|ANYONECANPAY]'

        # 1) P2PK scriptSig
        txSave.vin[0].scriptSig = hex_str_to_bytes(push_signature)
        rpc_result = self.nodes[0].decoderawtransaction(ToHex(txSave))
        assert_equal(signature_sighash_decoded,
                     rpc_result['vin'][0]['scriptSig']['asm'])

        # make sure that the sighash decodes come out correctly for a more
        # complex / lesser used case.
        txSave.vin[0].scriptSig = hex_str_to_bytes(push_signature_2)
        rpc_result = self.nodes[0].decoderawtransaction(ToHex(txSave))
        assert_equal(signature_2_sighash_decoded,
                     rpc_result['vin'][0]['scriptSig']['asm'])

        # 2) multisig scriptSig
        txSave.vin[0].scriptSig = hex_str_to_bytes(
            '00' + push_signature + push_signature_2)
        rpc_result = self.nodes[0].decoderawtransaction(ToHex(txSave))
        assert_equal('0 ' + signature_sighash_decoded + ' ' +
                     signature_2_sighash_decoded, rpc_result['vin'][0]['scriptSig']['asm'])

        # 3) test a scriptSig that contains more than push operations.
        # in fact, it contains an OP_RETURN with data specially crafted to
        # cause improper decode if the code does not catch it.
        txSave.vin[0].scriptSig = hex_str_to_bytes(
            '6a143011020701010101010101020601010101010101')
        rpc_result = self.nodes[0].decoderawtransaction(ToHex(txSave))
        assert_equal('OP_RETURN 3011020701010101010101020601010101010101',
                     rpc_result['vin'][0]['scriptSig']['asm'])

    def run_test(self):
        self.decodescript_script_sig()
        self.decodescript_script_pub_key()
        self.decoderawtransaction_asm_sighashtype()


if __name__ == '__main__':
    DecodeScriptTest().main()
