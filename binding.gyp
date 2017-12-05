{
  'targets': [
    {
      'target_name': '<(module_name)',
      'sources': [
        'src/odbc.cpp',
        'src/odbc_connection.cpp',
        'src/odbc_statement.cpp',
        'src/odbc_result.cpp',
        'src/dynodbc.cpp',
        'src/chunked_buffer.cpp'
      ],
      'cflags': ['-Wall', '-Wextra', '-Wno-unused-parameter'],
      'include_dirs': [
        '<!(node -e "require(\'nan\')")'
      ],
      'defines': [
        'UNICODE'
      ],
      'conditions': [
        ['OS == "linux"', {
          'libraries': [
            '-lodbc'
          ],
          'cflags': [
            '-g'
          ]
        }],
        ['OS == "mac"', {
          'include_dirs': [
            '/usr/local/include'
          ],
          'libraries': [
            '-L/usr/local/lib',
            '-lodbc'
          ]
        }],
        ['OS == "win"', {
          'libraries': [
            '-lodbccp32.lib'
          ]
        }]
      ]
    },
    {
      'target_name': '<(module_name)_post_build',
      'type': 'none',
      'dependencies': ['<(module_name)'],
      'copies': [{
        'files': ['<(PRODUCT_DIR)/<(module_name).node'],
        'destination': '<(module_path)'
      }]
    }
  ]
}
