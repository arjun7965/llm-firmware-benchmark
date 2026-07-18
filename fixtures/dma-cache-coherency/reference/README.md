# Trusted Reference

The reference validates each DMA range before invoking any fixture API. It
cleans aligned cache lines before transmit, invalidates before receive launch
and after successful receive completion, and preserves receive state across a
nonterminal busy result.
