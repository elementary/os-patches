#!/usr/bin/python

import apt_pkg

import os
import unittest
import sys
sys.path.insert(0, "..")

from mock import patch

import softwareproperties.ppa
from  softwareproperties.ppa import (
    AddPPASigningKeyThread,
    mangle_ppa_shortcut,
    verify_keyid_is_v4,
    )


MOCK_PPA_INFO={
    "displayname": "PPA for Michael Vogt",
    "web_link": "https://launchpad.net/~mvo/+archive/ppa", 
    "signing_key_fingerprint": "019A25FED88F961763935D7F129196470EB12F05",
    "name": "ppa",
    'distribution_link': 'https://launchpad.net/api/1.0/ubuntu',
    'owner_link': 'https://launchpad.net/api/1.0/~mvo',
    'reference': '~mvo/ubuntu/ppa',
    }

MOCK_KEY="""
-----BEGIN PGP PUBLIC KEY BLOCK-----
Version: SKS 1.1.6
Comment: Hostname: keyserver.ubuntu.com

mI0ESXP67wEEAN2m3xWkAP0p1erHbJx1wYBCL6tLqWXESx1BmF0htLzdD9lfsUYiNs+Zgg3w
uU0PrQIcqZtyTESh514tw3KQ+OAK2I0a2XJR99lXPksiKoxaOOsr0pTVWDYuIlfV3yfmXvnK
FZSmaMjjKuqQbCwZe8Ev7yry9Gh9pM5Y87MbNT05ABEBAAG0HkxhdW5jaHBhZCBQUEEgZm9y
IE1pY2hhZWwgVm9ndIi2BBMBAgAgBQJJc/rvAhsDBgsJCAcDAgQVAggDBBYCAwECHgECF4AA
CgkQEpGWRw6xLwVofAP/YyU3YykXbr8p7wRp1EpFlDmtbPlFXp00gt4Cqlu2AWVOkwkVoMRQ
Ncb7wog2Z6u7KyUhD8pgC2FEL0+FQjyNemv7D0OYBG+6DLdjtRsv0CumLdWFmviU96j3OcwT
G2GkIC/eB2maTrV/vj7vlZ0Qe/T1NL6XLpr0A6Rg6JAtkFM=
=SMbJ
-----END PGP PUBLIC KEY BLOCK-----
"""

MOCK_SECOND_KEY="""
-----BEGIN PGP PUBLIC KEY BLOCK-----
Version: SKS 1.1.6
Comment: Hostname: keyserver.ubuntu.com

mI0ESX34EgEEAOTzplZO3TXmb9dRLu7kOuIEia21e4gwQ/RQe+LD7HdhikcETjf2Ruu0mn6S
sgPLL+duhKxmv6ZciLUgkk0qEDCZuR6BPxdgAIwqmQmFipcv6UTMQitRPUa9WlPU37Qg+joL
cTBUdamnVq+yJhLmnuO44UWAty85nNJzDd29gxqXABEBAAG0LUxhdW5jaHBhZCBQUEEgZm9y
INCU0LzQuNGC0YDQuNC5INCb0LXQtNC60L7Qsoi2BBMBAgAgBQJJffgSAhsDBgsJCAcDAgQV
AggDBBYCAwECHgECF4AACgkQFXlR/kAx0oeuSwQAuhhgWgeeG3F9XMYDqgJShzMSeQOLMKBq
6mNFEL1sDhRdbinf7rwuQFXDSSNCj8/PLa3DF/u09tAm6CTi10iwxxbXf16pTq21gxCA3/xS
fszv352yZpcN85MD5aozqv7qUCGOQ9Gey7JzgD7L4wMEjyRScVjx1chfLgyapdj822E=
=pdql
-----END PGP PUBLIC KEY BLOCK-----
"""

