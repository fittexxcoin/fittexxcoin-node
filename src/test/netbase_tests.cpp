// Copyright (c) 2012-2016 The Fittexxcoin Core developers
// Copyright (c) 2017-2022 The Fittexxcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <netbase.h>

#include <net_permissions.h>
#include <protocol.h>
#include <serialize.h>
#include <streams.h>
#include <util/bit_cast.h>
#include <util/strencodings.h>
#include <version.h>

#include <test/setup_common.h>

#include <boost/test/unit_test.hpp>

#include <string>

BOOST_FIXTURE_TEST_SUITE(netbase_tests, BasicTestingSetup)

static CNetAddr ResolveIP(const std::string &ip) {
    CNetAddr addr;
    LookupHost(ip, addr, false);
    return addr;
}

static CSubNet ResolveSubNet(const std::string &subnet) {
    CSubNet ret;
    LookupSubNet(subnet, ret);
    return ret;
}

static CNetAddr CreateInternal(const std::string &host) {
    CNetAddr addr;
    addr.SetInternal(host);
    return addr;
}

BOOST_AUTO_TEST_CASE(netbase_networks) {
    BOOST_CHECK(ResolveIP("127.0.0.1").GetNetwork() == NET_UNROUTABLE);
    BOOST_CHECK(ResolveIP("::1").GetNetwork() == NET_UNROUTABLE);
    BOOST_CHECK(ResolveIP("8.8.8.8").GetNetwork() == NET_IPV4);
    BOOST_CHECK(ResolveIP("2001::8888").GetNetwork() == NET_IPV6);
    BOOST_CHECK(
        ResolveIP("FD87:D87E:EB43:edb1:8e4:3588:e546:35ca").GetNetwork() ==
        NET_ONION);
    BOOST_CHECK(CreateInternal("foo.com").GetNetwork() == NET_INTERNAL);
}

BOOST_AUTO_TEST_CASE(netbase_properties) {
    BOOST_CHECK(ResolveIP("127.0.0.1").IsIPv4());
    BOOST_CHECK(ResolveIP("::FFFF:192.168.1.1").IsIPv4());
    BOOST_CHECK(ResolveIP("::1").IsIPv6());
    BOOST_CHECK(ResolveIP("10.0.0.1").IsRFC1918());
    BOOST_CHECK(ResolveIP("192.168.1.1").IsRFC1918());
    BOOST_CHECK(ResolveIP("172.31.255.255").IsRFC1918());
    BOOST_CHECK(ResolveIP("2001:0DB8::").IsRFC3849());
    BOOST_CHECK(ResolveIP("169.254.1.1").IsRFC3927());
    BOOST_CHECK(ResolveIP("2002::1").IsRFC3964());
    BOOST_CHECK(ResolveIP("FC00::").IsRFC4193());
    BOOST_CHECK(ResolveIP("2001::2").IsRFC4380());
    BOOST_CHECK(ResolveIP("2001:10::").IsRFC4843());
    BOOST_CHECK(ResolveIP("2001:20::").IsRFC7343());
    BOOST_CHECK(ResolveIP("FE80::").IsRFC4862());
    BOOST_CHECK(ResolveIP("64:FF9B::").IsRFC6052());
    BOOST_CHECK(ResolveIP("FD87:D87E:EB43:edb1:8e4:3588:e546:35ca").IsTor());
    BOOST_CHECK(ResolveIP("127.0.0.1").IsLocal());
    BOOST_CHECK(ResolveIP("::1").IsLocal());
    BOOST_CHECK(ResolveIP("8.8.8.8").IsRoutable());
    BOOST_CHECK(ResolveIP("2001::1").IsRoutable());
    BOOST_CHECK(ResolveIP("127.0.0.1").IsValid());
    BOOST_CHECK(
        CreateInternal("FD6B:88C0:8724:edb1:8e4:3588:e546:35ca").IsInternal());
    BOOST_CHECK(CreateInternal("bar.com").IsInternal());
}

static bool TestSplitHost(const std::string &test, const std::string &host, int port) {
    std::string hostOut;
    int portOut = -1;
    SplitHostPort(test, portOut, hostOut);
    return hostOut == host && port == portOut;
}

BOOST_AUTO_TEST_CASE(netbase_splithost) {
    BOOST_CHECK(TestSplitHost("www.fittexxcoin.org", "www.fittexxcoin.org", -1));
    BOOST_CHECK(TestSplitHost("[www.fittexxcoin.org]", "www.fittexxcoin.org", -1));
    BOOST_CHECK(TestSplitHost("www.fittexxcoin.org:80", "www.fittexxcoin.org", 80));
    BOOST_CHECK(TestSplitHost("[www.fittexxcoin.org]:80", "www.fittexxcoin.org", 80));
    BOOST_CHECK(TestSplitHost("127.0.0.1", "127.0.0.1", -1));
    BOOST_CHECK(TestSplitHost("127.0.0.1:7890", "127.0.0.1", 7890));
    BOOST_CHECK(TestSplitHost("[127.0.0.1]", "127.0.0.1", -1));
    BOOST_CHECK(TestSplitHost("[127.0.0.1]:7890", "127.0.0.1", 7890));
    BOOST_CHECK(TestSplitHost("::ffff:127.0.0.1", "::ffff:127.0.0.1", -1));
    BOOST_CHECK(
        TestSplitHost("[::ffff:127.0.0.1]:7890", "::ffff:127.0.0.1", 7890));
    BOOST_CHECK(TestSplitHost("[::]:7890", "::", 7890));
    BOOST_CHECK(TestSplitHost("::7890", "::7890", -1));
    BOOST_CHECK(TestSplitHost(":7890", "", 7890));
    BOOST_CHECK(TestSplitHost("[]:7890", "", 7890));
    BOOST_CHECK(TestSplitHost("", "", -1));
}

