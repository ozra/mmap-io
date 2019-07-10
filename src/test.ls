# Tests for mmap-io (duh!)
#
# Some lines snatched from Ben Noordhuis test-script of "node-mmap"

fs =        require "fs"
os =        require "os"
mmap =      require "../"
assert =    require "assert"
constants = require "constants"

say = (...args) -> console.log.apply console, args

say "mmap in test is", mmap

{PAGESIZE, PROT_READ, PROT_WRITE, MAP_SHARED} = mmap

try
    say "mmap.PAGESIZE = ", mmap.PAGESIZE, "tries to overwrite it with 47"
    mmap.PAGESIZE = 47
    say "now mmap.PAGESIZE should be the same:", mmap.PAGESIZE, "silently kept"
catch e
    say "Caught trying to modify the mmap-object. Does this ever happen?", e

# open self (this script)
fd = fs.open-sync(process.argv[1], 'r')
size = fs.fstat-sync(fd).size
say "file size", size

# full 6-arg call
buffer = mmap.map(size, PROT_READ, MAP_SHARED, fd, 0, mmap.MADV_SEQUENTIAL)
say "buflen 1 = ", buffer.length
assert.equal(buffer.length, size)

say "Give advise with 2 args"
mmap.advise buffer, mmap.MADV_NORMAL

say "Give advise with 4 args"
mmap.advise buffer, 0, mmap.PAGESIZE, mmap.MADV_NORMAL

# Read the data..
# *todo* this isn't really helpful as a test..
say "\n\nBuffer contents, read byte for byte backwards and see that nothing explodes:\n"
try
    out = ""
    for ix from (size - 1) to 0 by -1
        out += String.from-char-code(buffer[ix])

    # not implemented on Win32
    incore_stats = mmap.incore(buffer)
    assert.equal(incore_stats[0], 0)
    assert.equal(incore_stats[1], 2)

catch e
    if e.message != 'mincore() not implemented'
        assert false, "Shit happened while reading from buffer"

try
    say "read out of bounds test"
    assert.equal typeof( buffer[size + 47] ), "undefined"

catch e
    say "deliberate out of bounds, caught exception - does this thing happen?", e.code, 'err-obj = ', e


# Ok, I won't write a segfault catcher cause that would be evil, so this will simply be uncatchable.. /ORC
#try
#    say "Try to write to read buffer"
#    buffer[0] = 47
#catch e
#    say "caught deliberate segmentation fault", e.code, 'err-obj = ', e


# 5-arg call
buffer = mmap.map(size, PROT_READ, MAP_SHARED, fd, 0)
say "buflen test 5-arg map call = ", buffer.length
assert.equal(buffer.length, size)

# 4-arg call
buffer = mmap.map(size, PROT_READ, MAP_SHARED, fd)
say "buflen test 4-arg map call = ", buffer.length
assert.equal(buffer.length, size)

# Snatched from Ben Noordhuis test-script:
if os.type() != 'Windows_NT'
    # XXX: this will always fail on Windows, as it requires that offset be a
    # multiple of the dwAllocationGranularity, which is NOT the same as the
    # pagesize.  In addition, the offset+length can't exceed the file size.
    fd = fs.open-sync(process.argv[1], 'r')
    buffer = mmap.map(size, PROT_READ, MAP_SHARED, fd, PAGESIZE)
    say "buflen test 3 = ", buffer.length
    assert.equal(buffer.length, size);    # ...but this is according to spec

# non int param should throw exception
fd = fs.open-sync(process.argv[1], 'r')
try
    buffer = mmap.map("foo", PROT_READ, MAP_SHARED, fd, 0)
catch e
    say "Pass faulty arg - caught deliberate exception: #{e.message}"
    #assert.equal(e.code, constants.EINVAL)

# zero size should throw exception
fd = fs.open-sync(process.argv[1], 'r')
try
    buffer = mmap.map(0, PROT_READ, MAP_SHARED, fd, 0)
catch e
    say "Pass zero size - caught deliberate exception: #{e.message}"
    #assert.equal(e.code, constants.EINVAL)

# non-page size offset should throw exception
WRONG_PAGE_SIZE = PAGESIZE - 1
fd = fs.open-sync(process.argv[1], 'r')
try
    buffer = mmap.map(size, PROT_READ, MAP_SHARED, fd, WRONG_PAGE_SIZE)
catch e
    say "Pass wrong page-size as offset - caught deliberate exception: #{e.message}"
    #assert.equal(e.code, constants.EINVAL)


# faulty param to advise should throw exception
fd = fs.open-sync(process.argv[1], 'r')
try
    buffer = mmap.map(size, PROT_READ, MAP_SHARED, fd)
    mmap.advise buffer, "fuck off"
catch e
    say "Pass faulty arg to advise() - caught deliberate exception: #{e.message}"
    #assert.equal(e.code, constants.EINVAL)


# Write tests

say "Now for some write/read tests"

try
    say "Creates file"

    test-file = "./tmp-mmap-file"
    test-size = 47474

    fs.write-file-sync test-file, ""
    fs.truncate-sync test-file, test-size

    say "open write buffer"
    fd-w = fs.open-sync test-file, 'r+'
    say "fd-write = ", fd-w
    w-buffer = mmap.map(test-size, PROT_WRITE, MAP_SHARED, fd-w)
    fs.close-sync fd-w
    mmap.advise w-buffer, mmap.MADV_SEQUENTIAL

    say "open read bufer"
    fd-r = fs.open-sync test-file, 'r'
    r-buffer = mmap.map(test-size, PROT_READ, MAP_SHARED, fd-r)
    fs.close-sync fd-r
    mmap.advise r-buffer, mmap.MADV_SEQUENTIAL

    say "verify write and read"

    for i from 0 til test-size
        val = 32 + (i % 60)
        w-buffer[i] = val
        assert.equal r-buffer[i], val

    say "Write/read verification seemed to work out"

catch e
    say "Something fucked up in the write/read test::", e.message

try
    say "sync() tests x 4"

    say "1. Does explicit blocking sync to disk"
    mmap.sync w-buffer, 0, test-size, true, false

    say "2. Does explicit blocking sync without offset/length arguments"
    mmap.sync w-buffer, true, false

    say "3. Does explicit sync to disk without blocking/invalidate flags"
    mmap.sync w-buffer, 0, test-size

    say "4. Does explicit sync with no additional arguments"
    mmap.sync w-buffer

catch e
    say "Something fucked up for syncs::", e.message

try
    fs.unlink-sync test-file

catch e
    say "Failed to remove test-file", test-file

say "\nAll done"


process.exit 0
