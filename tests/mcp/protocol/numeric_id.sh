# Real clients send numeric ids: the reply must echo the number, not a string (src/mcp_main.cpp)
notify '"id":7,"method":"ping"'
if [ "$PHASE" = send ]; then CALLS=$((CALLS + 1)); else
  ID=numeric-id
  REPLY=$(grep -F '"id":7,' "$RES")
  expect '"id":7,"jsonrpc":"2.0","result":{}'
fi