static bool TestParse(const std::string &src, const std::string &canon, CService *out = nullptr) {
    const CService addr = LookupNumeric(src, 65535);
    if (out) *out = addr;
    return canon == addr.ToString();
}

BOOST_AUTO_TEST_CASE(netbase_lookupnumeric) {
    BOOST_CHECK(TestParse("127.0.0.1", "127.0.0.1:65535"));
    BOOST_CHECK(TestParse("127.0.0.1:7890", "127.0.0.1:7890"));
    BOOST_CHECK(TestParse("::ffff:127.0.0.1", "127.0.0.1:65535"));
    BOOST_CHECK(TestParse("::", "[::]:65535"));
    BOOST_CHECK(TestParse("[::]:7890", "[::]:7890"));
    BOOST_CHECK(TestParse("[127.0.0.1]", "127.0.0.1:65535"));
    BOOST_CHECK(TestParse(":::", "[::]:0"));

    // verify that an internal address fails to resolve
    BOOST_CHECK(TestParse("[fd6b:88c0:8724:1:2:3:4:5]", "[::]:0"));
    // and that a one-off resolves correctly
    BOOST_CHECK(TestParse("[fd6c:88c0:8724:1:2:3:4:5]",
                          "[fd6c:88c0:8724:1:2:3:4:5]:65535"));
}

BOOST_AUTO_TEST_CASE(onioncat_test) {
    // values from
    // https://web.archive.org/web/20121122003543/http://www.cypherpunk.at/onioncat/wiki/OnionCat
    CNetAddr addr1(ResolveIP("5wyqrzbvrdsumnok.onion"));
    CNetAddr addr2(ResolveIP("FD87:D87E:EB43:edb1:8e4:3588:e546:35ca"));
    BOOST_CHECK(addr1 == addr2);
    BOOST_CHECK(addr1.IsTor());
    BOOST_CHECK(addr1.ToStringIP() == "5wyqrzbvrdsumnok.onion");
    BOOST_CHECK(addr1.IsRoutable());
}

BOOST_AUTO_TEST_CASE(embedded_test) {
    CNetAddr addr1(ResolveIP("1.2.3.4"));
    CNetAddr addr2(ResolveIP("::FFFF:0102:0304"));
    BOOST_CHECK(addr2.IsIPv4());
    BOOST_CHECK_EQUAL(addr1.ToString(), addr2.ToString());
}

