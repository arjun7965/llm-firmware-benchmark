# Backend idempotency starter contract

`orders_api.ts` declares the Express request augmentation and the required
factory shape. The fixture supplies the `authenticate` middleware. It sets
`req.authenticatedUserId` only after validating the test identity header, so
the candidate must install it rather than interpreting that header itself.

The candidate application owns JSON parsing and must preserve the original
request bytes on `req.rawBody` before using `req.body`.

`compile-typescript.mjs` is validator-owned. It runs the profile-attested
TypeScript package through the pinned Node runtime without exposing a shell or
an unpinned global compiler.
