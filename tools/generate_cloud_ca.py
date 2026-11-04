"""Capture a root CA from a hostname-verified OS trust chain; never disable TLS."""
import socket
import ssl
import sys
from pathlib import Path
from cryptography import x509

host = sys.argv[1]
context = ssl.create_default_context()
with socket.create_connection((host, 443), timeout=15) as tcp:
    with context.wrap_socket(tcp, server_hostname=host) as tls:
        chain = tls.get_verified_chain()
root = x509.load_der_x509_certificate(chain[-1])
assert root.subject == root.issuer, 'Expected a self-signed trust anchor'
assert root.extensions.get_extension_for_class(x509.BasicConstraints).value.ca
pem = ssl.DER_cert_to_PEM_cert(chain[-1])
path = Path(__file__).resolve().parents[1] / 'include' / 'CloudRootCA.h'
path.write_text('#pragma once\n// Trust anchor from OS-verified TLS chain for ' + host + '\n'
                'static const char CLOUD_ROOT_CA[] = R"PEM(' + pem + ')PEM";\n')
print('Verified root:', root.subject.rfc4514_string())