BOOST_AUTO_TEST_CASE(subnet_test) {
    BOOST_CHECK(ResolveSubNet("1.2.3.0/24") == ResolveSubNet("1.2.3.0/255.255.255.0"));
    BOOST_CHECK(ResolveSubNet("1.2.3.0/24") != ResolveSubNet("1.2.4.0/255.255.255.0"));
    BOOST_CHECK(ResolveSubNet("1.2.3.0/24").Match(ResolveIP("1.2.3.4")));
    BOOST_CHECK(!ResolveSubNet("1.2.2.0/24").Match(ResolveIP("1.2.3.4")));
    BOOST_CHECK(ResolveSubNet("1.2.3.4").Match(ResolveIP("1.2.3.4")));
    BOOST_CHECK(ResolveSubNet("1.2.3.4/32").Match(ResolveIP("1.2.3.4")));
    BOOST_CHECK(!ResolveSubNet("1.2.3.4").Match(ResolveIP("5.6.7.8")));
    BOOST_CHECK(!ResolveSubNet("1.2.3.4/32").Match(ResolveIP("5.6.7.8")));
    BOOST_CHECK(
        ResolveSubNet("::ffff:127.0.0.1").Match(ResolveIP("127.0.0.1")));
    BOOST_CHECK(
        ResolveSubNet("1:2:3:4:5:6:7:8").Match(ResolveIP("1:2:3:4:5:6:7:8")));
    BOOST_CHECK(
        !ResolveSubNet("1:2:3:4:5:6:7:8").Match(ResolveIP("1:2:3:4:5:6:7:9")));
    BOOST_CHECK(ResolveSubNet("1:2:3:4:5:6:7:0/112")
                    .Match(ResolveIP("1:2:3:4:5:6:7:1234")));
    BOOST_CHECK(
        ResolveSubNet("192.168.0.1/24").Match(ResolveIP("192.168.0.2")));
    BOOST_CHECK(
        ResolveSubNet("192.168.0.20/29").Match(ResolveIP("192.168.0.18")));
    BOOST_CHECK(ResolveSubNet("1.2.2.1/24").Match(ResolveIP("1.2.2.4")));
    BOOST_CHECK(ResolveSubNet("1.2.2.110/31").Match(ResolveIP("1.2.2.111")));
    BOOST_CHECK(ResolveSubNet("1.2.2.20/26").Match(ResolveIP("1.2.2.63")));
    // All-Matching IPv6 Matches arbitrary IPv6
    BOOST_CHECK(ResolveSubNet("::/0").Match(ResolveIP("1:2:3:4:5:6:7:1234")));
    // But not `::` or `0.0.0.0` because they are considered invalid addresses
    BOOST_CHECK(!ResolveSubNet("::/0").Match(ResolveIP("::")));
    BOOST_CHECK(!ResolveSubNet("::/0").Match(ResolveIP("0.0.0.0")));
    // Addresses from one network (IPv4) don't belong to subnets of another network (IPv6)
    BOOST_CHECK(!ResolveSubNet("::/0").Match(ResolveIP("1.2.3.4")));
    // All-Matching IPv4 does not Match IPv6
    BOOST_CHECK(
        !ResolveSubNet("0.0.0.0/0").Match(ResolveIP("1:2:3:4:5:6:7:1234")));
    // Invalid subnets Match nothing (not even invalid addresses)
    BOOST_CHECK(!CSubNet().Match(ResolveIP("1.2.3.4")));
    BOOST_CHECK(!ResolveSubNet("").Match(ResolveIP("4.5.6.7")));
    BOOST_CHECK(!ResolveSubNet("bloop").Match(ResolveIP("0.0.0.0")));
    BOOST_CHECK(!ResolveSubNet("bloop").Match(ResolveIP("hab")));
    // Check valid/invalid
    BOOST_CHECK(ResolveSubNet("1.2.3.0/0").IsValid());
    BOOST_CHECK(!ResolveSubNet("1.2.3.0/-1").IsValid());
    BOOST_CHECK(ResolveSubNet("1.2.3.0/32").IsValid());
    BOOST_CHECK(!ResolveSubNet("1.2.3.0/33").IsValid());
    BOOST_CHECK(!ResolveSubNet("1.2.3.0/300").IsValid());
    BOOST_CHECK(ResolveSubNet("1:2:3:4:5:6:7:8/0").IsValid());
    BOOST_CHECK(ResolveSubNet("1:2:3:4:5:6:7:8/33").IsValid());
    BOOST_CHECK(!ResolveSubNet("1:2:3:4:5:6:7:8/-1").IsValid());
    BOOST_CHECK(ResolveSubNet("1:2:3:4:5:6:7:8/128").IsValid());
    BOOST_CHECK(!ResolveSubNet("1:2:3:4:5:6:7:8/129").IsValid());
    BOOST_CHECK(!ResolveSubNet("fuzzy").IsValid());

    // CNetAddr constructor test
    BOOST_CHECK(CSubNet(ResolveIP("127.0.0.1")).IsValid());
    BOOST_CHECK(CSubNet(ResolveIP("127.0.0.1")).Match(ResolveIP("127.0.0.1")));
    BOOST_CHECK(!CSubNet(ResolveIP("127.0.0.1")).Match(ResolveIP("127.0.0.2")));
    BOOST_CHECK(CSubNet(ResolveIP("127.0.0.1")).ToString() == "127.0.0.1/32");

    CSubNet subnet = CSubNet(ResolveIP("1.2.3.4"), 32);
    BOOST_CHECK_EQUAL(subnet.ToString(), "1.2.3.4/32");
    subnet = CSubNet(ResolveIP("1.2.3.4"), 8);
    BOOST_CHECK_EQUAL(subnet.ToString(), "1.0.0.0/8");
    subnet = CSubNet(ResolveIP("1.2.3.4"), 0);
    BOOST_CHECK_EQUAL(subnet.ToString(), "0.0.0.0/0");

    subnet = CSubNet(ResolveIP("1.2.3.4"), ResolveIP("255.255.255.255"));
    BOOST_CHECK_EQUAL(subnet.ToString(), "1.2.3.4/32");
    subnet = CSubNet(ResolveIP("1.2.3.4"), ResolveIP("255.0.0.0"));
    BOOST_CHECK_EQUAL(subnet.ToString(), "1.0.0.0/8");
    subnet = CSubNet(ResolveIP("1.2.3.4"), ResolveIP("0.0.0.0"));
    BOOST_CHECK_EQUAL(subnet.ToString(), "0.0.0.0/0");

    BOOST_CHECK(CSubNet(ResolveIP("1:2:3:4:5:6:7:8")).IsValid());
    BOOST_CHECK(CSubNet(ResolveIP("1:2:3:4:5:6:7:8"))
                    .Match(ResolveIP("1:2:3:4:5:6:7:8")));
    BOOST_CHECK(!CSubNet(ResolveIP("1:2:3:4:5:6:7:8"))
                     .Match(ResolveIP("1:2:3:4:5:6:7:9")));
    BOOST_CHECK(CSubNet(ResolveIP("1:2:3:4:5:6:7:8")).ToString() ==
                "1:2:3:4:5:6:7:8/128");
    // IPv4 address with IPv6 netmask or the other way around.
    BOOST_CHECK(!CSubNet(ResolveIP("1.1.1.1"), ResolveIP("ffff::")).IsValid());
    BOOST_CHECK(!CSubNet(ResolveIP("::1"), ResolveIP("255.0.0.0")).IsValid());
    // Can't subnet TOR (or any other non-IPv4 and non-IPv6 network).
    BOOST_CHECK(!CSubNet(ResolveIP("5wyqrzbvrdsumnok.onion"), ResolveIP("255.0.0.0")).IsValid());

    subnet = ResolveSubNet("1.2.3.4/255.255.255.255");
    BOOST_CHECK_EQUAL(subnet.ToString(), "1.2.3.4/32");
    subnet = ResolveSubNet("1.2.3.4/255.255.255.254");
    BOOST_CHECK_EQUAL(subnet.ToString(), "1.2.3.4/31");
    subnet = ResolveSubNet("1.2.3.4/255.255.255.252");
    BOOST_CHECK_EQUAL(subnet.ToString(), "1.2.3.4/30");
    subnet = ResolveSubNet("1.2.3.4/255.255.255.248");
    BOOST_CHECK_EQUAL(subnet.ToString(), "1.2.3.0/29");
    subnet = ResolveSubNet("1.2.3.4/255.255.255.240");
    BOOST_CHECK_EQUAL(subnet.ToString(), "1.2.3.0/28");
    subnet = ResolveSubNet("1.2.3.4/255.255.255.224");
    BOOST_CHECK_EQUAL(subnet.ToString(), "1.2.3.0/27");
    subnet = ResolveSubNet("1.2.3.4/255.255.255.192");
    BOOST_CHECK_EQUAL(subnet.ToString(), "1.2.3.0/26");
    subnet = ResolveSubNet("1.2.3.4/255.255.255.128");
    BOOST_CHECK_EQUAL(subnet.ToString(), "1.2.3.0/25");
    subnet = ResolveSubNet("1.2.3.4/255.255.255.0");
    BOOST_CHECK_EQUAL(subnet.ToString(), "1.2.3.0/24");
    subnet = ResolveSubNet("1.2.3.4/255.255.254.0");
    BOOST_CHECK_EQUAL(subnet.ToString(), "1.2.2.0/23");
    subnet = ResolveSubNet("1.2.3.4/255.255.252.0");
    BOOST_CHECK_EQUAL(subnet.ToString(), "1.2.0.0/22");
    subnet = ResolveSubNet("1.2.3.4/255.255.248.0");
    BOOST_CHECK_EQUAL(subnet.ToString(), "1.2.0.0/21");
    subnet = ResolveSubNet("1.2.3.4/255.255.240.0");
    BOOST_CHECK_EQUAL(subnet.ToString(), "1.2.0.0/20");
    subnet = ResolveSubNet("1.2.3.4/255.255.224.0");
    BOOST_CHECK_EQUAL(subnet.ToString(), "1.2.0.0/19");
    subnet = ResolveSubNet("1.2.3.4/255.255.192.0");
    BOOST_CHECK_EQUAL(subnet.ToString(), "1.2.0.0/18");
    subnet = ResolveSubNet("1.2.3.4/255.255.128.0");
    BOOST_CHECK_EQUAL(subnet.ToString(), "1.2.0.0/17");
    subnet = ResolveSubNet("1.2.3.4/255.255.0.0");
    BOOST_CHECK_EQUAL(subnet.ToString(), "1.2.0.0/16");
    subnet = ResolveSubNet("1.2.3.4/255.254.0.0");
    BOOST_CHECK_EQUAL(subnet.ToString(), "1.2.0.0/15");
    subnet = ResolveSubNet("1.2.3.4/255.252.0.0");
    BOOST_CHECK_EQUAL(subnet.ToString(), "1.0.0.0/14");
    subnet = ResolveSubNet("1.2.3.4/255.248.0.0");
    BOOST_CHECK_EQUAL(subnet.ToString(), "1.0.0.0/13");
    subnet = ResolveSubNet("1.2.3.4/255.240.0.0");
    BOOST_CHECK_EQUAL(subnet.ToString(), "1.0.0.0/12");
    subnet = ResolveSubNet("1.2.3.4/255.224.0.0");
    BOOST_CHECK_EQUAL(subnet.ToString(), "1.0.0.0/11");
    subnet = ResolveSubNet("1.2.3.4/255.192.0.0");
    BOOST_CHECK_EQUAL(subnet.ToString(), "1.0.0.0/10");
    subnet = ResolveSubNet("1.2.3.4/255.128.0.0");
    BOOST_CHECK_EQUAL(subnet.ToString(), "1.0.0.0/9");
    subnet = ResolveSubNet("1.2.3.4/255.0.0.0");
    BOOST_CHECK_EQUAL(subnet.ToString(), "1.0.0.0/8");
    subnet = ResolveSubNet("1.2.3.4/254.0.0.0");
    BOOST_CHECK_EQUAL(subnet.ToString(), "0.0.0.0/7");
    subnet = ResolveSubNet("1.2.3.4/252.0.0.0");
    BOOST_CHECK_EQUAL(subnet.ToString(), "0.0.0.0/6");
    subnet = ResolveSubNet("1.2.3.4/248.0.0.0");
    BOOST_CHECK_EQUAL(subnet.ToString(), "0.0.0.0/5");
    subnet = ResolveSubNet("1.2.3.4/240.0.0.0");
    BOOST_CHECK_EQUAL(subnet.ToString(), "0.0.0.0/4");
    subnet = ResolveSubNet("1.2.3.4/224.0.0.0");
    BOOST_CHECK_EQUAL(subnet.ToString(), "0.0.0.0/3");
    subnet = ResolveSubNet("1.2.3.4/192.0.0.0");
    BOOST_CHECK_EQUAL(subnet.ToString(), "0.0.0.0/2");
    subnet = ResolveSubNet("1.2.3.4/128.0.0.0");
    BOOST_CHECK_EQUAL(subnet.ToString(), "0.0.0.0/1");
    subnet = ResolveSubNet("1.2.3.4/0.0.0.0");
    BOOST_CHECK_EQUAL(subnet.ToString(), "0.0.0.0/0");

    subnet = ResolveSubNet(
        "1:2:3:4:5:6:7:8/ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff");
    BOOST_CHECK_EQUAL(subnet.ToString(), "1:2:3:4:5:6:7:8/128");
    subnet = ResolveSubNet(
        "1:2:3:4:5:6:7:8/ffff:0000:0000:0000:0000:0000:0000:0000");
    BOOST_CHECK_EQUAL(subnet.ToString(), "1::/16");
    subnet = ResolveSubNet(
        "1:2:3:4:5:6:7:8/0000:0000:0000:0000:0000:0000:0000:0000");
    BOOST_CHECK_EQUAL(subnet.ToString(), "::/0");
    // Invalid netmasks (with 1-bits after 0-bits)
    subnet = ResolveSubNet("1.2.3.4/255.255.232.0");
    BOOST_CHECK(!subnet.IsValid());
    subnet = ResolveSubNet("1.2.3.4/255.0.255.255");
    BOOST_CHECK(!subnet.IsValid());
    subnet = ResolveSubNet("1:2:3:4:5:6:7:8/ffff:ffff:ffff:fffe:ffff:ffff:ffff:ff0f");
    BOOST_CHECK(!subnet.IsValid());
}

