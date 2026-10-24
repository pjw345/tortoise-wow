# SOAP runtime build compatibility

The generated SOAP bindings and vendored header use gSOAP 2.8.135. The old
vendored source was 2.7.15, and the prebuilt Linux archive referenced glibc
2.38 symbols (`__isoc23_strtol`, `strlcpy`, etc.) unavailable on Ubuntu 22.04.
Compiling the matching runtime source with the server toolchain removes that
binary dependency without changing the command interface or authentication.

Source: https://downloads.sourceforge.net/project/gsoap2/gsoap_2.8.135.zip
Archive SHA256: b2f32299559a2aaaa6b7e726f15403b5ee6837f838a0c4f9f5985224a5bd710e
File: gsoap-2.8/gsoap/stdsoap2.cpp, unmodified. Its companion header matches the
already vendored header byte for byte. Upstream licensing notices are retained.

Runtime remains off unless SOAP.Enabled=1. The requested VM setup binds to
127.0.0.1 inside the game container and publishes no administrator port.
Administrator authentication and world-thread command dispatch are unchanged.

Core DEBUG flags enable gSOAP wire logging. The listener explicitly disables
receive, send and diagnostic log files before accepting connections, so Basic
authentication headers and administrator commands are not written to them.
