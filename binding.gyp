{
    "targets": [{
        "target_name": "mmap-io",
        "sources": [ "src/mmap-io.cc" ],
        "include_dirs": [
            "<!(node -e \"require('nan')\")"
        ],
        "cflags_cc": [
            "-std=c++11"
        ]
    }]
}