BOOST_AUTO_TEST_CASE(netbase_getgroup) {
    // use /16
    std::vector<bool> asmap;
    typedef std::vector<uint8_t> Vec8;
    // Local -> !Routable()
    BOOST_CHECK(ResolveIP("127.0.0.1").GetGroup(asmap) == Vec8{0});
    // !Valid -> !Routable()
    BOOST_CHECK(ResolveIP("257.0.0.1").GetGroup(asmap) == Vec8{0});
    // RFC1918 -> !Routable()
    BOOST_CHECK(ResolveIP("10.0.0.1").GetGroup(asmap) == Vec8{0});
    // RFC3927 -> !Routable()
    BOOST_CHECK(ResolveIP("169.254.1.1").GetGroup(asmap) == Vec8{0});
    // IPv4
    BOOST_CHECK(ResolveIP("1.2.3.4").GetGroup(asmap) == Vec8({NET_IPV4, 1, 2}));
    // RFC6145
    BOOST_CHECK(ResolveIP("::FFFF:0:102:304").GetGroup(asmap) == Vec8({NET_IPV4, 1, 2}));
    // RFC6052
    BOOST_CHECK(ResolveIP("64:FF9B::102:304").GetGroup(asmap) == Vec8({NET_IPV4, 1, 2}));
    // RFC3964
    BOOST_CHECK(ResolveIP("2002:102:304:9999:9999:9999:9999:9999").GetGroup(asmap) == Vec8({NET_IPV4, 1, 2}));
    // RFC4380
    BOOST_CHECK(ResolveIP("2001:0:9999:9999:9999:9999:FEFD:FCFB").GetGroup(asmap) == Vec8({NET_IPV4, 1, 2}));
    // Tor
    BOOST_CHECK(ResolveIP("FD87:D87E:EB43:edb1:8e4:3588:e546:35ca").GetGroup(asmap) == Vec8({NET_ONION, 239}));
    // he.net
    BOOST_CHECK(ResolveIP("2001:470:abcd:9999:9999:9999:9999:9999").GetGroup(asmap) ==
                Vec8({NET_IPV6, 32, 1, 4, 112, 175}));
    // IPv6
    BOOST_CHECK(ResolveIP("2001:2001:9999:9999:9999:9999:9999:9999").GetGroup(asmap) == Vec8({NET_IPV6, 32, 1, 32, 1}));

    // baz.net sha256 hash:
    // 12929400eb4607c4ac075f087167e75286b179c693eb059a01774b864e8fe505
    Vec8 internal_group = {NET_INTERNAL, 0x12, 0x92, 0x94, 0x00, 0xeb,
                           0x46,         0x07, 0xc4, 0xac, 0x07};
    BOOST_CHECK(CreateInternal("baz.net").GetGroup(asmap) == internal_group);
}