class LaunchpadPPATestCase(unittest.TestCase):
	
    @classmethod
    def setUpClass(cls):
        for k in apt_pkg.config.keys():
            apt_pkg.config.clear(k)
        apt_pkg.init()        

    @unittest.skipUnless(
        "TEST_ONLINE" in os.environ,
        "skipping online tests unless TEST_ONLINE environment variable is set")
    @unittest.skipUnless(
        sys.version_info[0] > 2,
        "pycurl doesn't raise SSL exceptions anymore it seems")
    def test_ppa_info_from_lp(self):
        # use correct data
        info = softwareproperties.ppa.get_ppa_info_from_lp("mvo", "ppa")
        self.assertNotEqual(info, {})
        self.assertEqual(info["name"], "ppa")
        # use empty CERT file
        softwareproperties.ppa.LAUNCHPAD_PPA_CERT = "/dev/null"
        with self.assertRaises(Exception):
            softwareproperties.ppa.get_ppa_info_from_lp("mvo", "ppa")

    def test_mangle_ppa_shortcut(self):
        self.assertEqual("~mvo/ubuntu/ppa", mangle_ppa_shortcut("ppa:mvo"))
        self.assertEqual(
            "~mvo/ubuntu/compiz", mangle_ppa_shortcut("ppa:mvo/compiz"))
        self.assertEqual(
            "~mvo/ubuntu-rtm/compiz",
            mangle_ppa_shortcut("ppa:mvo/ubuntu-rtm/compiz"))

    def test_mangle_ppa_shortcut_leading_slash(self):
        # Test for LP: #1426933
        self.assertEqual("~gottcode/ubuntu/gcppa",
                         mangle_ppa_shortcut("ppa:/gottcode/gcppa"))


class AddPPASigningKeyTestCase(unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        for k in apt_pkg.config.keys():
            apt_pkg.config.clear(k)
        apt_pkg.init()
        cls.trustedgpg = os.path.join(
            os.path.dirname(__file__), "aptroot", "etc", "apt", "trusted.gpg.d")
        try:
            os.makedirs(cls.trustedgpg)
        except:
            pass

    def setUp(self):
        self.t = AddPPASigningKeyThread("~mvo/ubuntu/ppa")

    @patch("softwareproperties.ppa.get_ppa_info_from_lp")
    @patch("softwareproperties.ppa.subprocess")
    def test_fingerprint_len_check(self, mock_subprocess, mock_get_ppa_info):
        """Test that short keyids (<160bit) are rejected""" 
        mock_ppa_info = MOCK_PPA_INFO.copy()
        mock_ppa_info["signing_key_fingerprint"] = "0EB12F05"
        mock_get_ppa_info.return_value = mock_ppa_info
        # do it
        res = self.t.add_ppa_signing_key("~mvo/ubuntu/ppa")
        self.assertFalse(res)
        self.assertFalse(mock_subprocess.Popen.called)
        self.assertFalse(mock_subprocess.call.called)

    @patch("softwareproperties.ppa.get_ppa_info_from_lp")
    @patch("softwareproperties.ppa.get_info_from_https")
    def test_add_ppa_signing_key_wrong_fingerprint(self, mock_https, mock_get_ppa_info):
        mock_get_ppa_info.return_value = MOCK_PPA_INFO
        mock_https.return_value = MOCK_SECOND_KEY
        res = self.t.add_ppa_signing_key("~mvo/ubuntu/ppa")
        self.assertFalse(res)

    @patch("softwareproperties.ppa.get_ppa_info_from_lp")
    @patch("softwareproperties.ppa.get_info_from_https")
    def test_add_ppa_signing_key_multiple_fingerprints(self, mock_https, mock_get_ppa_info):
        mock_get_ppa_info.return_value = MOCK_PPA_INFO
        mock_https.return_value = '\n'.join([MOCK_KEY, MOCK_SECOND_KEY])
        res = self.t.add_ppa_signing_key("~mvo/ubuntu/ppa")
        self.assertFalse(res)

    @patch("softwareproperties.ppa.get_ppa_info_from_lp")
    @patch("softwareproperties.ppa.get_info_from_https")
    @patch("apt_pkg.config")
    def test_add_ppa_signing_key_ok(self, mock_config, mock_https, mock_get_ppa_info):
        mock_get_ppa_info.return_value = MOCK_PPA_INFO
        mock_https.return_value = MOCK_KEY
        mock_config.find_dir.return_value = self.trustedgpg
        res = self.t.add_ppa_signing_key("~mvo/ubuntu/ppa")
        self.assertTrue(res)
    
    def test_verify_keyid_is_v4(self):
        keyid = "0EB12F05"
        self.assertFalse(verify_keyid_is_v4(keyid))


if __name__ == "__main__":
    unittest.main()
