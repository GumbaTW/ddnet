# Restricting dummies on a server

Set the following in the server configuration:

```cfg
sv_max_clients_per_ip 1
```

This allows one normal client per IP address, including any retained
timeout-protected tee. A second local connection (a dummy) is rejected while
the first is connected. The default remains 4. The restriction applies to both
0.6 and 0.7 connections.

## Timeout recovery

When retained timeout-protected tees fill the per-IP allowance, the server can
admit **one extra recovery-only connection per IP**. At least one of those tees
must have timed out, and active connections must remain below the per-IP limit.

The extra connection starts as a spectator. It cannot spawn, chat, vote, change
teams, run other commands, or enable timeout protection. It can use `/timeout`
to reclaim a protected tee from the same IP using that tee's existing timeout
code. Sharing an IP alone never grants ownership. The client's automatic
`/timeout` and `/mc;timeout ...` messages continue to work.

Successful recovery transfers the connection to the retained tee and releases
the temporary slot. A wrong code cannot enable protection on the temporary
connection. If that connection times out or disconnects, it is removed instead
of retaining another tee. It is also disconnected after `sv_timeout_recovery`
seconds (default 60, configurable from 1 to 300). The deadline starts at network
admission, includes map loading, and cannot be extended by rejoining.

Retained normal tees continue to count toward `sv_max_clients_per_ip`.
Repeatedly timing out recovery connections therefore cannot accumulate protected
tees: an IP can occupy its configured allowance plus one temporary recovery
slot. Lowering the limit does not evict existing tees, but admits no new normal
clients while their number remains at or above the new limit.

If the server has not yet detected the old connection's timeout, the player must
wait for the normal `conn_timeout` period before reconnecting. Recovery still
requires a free server slot; it does not reserve slots or evict other players
when the server is full.

## Limits of IP-based restrictions

The server cannot reliably identify which connection is a dummy. This setting
also prevents two people behind the same public IP (for example, in one home)
from playing together. A player using different public IP addresses can bypass
it. A recovery-only connection cannot reclaim a tee from a different IP; a
normal connection with spare allowance can still use ordinary timeout recovery
after an IP change.
