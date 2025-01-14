# Troll

An all-in-one tool for dumping useful data from a certain video game

## Disclaimer

This tool is for personal modding for a certain video game only. Following these rules ensures:

- Continuous development and improvements
- Stable, long-term support
- Better features and updates
- Protection of our shared development environment

### 1. **Proprietary Restrictions**

All generated content must remain private. Sharing or distribution is prohibited.

### 2. **Distribution Ban**

Distribution any generated content results in legal action and bans.

### 3. **Legal Compliance**

Follow all laws and game terms.

### 4. **No Warranty**

Tool provided "as is". Users assume responsibility.

## **WARNING**

Use indicates acceptance. Non-compliance requires immediate cessation. Violations may end access and trigger legal action.

*By following these rules, you support our sustainable development.*

## Usage

- Automatic:

    Just run Troll.py and enjoy.

    ```log
    usage: Troll.py [-h] [-e EXT_SYMBOL_LIST] [-d WORK_DIR]

    Troll script

    options:
    -h, --help            show this help message and exit
    -e EXT_SYMBOL_LIST, --ext-symbol-list EXT_SYMBOL_LIST
                            External symbol list file path
    -d WORK_DIR, --work-dir WORK_DIR
                            Work directory
    ```

- Manual:

    ```cmd
    Troll.exe <build_directory> <include_header_file> <pe_file_path> <pdb_export_path> <pdb_name> [external_symbol_list]
    ```

    The last argument is optional, and it is a file of external symbols that you want to dump. The list should be separated by a newline.

    Maybe you can dump external symbols from a old version of the game.

## Troubleshooting

For a list of common issues and solutions, please check out [all issues](https://github.com/bdsmodding/Troll/issues?q=is%3Aissue). If not found, please [open one](https://github.com/bdsmodding/Troll/issues/new).

## Copyright

Copyright © 2025 What The Fuck You Call Us

This work is free. You can redistribute it and/or modify it under the
terms of the Do What The Fuck You Want To Public License, Version 2,
as published by Sam Hocevar. See the COPYING file for more details.
