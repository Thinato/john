# JSON-RPC loop and initialize (src/mcp_main.cpp)
call init '"method":"initialize","params":{"protocolVersion":"2025-06-18","capabilities":{},"clientInfo":{"name":"t","version":"1"}}'
expect '"protocolVersion":"2025-06-18"'
notify '"method":"notifications/initialized"'
# modern clients probe server/discover first and fall back to initialize on -32601
call discover '"method":"server/discover"'
expect '"code":-32601'
call method-not-string '"method":5'
expect '"code":-32600'
call init-null-params '"method":"initialize","params":null'
expect '"protocolVersion":"2025-11-25"'
call ping '"method":"ping"'
expect '"id":"ping","jsonrpc":"2.0","result":{}'
