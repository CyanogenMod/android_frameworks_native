# bugreportz protocol

`bugreportz` is used to generate a zippped bugreport whose path is passed back to `adb`, using
the simple protocol defined below.

# Version 1.1
On version 1.1, in addition to the `OK` and `FAILURE` lines, `bugreportz -p` generates progress
lines in the following format:

- `PROGRESS:<progress>/<total>`, where `<progress>` is the current progress units out of a max of `<total>`.

## Version 1.0
On version 1.0, `bugreportz` does not generate any output on `stdout` until the bugreport is
finished, when it then prints one line with the result:

- `OK:<path_to_bugreport_file>` in case of success.
- `FAIL:<error message>` in case of failure.
