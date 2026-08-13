# Public Tests

The public suite covers zero-state initialization, invalid-call isolation, the
exact 28-cycle boundary, transactional commit failure, the full impulse
response, positive and negative tie rounding, overshoot saturation, history
movement, all four modeled MAC operands in order, and output agreement with an
independent seven-tap convolution across deterministic inputs. Invalid opaque
access and commit-time callbacks prove that invalid calls have no cost effect
and that history/output publication occurs strictly after a successful commit.
