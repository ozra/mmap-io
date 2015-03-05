mmap-io = require "./build/Release/mmap-io.node"

private-sync = mmap-io.sync_lib_private__
delete mmap-io.sync_lib_private__

# Take care of all the param juggling here instead of in C++-code, yikes.. /ORC
mmap-io.sync = (buf, offset, size, blocking-sync, invalidate-pages) !->
    switch typeof offset
    | "boolean"
        blocking-sync = offset
        invalidate-pages = size ? false
        offset = 0
        size = buf.length

    | "number"
        blocking-sync ?= false
        invalidate-pages ?= false

    | _
        offset = 0
        size = buf.length
        blocking-sync = false
        invalidate-pages = false

    private-sync(buf, offset, size, blocking-sync, invalidate-pages)

module.exports = mmap-io
