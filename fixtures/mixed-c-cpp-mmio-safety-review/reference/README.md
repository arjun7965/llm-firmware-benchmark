# Trusted Reference

The reference reports each fixed-line defect in the supplied evidence and owns
one opaque transfer through a move-only C++17 RAII type. It is validation input,
not model-visible task output.

It enforces overlap only within each owner object. The caller supplies
per-handle exclusivity across live active owners, and two distinct active
objects moved by assignment therefore use distinct handles.
