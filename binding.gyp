{
    "targets": [{
        "target_name": "mmap-io",
        "sources": [ "src/mmap-io.cc" ],
        "include_dirs": [
            "<!(node -e \"require('nan')\")"
        ],
        "cflags_cc": [
            "-std=c++14"
        ],
        "conditions": [
            [ 'OS=="mac"',
                { "xcode_settings": {
                    'OTHER_CPLUSPLUSFLAGS' : ['-std=c++14','-stdlib=libc++'],
                    'OTHER_LDFLAGS': ['-stdlib=libc++'],
                    'MACOSX_DEPLOYMENT_TARGET': '10.8'
                }}
            ]
        ]
    }]
}
