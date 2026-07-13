# Starter

`pathutil.py` supplies the system under test. The extracted model answer is a
single pytest/Hypothesis module that imports `normalize_path` from `pathutil`.
The validator adds this directory to Python's import path; candidates must not
copy or replace the supplied implementation.
