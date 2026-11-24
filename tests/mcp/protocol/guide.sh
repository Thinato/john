# The agent guide (GUIDE in src/mcp_main.cpp), sent as initialize's instructions
call init '"method":"initialize","params":{"protocolVersion":"2025-06-18","capabilities":{},"clientInfo":{"name":"t","version":"1"}}'
# store the written-for technique, never the instrument's equivalent
expect 'set the one in parentheses (roll), never the equivalent (turn)'
# how to help a singer
expect 'Find my range'