BOOST_AUTO_TEST_CASE(netbase_parsenetwork) {
    BOOST_CHECK_EQUAL(ParseNetwork("ipv4"), NET_IPV4);
    BOOST_CHECK_EQUAL(ParseNetwork("ipv6"), NET_IPV6);
    BOOST_CHECK_EQUAL(ParseNetwork("onion"), NET_ONION);
    BOOST_CHECK_EQUAL(ParseNetwork("tor"), NET_ONION);

    BOOST_CHECK_EQUAL(ParseNetwork("IPv4"), NET_IPV4);
    BOOST_CHECK_EQUAL(ParseNetwork("IPv6"), NET_IPV6);
    BOOST_CHECK_EQUAL(ParseNetwork("ONION"), NET_ONION);
    BOOST_CHECK_EQUAL(ParseNetwork("TOR"), NET_ONION);

    BOOST_CHECK_EQUAL(ParseNetwork(":)"), NET_UNROUTABLE);
    BOOST_CHECK_EQUAL(ParseNetwork("tÖr"), NET_UNROUTABLE);
    BOOST_CHECK_EQUAL(ParseNetwork("\xfe\xff"), NET_UNROUTABLE);
    BOOST_CHECK_EQUAL(ParseNetwork(""), NET_UNROUTABLE);
}

