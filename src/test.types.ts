import mmap from "./mmap-io"
import fs from "fs"

const fd = fs.openSync(__filename, "r+")

/**
 * Writing, for instance `4` instead of ie `2` (MADV_SEQUENTIAL), here, will
 * give an error already while typing (provided language-server is setup)
 * because of the specific types
 *
 * A problem arise with the traditional dirty technique of binary-or, widening
 * the type from literal to number-field
 */

mmap.map(
    100,
    (mmap.PROT_EXEC | mmap.PROT_READ) as 5,
    mmap.MAP_SHARED,
    fd,
    0,
    mmap.MADV_SEQUENTIAL
)
