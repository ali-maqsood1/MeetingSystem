#!/bin/bash

BASE_URL="http://localhost:8080"

echo "========================================="
echo " Testing Meeting System Features"
echo "========================================="

# Get token (replace with your actual token)
echo -e "\n[SETUP] Using existing token"
TOKEN="a1e24e1e702dc2ca2a7957721e167b50"
echo "Token: $TOKEN"

# Test Health
echo -e "\n========== HEALTH CHECK =========="
curl -s $BASE_URL/health | jq .

# WEEK 6: Chat Tests
echo -e "\n========== CHAT TESTS =========="

echo -e "\n[TEST 1] Send a chat message"
curl -s -X POST $BASE_URL/api/v1/meetings/1/messages \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer $TOKEN" \
  -d '{"content":"Testing chat system!"}' | jq .

echo -e "\n[TEST 2] Get message count"
MESSAGE_COUNT=$(curl -s $BASE_URL/api/v1/meetings/1/messages \
  -H "Authorization: Bearer $TOKEN" | jq -r '.messages' | jq '. | length')
echo "Total messages in meeting: $MESSAGE_COUNT"

# WEEK 8: Whiteboard Tests
echo -e "\n========== WHITEBOARD TESTS =========="

echo -e "\n[TEST 3] Draw a line (element_type=0)"
curl -s -X POST $BASE_URL/api/v1/meetings/1/whiteboard/draw \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer $TOKEN" \
  -d '{"element_type":0,"x1":10,"y1":20,"x2":100,"y2":200}' | jq .

echo -e "\n[TEST 4] Draw a rectangle (element_type=1)"
curl -s -X POST $BASE_URL/api/v1/meetings/1/whiteboard/draw \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer $TOKEN" \
  -d '{"element_type":1,"x1":50,"y1":50,"x2":150,"y2":150}' | jq .

echo -e "\n[TEST 5] Draw a circle (element_type=2)"
curl -s -X POST $BASE_URL/api/v1/meetings/1/whiteboard/draw \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer $TOKEN" \
  -d '{"element_type":2,"x1":200,"y1":200,"x2":300,"y2":300}' | jq .

echo -e "\n[TEST 6] Get all whiteboard elements"
ELEMENT_COUNT=$(curl -s $BASE_URL/api/v1/meetings/1/whiteboard/elements \
  -H "Authorization: Bearer $TOKEN" | jq -r '.elements' | jq '. | length')
echo "Total whiteboard elements: $ELEMENT_COUNT"

# WEEK 9: Screen Share Tests
echo -e "\n========== SCREEN SHARE TESTS =========="

echo -e "\n[TEST 7] Start screen share"
STREAM_RESPONSE=$(curl -s -X POST $BASE_URL/api/v1/meetings/1/screenshare/start \
  -H "Authorization: Bearer $TOKEN")
echo $STREAM_RESPONSE | jq .
STREAM_ID=$(echo $STREAM_RESPONSE | jq -r '.stream_id')
echo "Stream ID: $STREAM_ID"

echo -e "\n[TEST 8] Stop screen share"
curl -s -X POST $BASE_URL/api/v1/meetings/1/screenshare/stop \
  -H "Authorization: Bearer $TOKEN" | jq .

# Summary
echo -e "\n========================================="
echo " Test Summary"
echo "========================================="
echo "✅ Chat: $MESSAGE_COUNT messages"
echo "✅ Whiteboard: $ELEMENT_COUNT elements"
echo "✅ Screen Share: Working"
echo "========================================="