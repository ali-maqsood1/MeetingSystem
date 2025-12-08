#!/bin/bash

# Test script for file upload
# Save as test_file_upload.sh and run: bash test_file_upload.sh

# 1. Login first
echo "=== Logging in ==="
LOGIN_RESPONSE=$(curl -s -X POST http://localhost:8080/api/v1/auth/login \
  -H "Content-Type: application/json" \
  -d '{"email":"ali1@gmail.com","password":"hi89"}')

echo "Login response: $LOGIN_RESPONSE"

TOKEN=$(echo $LOGIN_RESPONSE | grep -o '"session_token":"[^"]*"' | cut -d'"' -f4)
echo "Token: $TOKEN"

# 2. Create a test file
echo "Hello, this is a test file!" > test_upload.txt

# 3. Convert to base64
BASE64_DATA=$(base64 -w 0 test_upload.txt)
echo "Base64 data (first 50 chars): ${BASE64_DATA:0:50}..."

# 4. Upload file
echo -e "\n=== Uploading file ==="
UPLOAD_RESPONSE=$(curl -s -X POST http://localhost:8080/api/v1/meetings/4/files/upload \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer $TOKEN" \
  -d "{\"filename\":\"test_upload.txt\",\"data\":\"$BASE64_DATA\"}")

echo "Upload response: $UPLOAD_RESPONSE"

# 5. Get files list
echo -e "\n=== Getting files list ==="
FILES_RESPONSE=$(curl -s -X GET http://localhost:8080/api/v1/meetings/4/files \
  -H "Authorization: Bearer $TOKEN")

echo "Files response: $FILES_RESPONSE"

# Cleanup
rm test_upload.txt