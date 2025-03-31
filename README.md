# Custom Lightweight Discovery Protocol (CLDP)

## Overview
This project implements a custom discovery protocol using raw sockets in C. The protocol allows nodes in a network to announce their presence and query other nodes for system metadata.

## Supported Metadata Types
1. Hostname: Returns the node's hostname
2. System Time: Returns the current system time
3. CPU Load: Returns the system load averages (1, 5, and 15 minutes)

## Building the Project
```bash
# Compile both client and server
make all

# Compile only the server
make cldp_server

# Compile only the client
make cldp_client

# Clean build files
make clean