# Mocks

No external service or network listener is required. Public tests use direct
`http.Handler` requests and a blocking in-memory `net.Listener` to make HTTP
shutdown ordering deterministic.
