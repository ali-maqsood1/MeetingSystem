#!/bin/bash
# Generate self-signed SSL certificates for development

echo "🔐 Generating SSL certificates for HTTPS..."

# Create certs directory if it doesn't exist
mkdir -p certs

# Generate private key and certificate
openssl req -x509 -newkey rsa:4096 -keyout certs/key.pem -out certs/cert.pem -days 365 -nodes -subj "/CN=localhost"

echo ""
echo "✅ SSL certificates generated in ./certs/"
echo "   - cert.pem (certificate)"
echo "   - key.pem (private key)"
echo ""
echo "⚠️  These are self-signed certificates for development only!"
echo "   Your browser will show a security warning - click 'Advanced' and 'Proceed'"
echo ""
