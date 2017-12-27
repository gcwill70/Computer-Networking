# Multi-service Server
Concurrent HTTP web service and UDP ping service. Uses the select() funciton to serve multiple HTTP clients. If a UDP message is sent containing an integer in the header, the server will respond with the same message and will also increment the integer (see the Handout.pdf, page 11).
