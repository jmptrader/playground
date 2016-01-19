Test exporting static lib functions linked to dll

- there is a static library link_test_lua
- it is linked to dynamic library link_test_dll, functions from link_test_lua are exported from link_test_dll using module definition file
- functions from link_test_lua can be used in link_test_main even though only link_test_dll is linked to link_test_main

