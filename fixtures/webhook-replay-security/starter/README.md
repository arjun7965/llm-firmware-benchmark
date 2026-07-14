# Webhook replay security API contract

The answer's `handler.ts` exports
`createWebhookApp(pool, secrets, now?)`. `now`, when supplied, returns Unix
milliseconds. The handler must register `POST /webhook`; the two manifest-owned
answer files are compiled with this declaration and the public tests.
