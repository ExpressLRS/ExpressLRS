import unittest

from binary_configurator import RegulatoryDomain, domain_number


class RegulatoryDomainConfigurationTests(unittest.TestCase):
    def test_persisted_domain_ids_are_stable(self):
        domains = ('au_915', 'fcc_915', 'eu_868', 'in_866', 'au_433',
                   'eu_433', 'us_433', 'us_433_wide', 'th_920', 'br_915')
        for expected_id, name in enumerate(domains):
            with self.subTest(domain=name):
                self.assertEqual(expected_id, domain_number(RegulatoryDomain(name)))

    def test_brazil_cli_value(self):
        self.assertEqual('br_915', str(RegulatoryDomain.br_915))


if __name__ == '__main__':
    unittest.main()