BOOST_AUTO_TEST_CASE(netpermissions_test) {
    std::string error;
    NetWhitebindPermissions whitebindPermissions;
    NetWhitelistPermissions whitelistPermissions;

    // Detect invalid white bind
    BOOST_CHECK(
        !NetWhitebindPermissions::TryParse("", whitebindPermissions, error));
    BOOST_CHECK(error.find("Cannot resolve -whitebind address") !=
                std::string::npos);
    BOOST_CHECK(!NetWhitebindPermissions::TryParse(
        "127.0.0.1", whitebindPermissions, error));
    BOOST_CHECK(error.find("Need to specify a port with -whitebind") !=
                std::string::npos);
    BOOST_CHECK(
        !NetWhitebindPermissions::TryParse("", whitebindPermissions, error));

    // If no permission flags, assume backward compatibility
    BOOST_CHECK(NetWhitebindPermissions::TryParse("1.2.3.4:32",
                                                  whitebindPermissions, error));
    BOOST_CHECK(error.empty());
    BOOST_CHECK_EQUAL(whitebindPermissions.m_flags, PF_ISIMPLICIT);
    BOOST_CHECK(
        NetPermissions::HasFlag(whitebindPermissions.m_flags, PF_ISIMPLICIT));
    NetPermissions::ClearFlag(whitebindPermissions.m_flags, PF_ISIMPLICIT);
    BOOST_CHECK(
        !NetPermissions::HasFlag(whitebindPermissions.m_flags, PF_ISIMPLICIT));
    BOOST_CHECK_EQUAL(whitebindPermissions.m_flags, PF_NONE);
    NetPermissions::AddFlag(whitebindPermissions.m_flags, PF_ISIMPLICIT);
    BOOST_CHECK(
        NetPermissions::HasFlag(whitebindPermissions.m_flags, PF_ISIMPLICIT));

    // Can set one permission
    BOOST_CHECK(NetWhitebindPermissions::TryParse("bloom@1.2.3.4:32",
                                                  whitebindPermissions, error));
    BOOST_CHECK_EQUAL(whitebindPermissions.m_flags, PF_BLOOMFILTER);
    BOOST_CHECK(NetWhitebindPermissions::TryParse("@1.2.3.4:32",
                                                  whitebindPermissions, error));
    BOOST_CHECK_EQUAL(whitebindPermissions.m_flags, PF_NONE);

    // Happy path, can parse flags
    BOOST_CHECK(NetWhitebindPermissions::TryParse("bloom,forcerelay@1.2.3.4:32",
                                                  whitebindPermissions, error));
    // forcerelay should also activate the relay permission
    BOOST_CHECK_EQUAL(whitebindPermissions.m_flags,
                      PF_BLOOMFILTER | PF_FORCERELAY | PF_RELAY);
    BOOST_CHECK(NetWhitebindPermissions::TryParse(
        "bloom,relay,noban@1.2.3.4:32", whitebindPermissions, error));
    BOOST_CHECK_EQUAL(whitebindPermissions.m_flags,
                      PF_BLOOMFILTER | PF_RELAY | PF_NOBAN);
    BOOST_CHECK(NetWhitebindPermissions::TryParse(
        "bloom,forcerelay,noban@1.2.3.4:32", whitebindPermissions, error));
    BOOST_CHECK(NetWhitebindPermissions::TryParse("all@1.2.3.4:32",
                                                  whitebindPermissions, error));
    BOOST_CHECK_EQUAL(whitebindPermissions.m_flags, PF_ALL);

    // Allow dups
    BOOST_CHECK(NetWhitebindPermissions::TryParse(
        "bloom,relay,noban,noban@1.2.3.4:32", whitebindPermissions, error));
    BOOST_CHECK_EQUAL(whitebindPermissions.m_flags,
                      PF_BLOOMFILTER | PF_RELAY | PF_NOBAN);

    // Allow empty
    BOOST_CHECK(NetWhitebindPermissions::TryParse(
        "bloom,relay,,noban@1.2.3.4:32", whitebindPermissions, error));
    BOOST_CHECK_EQUAL(whitebindPermissions.m_flags,
                      PF_BLOOMFILTER | PF_RELAY | PF_NOBAN);
    BOOST_CHECK(NetWhitebindPermissions::TryParse(",@1.2.3.4:32",
                                                  whitebindPermissions, error));
    BOOST_CHECK_EQUAL(whitebindPermissions.m_flags, PF_NONE);
    BOOST_CHECK(NetWhitebindPermissions::TryParse(",,@1.2.3.4:32",
                                                  whitebindPermissions, error));
    BOOST_CHECK_EQUAL(whitebindPermissions.m_flags, PF_NONE);

    // Detect invalid flag
    BOOST_CHECK(!NetWhitebindPermissions::TryParse(
        "bloom,forcerelay,oopsie@1.2.3.4:32", whitebindPermissions, error));
    BOOST_CHECK(error.find("Invalid P2P permission") != std::string::npos);

    // Check whitelist error
    BOOST_CHECK(!NetWhitelistPermissions::TryParse(
        "bloom,forcerelay,noban@1.2.3.4:32", whitelistPermissions, error));
    BOOST_CHECK(error.find("Invalid netmask specified in -whitelist") !=
                std::string::npos);

    // Happy path for whitelist parsing
    BOOST_CHECK(NetWhitelistPermissions::TryParse("noban@1.2.3.4",
                                                  whitelistPermissions, error));
    BOOST_CHECK_EQUAL(whitelistPermissions.m_flags, PF_NOBAN);
    BOOST_CHECK(NetWhitelistPermissions::TryParse(
        "bloom,forcerelay,noban,relay@1.2.3.4/32", whitelistPermissions,
        error));
    BOOST_CHECK_EQUAL(whitelistPermissions.m_flags,
                      PF_BLOOMFILTER | PF_FORCERELAY | PF_NOBAN | PF_RELAY);
    BOOST_CHECK(error.empty());
    BOOST_CHECK_EQUAL(whitelistPermissions.m_subnet.ToString(), "1.2.3.4/32");
    BOOST_CHECK(NetWhitelistPermissions::TryParse(
        "bloom,forcerelay,noban,relay,mempool@1.2.3.4/32", whitelistPermissions,
        error));

    const auto strings = NetPermissions::ToStrings(PF_ALL);
    BOOST_CHECK_EQUAL(strings.size(), 6U);
    BOOST_CHECK(std::find(strings.begin(), strings.end(), "bloomfilter") !=
                strings.end());
    BOOST_CHECK(std::find(strings.begin(), strings.end(), "forcerelay") !=
                strings.end());
    BOOST_CHECK(std::find(strings.begin(), strings.end(), "relay") !=
                strings.end());
    BOOST_CHECK(std::find(strings.begin(), strings.end(), "noban") !=
                strings.end());
    BOOST_CHECK(std::find(strings.begin(), strings.end(), "mempool") !=
                strings.end());
    BOOST_CHECK(std::find(strings.begin(), strings.end(), "addr") != strings.end());
}

