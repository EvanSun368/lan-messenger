# Group file routing trust boundary (verified source-lab implementation)

Goal: let a received group file appear in the matching group conversation,
including a group's first event being a file, without changing old direct offers.

## Security claims we do not make

Current LAN hello/sender IDs are self-declared. There is no device signature,
certificate binding, authenticated group invitation or encrypted transport. A
matching member ID list is consistency validation, NOT proof of a person's or
physical device's identity. This application is for a trusted LAN at this stage.
A hostile LAN peer can impersonate IDs; file routing does not solve that.

## Implemented rules

- The optional versioned group context carries a complete group identity, display
  name and unique fixed membership list, not just an arbitrary conversation ID.
- Both the receiver's local ID and request sender's ID must be members. Validate
  types, lengths, member count, uniqueness and supported context version.
- A known group requires the same membership set; preserve its local name and
  membership. Reject conflicts instead of silently changing group authority.
- An unknown group may be established on first use, following the existing group
  chat trust model. This is an unauthenticated LAN declaration (TOFU), not an
  authenticated invitation. Persist successfully before routing to the new group.
- No context remains a direct offer. Unsupported/malformed context is rejected,
  not silently downgraded to a direct conversation.
- Older peers may ignore the new optional field and continue direct-file behavior.
  Do not infer receiver group support from an ordinary old `{ok:true}` response.
- An offer never authorizes automatic saving/opening. Explicit receive/reject and
  actual transfer/hash/publication states retain their previous meanings.

## Required verification

Real three-node offers (not fabricated screenshots), first-file group creation,
known-group matching, member-order normalization, outsider/receiver exclusion,
unknown version, member conflict, failed database persistence, old direct offer,
old response and restart persistence. Re-run all transport/history/long-text tests.

## Endpoint discovery and acknowledgement limits

Group context intentionally contains no peer network addresses. Knowing a member
ID does not mean discovering its address: in a star topology, a recipient's reply
to an undiscovered third member stays queued until discovery/manual peer setup.
Tests must not count this member as having received the reply.

If a notification handler destroys a Node immediately after persistence, socket
buffers may close before the ACK is delivered. A matching ACK or an explicit
connection failure is valid; never manufacture success. Reopening the receiver
and retrying the same message ID must return an ACK without duplicate history.

## Latest result-receipt follow-up

Versioned results now let upgraded direct/group sender cards reflect actual
receiver completion/rejection; see file-result-receipts.md. Older peers and shared
legacy downloads remain unconfirmed. New screenshots were taken after sender
and receiver terminal states matched, and the stale rejection invitation hint was
fixed. Latest FULL/default/DPI/ASAN evidence is recorded in VALIDATION.md.
