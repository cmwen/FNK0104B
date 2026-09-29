# SD file manager

Build with `pio run -e file-manager`, then upload with
`pio run -e file-manager -t upload`. The landscape touchscreen UI browses
directories, shows free and total card capacity, and opens common text files.
Other files open in a small read-only hexadecimal viewer. The browser caps each
folder at 96 entries and the text viewer reads 352 bytes per page.

All card access is read-only. The app never creates, deletes, or formats files.