BOOST_AUTO_TEST_CASE(cservice_get_set_sockaddr_test) {
    // test CService::GetSockAddr() and SetSockAddr() for IPv4
    {
        CService srv;
        BOOST_CHECK(TestParse("12.34.56.78:1234", "12.34.56.78:1234", &srv));
        BOOST_CHECK(srv.IsIPv4());
        auto pair = srv.GetSockAddr();
        BOOST_CHECK(bool(pair));
        BOOST_CHECK_EQUAL(pair->second, sizeof(sockaddr_in));
        CNetAddr netaddr(bit_cast<sockaddr_in>(pair->first).sin_addr);
        auto addrBytes = netaddr.GetAddrBytes();
        std::vector<uint8_t> expectedAddrBytes { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xff, 0xff, 12, 34, 56, 78 };
        BOOST_CHECK_EQUAL_COLLECTIONS(addrBytes.begin(), addrBytes.end(), expectedAddrBytes.begin(), expectedAddrBytes.end());
        BOOST_CHECK_EQUAL(bit_cast<sockaddr_in>(pair->first).sin_port, ntohs(1234));

        CService srv2;
        srv2.SetSockAddr(pair->first);
        BOOST_CHECK(srv == srv2);
        BOOST_CHECK(srv2.IsIPv4());
    }

    // test CService::GetSockAddr() and SetSockAddr() for IPv6
    {
        CService srv;
        BOOST_CHECK(TestParse("[2001:19f0:5:3ebe:5400:3ff:fe43:da2]:1234", "[2001:19f0:5:3ebe:5400:3ff:fe43:da2]:1234", &srv));
        BOOST_CHECK(srv.IsIPv6());
        auto pair = srv.GetSockAddr();
        BOOST_CHECK(bool(pair));
        BOOST_CHECK_EQUAL(pair->second, sizeof(sockaddr_in6));
        CNetAddr netaddr(bit_cast<sockaddr_in6>(pair->first).sin6_addr);
        auto addrBytes = netaddr.GetAddrBytes();
        const std::vector<uint8_t> expectedAddrBytes { 0x20, 0x01, 0x19, 0xf0, 0x00, 0x05, 0x3e, 0xbe, 0x54, 0x00, 0x03, 0xff, 0xfe, 0x43, 0x0d, 0xa2 };
        BOOST_CHECK_EQUAL_COLLECTIONS(addrBytes.begin(), addrBytes.end(), expectedAddrBytes.begin(), expectedAddrBytes.end());
        BOOST_CHECK_EQUAL(bit_cast<sockaddr_in6>(pair->first).sin6_port, ntohs(1234));
        BOOST_CHECK_EQUAL(bit_cast<sockaddr_in6>(pair->first).sin6_scope_id, 0);

        CService srv2;
        srv2.SetSockAddr(pair->first);
        BOOST_CHECK(srv == srv2);
        BOOST_CHECK(srv2.IsIPv6());
    }
}

BOOST_AUTO_TEST_CASE(netbase_dont_resolve_strings_with_embedded_nul_characters) {
    CNetAddr addr;
    BOOST_CHECK(LookupHost(std::string("127.0.0.1", 9), addr, false));
    BOOST_CHECK(!LookupHost(std::string("127.0.0.1\0", 10), addr, false));
    BOOST_CHECK(!LookupHost(std::string("127.0.0.1\0example.com", 21), addr, false));
    BOOST_CHECK(!LookupHost(std::string("127.0.0.1\0example.com\0", 22), addr, false));
    CSubNet ret;
    BOOST_CHECK(LookupSubNet(std::string("1.2.3.0/24", 10), ret));
    BOOST_CHECK(!LookupSubNet(std::string("1.2.3.0/24\0", 11), ret));
    BOOST_CHECK(!LookupSubNet(std::string("1.2.3.0/24\0example.com", 22), ret));
    BOOST_CHECK(!LookupSubNet(std::string("1.2.3.0/24\0example.com\0", 23), ret));
    // We only do subnetting for IPv4 and IPv6
    BOOST_CHECK(!LookupSubNet(std::string("5wyqrzbvrdsumnok.onion", 22), ret));
    BOOST_CHECK(!LookupSubNet(std::string("5wyqrzbvrdsumnok.onion\0", 23), ret));
    BOOST_CHECK(!LookupSubNet(std::string("5wyqrzbvrdsumnok.onion\0example.com", 34), ret));
    BOOST_CHECK(!LookupSubNet(std::string("5wyqrzbvrdsumnok.onion\0example.com\0", 35), ret));
}

// Since CNetAddr (un)ser is tested separately in net_tests.cpp here we only
// try a few edge cases for port, service flags and time.

static const std::vector<CAddress> fixture_addresses({
    CAddress(
        CService(CNetAddr(in6addr_loopback), 0 /* port */),
        NODE_NONE,
        0x4966bc61U /* Fri Jan  9 02:54:25 UTC 2009 */
    ),
    CAddress(
        CService(CNetAddr(in6addr_loopback), 0x00f1 /* port */),
        NODE_NETWORK,
        0x83766279U /* Tue Nov 22 11:22:33 UTC 2039 */
    ),
    CAddress(
        CService(CNetAddr(in6addr_loopback), 0xf1f2 /* port */),
        static_cast<ServiceFlags>(NODE_NETWORK_LIMITED),
        0xffffffffU /* Sun Feb  7 06:28:15 UTC 2106 */
    )
});

// fixture_addresses should equal to this when serialized in V1 format.
// When this is unserialized from V1 format it should equal to fixture_addresses.
static constexpr const char *stream_addrv1_hex =
    "03" // number of entries

    "61bc6649"                         // time, Fri Jan  9 02:54:25 UTC 2009
    "0000000000000000"                 // service flags, NODE_NONE
    "00000000000000000000000000000001" // address, fixed 16 bytes (IPv4 embedded in IPv6)
    "0000"                             // port

    "79627683"                         // time, Tue Nov 22 11:22:33 UTC 2039
    "0100000000000000"                 // service flags, NODE_NETWORK
    "00000000000000000000000000000001" // address, fixed 16 bytes (IPv6)
    "00f1"                             // port

    "ffffffff"                         // time, Sun Feb  7 06:28:15 UTC 2106
    "0004000000000000"                 // service flags, NODE_NETWORK_LIMITED
    "00000000000000000000000000000001" // address, fixed 16 bytes (IPv6)
    "f1f2";                            // port

// fixture_addresses should equal to this when serialized in V2 format.
// When this is unserialized from V2 format it should equal to fixture_addresses.
static constexpr const char *stream_addrv2_hex =
    "03" // number of entries

    "61bc6649"                         // time, Fri Jan  9 02:54:25 UTC 2009
    "00"                               // service flags, COMPACTSIZE(NODE_NONE)
    "02"                               // network id, IPv6
    "10"                               // address length, COMPACTSIZE(16)
    "00000000000000000000000000000001" // address
    "0000"                             // port

    "79627683"                         // time, Tue Nov 22 11:22:33 UTC 2039
    "01"                               // service flags, COMPACTSIZE(NODE_NETWORK)
    "02"                               // network id, IPv6
    "10"                               // address length, COMPACTSIZE(16)
    "00000000000000000000000000000001" // address
    "00f1"                             // port

    "ffffffff"                         // time, Sun Feb  7 06:28:15 UTC 2106
    "fd0004"                           // service flags, COMPACTSIZE(NODE_NETWORK_LIMITED)
    "02"                               // network id, IPv6
    "10"                               // address length, COMPACTSIZE(16)
    "00000000000000000000000000000001" // address
    "f1f2";                            // port

BOOST_AUTO_TEST_CASE(caddress_serialize_v1) {
    CDataStream s(SER_NETWORK, PROTOCOL_VERSION);

    s << fixture_addresses;
    BOOST_CHECK_EQUAL(HexStr(s), stream_addrv1_hex);
}

BOOST_AUTO_TEST_CASE(caddress_unserialize_v1) {
    CDataStream s(ParseHex(stream_addrv1_hex), SER_NETWORK, PROTOCOL_VERSION);
    std::vector<CAddress> addresses_unserialized;

    s >> addresses_unserialized;
    BOOST_CHECK(fixture_addresses == addresses_unserialized);
}

BOOST_AUTO_TEST_CASE(caddress_serialize_v2) {
    CDataStream s(SER_NETWORK, PROTOCOL_VERSION | ADDRV2_FORMAT);

    s << fixture_addresses;
    BOOST_CHECK_EQUAL(HexStr(s), stream_addrv2_hex);
}

BOOST_AUTO_TEST_CASE(caddress_unserialize_v2) {
    CDataStream s(ParseHex(stream_addrv2_hex), SER_NETWORK, PROTOCOL_VERSION | ADDRV2_FORMAT);
    std::vector<CAddress> addresses_unserialized;

    s >> addresses_unserialized;
    BOOST_CHECK(fixture_addresses == addresses_unserialized);
}

BOOST_AUTO_TEST_SUITE_END()
